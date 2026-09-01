/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */
#include "nearlink_ipshare_channel.h"

#include <cstring>

#include "log.h"
#include "iposl_profile.h"
#include "sdf_buff.h"

namespace OHOS::Nearlink {
namespace {
static_assert(DTAP_PI_IPV4 == 1, "IPoSL Demo requires IPv4 PI=0x01");
constexpr uint8_t IPV4_VERSION = 4;
constexpr uint8_t IPV4_MIN_IHL = 5;
constexpr uint8_t IPV4_PROTOCOL_UDP = 17;
constexpr uint16_t DHCP_SERVER_PORT = 67;
constexpr uint16_t DHCP_CLIENT_PORT = 68;
}

NearlinkIpShareChannel &NearlinkIpShareChannel::GetInstance()
{
    static NearlinkIpShareChannel instance;
    return instance;
}

int32_t NearlinkIpShareChannel::Initialize(const StateCallback &callback)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) {
        callback_ = callback;
        return 0;
    }
    if (DTAP_RegisterProtoRecvCbk(DTAP_PI_IPV4, &NearlinkIpShareChannel::OnIpv4Received) != 0) {
        return -1;
    }
    callback_ = callback;
    initialized_ = true;
    return 0;
}

void NearlinkIpShareChannel::Deinitialize()
{
    Close();
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) {
        (void)DTAP_UnregisterProtoRecvCbk(DTAP_PI_IPV4);
    }
    initialized_ = false;
    callback_ = nullptr;
}

int32_t NearlinkIpShareChannel::CreateTun()
{
    return tun_.Open([this](const uint8_t *data, uint16_t length) {
        if (Send(data, length) != 0) {
            HILOGW("[IpShare] drop outbound IPv4 packet");
        }
    });
}

void NearlinkIpShareChannel::SetPeer(const uint8_t peer[6], uint8_t addressType)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (peer != nullptr) {
        (void)memcpy(peer_, peer, sizeof(peer_));
    }
    addressType_ = addressType;
}

int32_t NearlinkIpShareChannel::Open(const uint8_t peer[6], uint8_t addressType)
{
    if (peer == nullptr) {
        return -1;
    }
    QOSM_TransChannelParams_S params = {};
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_ || channelPending_ || channelEstablished_) {
            return -1;
        }
        (void)memcpy(peer_, peer, sizeof(peer_));
        addressType_ = addressType;
        (void)memcpy(params.addr.addr, peer_, sizeof(peer_));
        params.addr.type = addressType_;
        params.linkMode = SLE_MODE_ACB;
        params.accessTransMode = ACCESS_TRANS_MODE_UNICAST;
        params.srcPort = IP_SHARE_PORT;
        params.dstPort = IP_SHARE_PORT;
        params.slqi = QOSM_TRANS_CHANNEL_SLQI_LOW;
        params.frameType = QOSM_SLE_RADIO_FRAME_TYPE_1;
        params.tcConf.mode = TRANSPORT_MODE_BASIC;
        channelPending_ = true;
    }
    if (QOSM_TransChannelCreate(&params) != 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        channelPending_ = false;
        return -1;
    }
    return 0;
}

void NearlinkIpShareChannel::Close()
{
    QOSM_TransChannelReleaseParams_S release = {};
    bool destroy = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (channelEstablished_) {
            (void)memcpy(release.addr.addr, peer_, sizeof(peer_));
            release.addr.type = addressType_;
            release.tcid = tcid_;
            destroy = true;
        }
        channelPending_ = false;
        channelEstablished_ = false;
        lcid_ = 0;
        tcid_ = 0;
        dhcpBound_ = false;
    }
    if (destroy) {
        (void)QOSM_TransChannelDestroy(&release);
    }
    tun_.Close();
}

void NearlinkIpShareChannel::SetDhcpBound(bool bound)
{
    std::lock_guard<std::mutex> lock(mutex_);
    dhcpBound_ = bound;
}

bool NearlinkIpShareChannel::IsIpSharePort(uint16_t port)
{
    return port == IP_SHARE_PORT;
}

bool NearlinkIpShareChannel::IsAcceptingPort(uint16_t port)
{
    return GetInstance().CanAccept(port);
}

bool NearlinkIpShareChannel::CanAccept(uint16_t port)
{
    std::lock_guard<std::mutex> lock(mutex_);
    const uint8_t emptyPeer[6] = {0};
    return IsIpSharePort(port) && initialized_ && tun_.IsOpen() &&
        memcmp(peer_, emptyPeer, sizeof(peer_)) != 0;
}

bool NearlinkIpShareChannel::HandleChannelStatus(const QOSM_TransChannelRspParams_S *params)
{
    return GetInstance().ConsumeStatus(params);
}

bool NearlinkIpShareChannel::ConsumeStatus(const QOSM_TransChannelRspParams_S *params)
{
    if (params == nullptr || (!IsIpSharePort(params->srcPort) && !IsIpSharePort(params->dstPort))) {
        return false;
    }
    StateCallback callback;
    bool established = false;
    int32_t error = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_ || memcmp(params->addr.addr, peer_, sizeof(peer_)) != 0 ||
            params->addr.type != addressType_) {
            return false;
        }
        callback = callback_;
        channelPending_ = false;
        if (params->status == QOSM_TRANS_CHANNEL_ESTABLISHED) {
            lcid_ = params->lcid;
            tcid_ = params->tcid;
            channelEstablished_ = true;
            established = true;
        } else if (params->status == QOSM_TRANS_CHANNEL_ESTABLISH_FAIL ||
            params->status == QOSM_TRANS_CHANNEL_RELEASE_FAIL) {
            channelEstablished_ = false;
            error = -1;
        } else if (params->status == QOSM_TRANS_CHANNEL_RELEASED) {
            channelEstablished_ = false;
            error = -1;
        }
    }
    if (callback) {
        callback(established, error);
    }
    return true;
}

int NearlinkIpShareChannel::OnIpv4Received(DTAP_Data_Info_S *info, SDF_Buff_S *buffer)
{
    return GetInstance().Receive(info, buffer);
}

int NearlinkIpShareChannel::Receive(DTAP_Data_Info_S *info, SDF_Buff_S *buffer)
{
    if (info == nullptr || buffer == nullptr || info->pi != DTAP_PI_IPV4) {
        return -1;
    }
    const uint8_t *data = SDF_DataOffset(buffer);
    uint32_t dataLen = SDF_DataLenGet(buffer);
    bool bound = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!channelEstablished_ || info->lcid != lcid_ || info->tcid != tcid_ || dataLen > UINT16_MAX) {
            return -1;
        }
        bound = dhcpBound_;
    }
    if (!ValidateIpv4(data, static_cast<uint16_t>(dataLen), bound)) {
        return -1;
    }
    return tun_.Write(data, static_cast<uint16_t>(dataLen));
}

int32_t NearlinkIpShareChannel::Send(const uint8_t *data, uint16_t length)
{
    uint16_t lcid = 0;
    uint8_t tcid = 0;
    bool bound = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!channelEstablished_) {
            return -1;
        }
        lcid = lcid_;
        tcid = tcid_;
        bound = dhcpBound_;
    }
    if (!ValidateIpv4(data, length, bound)) {
        return -1;
    }
    SDF_Buff_S *buffer = SDF_BuffNewWithReserve(length);
    if (buffer == nullptr) {
        return -1;
    }
    uint8_t *payload = SDF_BuffAppend(buffer, length);
    if (payload == nullptr) {
        SDF_BuffFree(buffer);
        return -1;
    }
    (void)memcpy(payload, data, length);
    DTAP_Data_S packet = {.pi = DTAP_PI_IPV4, .lcid = lcid, .tcid = tcid, .buff = buffer};
    if (DTAP_DataSend(&packet) != 0) {
        SDF_BuffFree(buffer);
        return -1;
    }
    return 0;
}

bool NearlinkIpShareChannel::ValidateIpv4(const uint8_t *data, uint16_t length, bool dhcpBound)
{
    if (data == nullptr || length < 20 || (data[0] >> 4) != IPV4_VERSION || (data[0] & 0x0F) < IPV4_MIN_IHL) {
        return false;
    }
    uint16_t headerLen = static_cast<uint16_t>((data[0] & 0x0F) * 4);
    uint16_t totalLen = static_cast<uint16_t>((static_cast<uint16_t>(data[2]) << 8) | data[3]);
    if (headerLen > length || totalLen != length || totalLen > IPOSL_MTU) {
        return false;
    }
    if (dhcpBound) {
        return true;
    }
    uint16_t fragment = static_cast<uint16_t>((static_cast<uint16_t>(data[6]) << 8) | data[7]);
    if ((fragment & 0x3FFFu) != 0) {
        return false;
    }
    if (data[9] != IPV4_PROTOCOL_UDP || length < headerLen + 8) {
        return false;
    }
    uint16_t sourcePort = static_cast<uint16_t>((static_cast<uint16_t>(data[headerLen]) << 8) |
        data[headerLen + 1]);
    uint16_t destinationPort = static_cast<uint16_t>((static_cast<uint16_t>(data[headerLen + 2]) << 8) |
        data[headerLen + 3]);
    return (sourcePort == DHCP_SERVER_PORT && destinationPort == DHCP_CLIENT_PORT) ||
        (sourcePort == DHCP_CLIENT_PORT && destinationPort == DHCP_SERVER_PORT);
}

}  // namespace OHOS::Nearlink
