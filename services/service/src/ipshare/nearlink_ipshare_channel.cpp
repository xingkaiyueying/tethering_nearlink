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
constexpr uint8_t IPV4_PROTOCOL_ICMP = 1;
constexpr uint8_t IPV4_PROTOCOL_UDP = 17;
constexpr uint16_t DHCP_SERVER_PORT = 67;
constexpr uint16_t DHCP_CLIENT_PORT = 68;
constexpr uint16_t UDP_HEADER_LENGTH = 8;
constexpr uint16_t DHCP_OPTIONS_OFFSET = 240;
constexpr uint16_t DHCP_MAGIC_COOKIE_OFFSET = 236;
constexpr uint8_t DHCP_BOOT_REPLY = 2;
constexpr uint8_t DHCP_OPTION_PAD = 0;
constexpr uint8_t DHCP_OPTION_MESSAGE_TYPE = 53;
constexpr uint8_t DHCP_OPTION_END = 255;
constexpr uint8_t DHCP_MESSAGE_ACK = 5;
constexpr uint8_t DHCP_MAGIC_COOKIE[] = {99, 130, 83, 99};

uint16_t ReadUint16(const uint8_t *data)
{
    return static_cast<uint16_t>((static_cast<uint16_t>(data[0]) << 8) | data[1]);
}

void LogIpv4Packet(const char *direction, const uint8_t *data, uint16_t length, bool bound,
    uint16_t lcid, uint8_t tcid)
{
    if (data == nullptr || length < 20) {
        HILOGE("[DHCP][IpShare][Packet] %{public}s invalid buffer length=%{public}u", direction, length);
        return;
    }
    uint16_t headerLen = static_cast<uint16_t>((data[0] & 0x0F) * 4);
    HILOGI("[DHCP][IpShare][Packet] %{public}s IPv4 protocol=%{public}u length=%{public}u "
        "src=%{public}u.%{public}u.%{public}u.%{public}u dst=%{public}u.%{public}u.%{public}u.%{public}u "
        "bound=%{public}d lcid=%{public}u tcid=%{public}u", direction, data[9], length,
        data[12], data[13], data[14], data[15], data[16], data[17], data[18], data[19], bound, lcid, tcid);
    if (data[9] == IPV4_PROTOCOL_ICMP && headerLen <= length && length - headerLen >= 8) {
        const uint8_t *icmp = data + headerLen;
        HILOGI("[DHCP][IpShare][ICMP] %{public}s type=%{public}u code=%{public}u id=%{public}u seq=%{public}u",
            direction, icmp[0], icmp[1], ReadUint16(icmp + 4), ReadUint16(icmp + 6));
    }
}
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
        HILOGI("[IpShare][Channel] initialize updated callback for active channel");
        return 0;
    }
    int32_t ret = DTAP_RegisterProtoRecvCbk(DTAP_PI_IPV4, &NearlinkIpShareChannel::OnIpv4Received);
    if (ret != 0) {
        HILOGE("[IpShare][Channel] initialize failed: DTAP IPv4 callback registration ret=%{public}d", ret);
        return -1;
    }
    callback_ = callback;
    initialized_ = true;
    HILOGI("[IpShare][Channel] initialize completed: DTAP IPv4 callback registered");
    return 0;
}

void NearlinkIpShareChannel::Deinitialize()
{
    HILOGI("[IpShare][Channel] deinitialize started");
    Close();
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) {
        (void)DTAP_UnregisterProtoRecvCbk(DTAP_PI_IPV4);
    }
    initialized_ = false;
    callback_ = nullptr;
    HILOGI("[IpShare][Channel] deinitialize completed");
}

int32_t NearlinkIpShareChannel::CreateTun()
{
    HILOGI("[IpShare][Channel] create TUN requested");
    int32_t ret = tun_.Open([this](const uint8_t *data, uint16_t length) {
        if (Send(data, length) != 0) {
            HILOGW("[IpShare][Channel] drop outbound IPv4 packet");
        }
    });
    if (ret != 0) {
        HILOGE("[IpShare][Channel] create TUN failed ret=%{public}d", ret);
        return ret;
    }
    HILOGI("[IpShare][Channel] create TUN completed");
    return 0;
}

void NearlinkIpShareChannel::SetPeer(const uint8_t peer[6], uint8_t addressType)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (peer != nullptr) {
        (void)memcpy(peer_, peer, sizeof(peer_));
    } else {
        HILOGE("[IpShare][Channel] peer update ignored: peer is null");
    }
    addressType_ = addressType;
    HILOGI("[IpShare][Channel] peer context updated addressType=%{public}u", addressType);
}

int32_t NearlinkIpShareChannel::Open(const uint8_t peer[6], uint8_t addressType)
{
    if (peer == nullptr) {
        HILOGE("[IpShare][Channel] open rejected: peer is null");
        return -1;
    }
    QOSM_TransChannelParams_S params = {};
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_ || channelPending_ || channelEstablished_) {
            HILOGE("[IpShare][Channel] open rejected initialized=%{public}d pending=%{public}d established=%{public}d",
                initialized_, channelPending_, channelEstablished_);
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
    HILOGI("[IpShare][Channel] QoSM create submitted port=%{public}u addressType=%{public}u", IP_SHARE_PORT,
        addressType);
    int32_t ret = QOSM_TransChannelCreate(&params);
    if (ret != 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        channelPending_ = false;
        HILOGE("[IpShare][Channel] QoSM create failed ret=%{public}d", ret);
        return -1;
    }
    HILOGI("[IpShare][Channel] QoSM create accepted; awaiting channel status");
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
        int32_t ret = QOSM_TransChannelDestroy(&release);
        if (ret != 0) {
            HILOGE("[IpShare][Channel] QoSM destroy failed tcid=%{public}u ret=%{public}d", release.tcid, ret);
        } else {
            HILOGI("[IpShare][Channel] QoSM destroy completed tcid=%{public}u", release.tcid);
        }
    }
    tun_.Close();
    HILOGI("[IpShare][Channel] channel and TUN closed");
}

void NearlinkIpShareChannel::SetDhcpBound(bool bound)
{
    std::lock_guard<std::mutex> lock(mutex_);
    dhcpBound_ = bound;
    HILOGI("[DHCP][IpShare][Channel] DHCP binding state=%{public}d", bound);
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
    if (established) {
        HILOGI("[IpShare][Channel] QoSM status established lcid=%{public}u tcid=%{public}u",
            params->lcid, params->tcid);
    } else if (error != 0) {
        HILOGE("[IpShare][Channel] QoSM status failed status=%{public}d lcid=%{public}u tcid=%{public}u",
            params->status, params->lcid, params->tcid);
    } else {
        HILOGI("[IpShare][Channel] QoSM status received status=%{public}d", params->status);
    }
    if (callback) {
        callback(established, error);
    } else {
        HILOGE("[IpShare][Channel] QoSM status dropped: state callback is null");
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
        HILOGE("[DHCP][IpShare][RX] packet rejected: invalid DTAP input");
        return -1;
    }
    const uint8_t *data = SDF_DataOffset(buffer);
    uint32_t dataLen = SDF_DataLenGet(buffer);
    bool bound = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!channelEstablished_ || info->lcid != lcid_ || info->tcid != tcid_ || dataLen > UINT16_MAX) {
            HILOGW("[DHCP][IpShare][RX] packet rejected: channel mismatch lcid=%{public}u tcid=%{public}u "
                "length=%{public}u", info->lcid, info->tcid, dataLen);
            return -1;
        }
        bound = dhcpBound_;
    }
    LogIpv4Packet("RX DTAP->TUN", data, static_cast<uint16_t>(dataLen), bound, info->lcid, info->tcid);
    if (!ValidateIpv4(data, static_cast<uint16_t>(dataLen), bound)) {
        HILOGW("[DHCP][IpShare][RX] packet rejected by IPv4 policy length=%{public}u dhcpBound=%{public}d",
            dataLen, bound);
        return -1;
    }
    bool dhcpAck = !bound && IsDhcpAck(data, static_cast<uint16_t>(dataLen));
    int32_t ret = tun_.Write(data, static_cast<uint16_t>(dataLen));
    if (ret != 0) {
        HILOGE("[DHCP][IpShare][RX] delivery to TUN failed ret=%{public}d length=%{public}u", ret, dataLen);
        return ret;
    }
    HILOGI("[DHCP][IpShare][RX] packet delivered to TUN length=%{public}u", dataLen);
    if (dhcpAck) {
        HILOGI("[DHCP][IpShare][RX] DHCP ACK delivered; enabling post-DHCP IPv4 traffic");
        SetDhcpBound(true);
    }
    return 0;
}

int32_t NearlinkIpShareChannel::Send(const uint8_t *data, uint16_t length)
{
    uint16_t lcid = 0;
    uint8_t tcid = 0;
    bool bound = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!channelEstablished_) {
            HILOGW("[DHCP][IpShare][TX] packet rejected: QoSM channel not established");
            return -1;
        }
        lcid = lcid_;
        tcid = tcid_;
        bound = dhcpBound_;
    }
    LogIpv4Packet("TX TUN->DTAP", data, length, bound, lcid, tcid);
    if (!ValidateIpv4(data, length, bound)) {
        HILOGW("[DHCP][IpShare][TX] packet rejected by IPv4 policy length=%{public}u dhcpBound=%{public}d",
            length, bound);
        return -1;
    }
    bool dhcpAck = !bound && IsDhcpAck(data, length);
    SDF_Buff_S *buffer = SDF_BuffNewWithReserve(length);
    if (buffer == nullptr) {
        HILOGE("[DHCP][IpShare][TX] packet failed: buffer allocation length=%{public}u", length);
        return -1;
    }
    uint8_t *payload = SDF_BuffAppend(buffer, length);
    if (payload == nullptr) {
        SDF_BuffFree(buffer);
        HILOGE("[DHCP][IpShare][TX] packet failed: buffer append length=%{public}u", length);
        return -1;
    }
    (void)memcpy(payload, data, length);
    DTAP_Data_S packet = {.pi = DTAP_PI_IPV4, .lcid = lcid, .tcid = tcid, .buff = buffer};
    int32_t ret = DTAP_DataSend(&packet);
    if (ret != 0) {
        SDF_BuffFree(buffer);
        HILOGE("[DHCP][IpShare][TX] DTAP send failed lcid=%{public}u tcid=%{public}u ret=%{public}d",
            lcid, tcid, ret);
        return -1;
    }
    HILOGI("[DHCP][IpShare][TX] packet accepted by DTAP length=%{public}u lcid=%{public}u tcid=%{public}u",
        length, lcid, tcid);
    if (dhcpAck) {
        HILOGI("[DHCP][IpShare][TX] DHCP ACK sent; enabling post-DHCP IPv4 traffic");
        SetDhcpBound(true);
    }
    return 0;
}

bool NearlinkIpShareChannel::IsDhcpAck(const uint8_t *data, uint16_t length)
{
    if (data == nullptr || length < 20 || (data[0] >> 4) != IPV4_VERSION) {
        return false;
    }
    uint16_t headerLen = static_cast<uint16_t>((data[0] & 0x0F) * 4);
    if (headerLen < IPV4_MIN_IHL * 4 || data[9] != IPV4_PROTOCOL_UDP ||
        length < headerLen + UDP_HEADER_LENGTH + DHCP_OPTIONS_OFFSET) {
        return false;
    }
    uint16_t sourcePort = static_cast<uint16_t>((static_cast<uint16_t>(data[headerLen]) << 8) |
        data[headerLen + 1]);
    uint16_t destinationPort = static_cast<uint16_t>((static_cast<uint16_t>(data[headerLen + 2]) << 8) |
        data[headerLen + 3]);
    if (sourcePort != DHCP_SERVER_PORT || destinationPort != DHCP_CLIENT_PORT) {
        return false;
    }
    uint16_t udpLength = static_cast<uint16_t>((static_cast<uint16_t>(data[headerLen + 4]) << 8) |
        data[headerLen + 5]);
    if (udpLength < UDP_HEADER_LENGTH + DHCP_OPTIONS_OFFSET || headerLen + udpLength > length) {
        return false;
    }
    const uint8_t *dhcp = data + headerLen + UDP_HEADER_LENGTH;
    uint16_t dhcpLength = static_cast<uint16_t>(udpLength - UDP_HEADER_LENGTH);
    if (dhcp[0] != DHCP_BOOT_REPLY ||
        memcmp(dhcp + DHCP_MAGIC_COOKIE_OFFSET, DHCP_MAGIC_COOKIE, sizeof(DHCP_MAGIC_COOKIE)) != 0) {
        return false;
    }
    uint16_t offset = DHCP_OPTIONS_OFFSET;
    while (offset < dhcpLength) {
        uint8_t option = dhcp[offset++];
        if (option == DHCP_OPTION_PAD) {
            continue;
        }
        if (option == DHCP_OPTION_END || offset >= dhcpLength) {
            return false;
        }
        uint8_t optionLength = dhcp[offset++];
        if (optionLength > dhcpLength - offset) {
            return false;
        }
        if (option == DHCP_OPTION_MESSAGE_TYPE) {
            return optionLength == 1 && dhcp[offset] == DHCP_MESSAGE_ACK;
        }
        offset = static_cast<uint16_t>(offset + optionLength);
    }
    return false;
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
