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

int32_t NearlinkIpShareChannel::SetPeer(const uint8_t peer[6], uint8_t addressType,
    bool gateway, const uint8_t *clientKey)
{
    std::lock_guard<std::mutex> lock(mutex_);
    // QoSM has no creation request ID. Do not reuse the binding until old work is drained.
    if (peer == nullptr || !initialized_ || channelPending_ || channelEstablished_ || releasing_ || active_) {
        return -1;
    }
    (void)memcpy(peer_, peer, sizeof(peer_));
    addressType_ = addressType;
    gateway_ = gateway;
    memcpy(clientKey_, clientKey == nullptr ? peer : clientKey, sizeof(clientKey_));
    dhcpDiscover_ = false;
    dhcpRequest_ = false;
    dhcpBound_ = false;
    boundIp_ = 0;
    active_ = true;
    ++generation_;
    return 0;
}

bool NearlinkIpShareChannel::IsCurrentGeneration(uint64_t generation)
{
    std::lock_guard<std::mutex> lock(mutex_);
    return active_ && generation == generation_;
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
        if (!initialized_ || !active_ || releasing_ || channelPending_ || channelEstablished_ ||
            memcmp(peer_, peer, sizeof(peer_)) != 0 || addressType_ != addressType) {
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
        if (active_) {
            active_ = false;
            ++generation_;
        }
        if (channelEstablished_ || releasing_) {
            (void)memcpy(release.addr.addr, peer_, sizeof(peer_));
            release.addr.type = addressType_;
            release.tcid = tcid_;
            destroy = true;
            releasing_ = true;
        }
        // Retain pending creation and release identity for late completion and repeated Stop.
        channelEstablished_ = false;
        dhcpBound_ = false;
        dhcpRequest_ = false;
        boundIp_ = 0;
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
    bool accept = IsIpSharePort(port) && initialized_ && active_ && !releasing_ &&
        !channelPending_ && !channelEstablished_ && tun_.IsOpen() &&
        memcmp(peer_, emptyPeer, sizeof(peer_)) != 0;
    if (accept) channelPending_ = true; // Passive creation has the same late-completion race as Open.
    return accept;
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
    bool destroy = false;
    int32_t error = 0;
    uint64_t generation = 0;
    QOSM_TransChannelReleaseParams_S release = {};
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (memcmp(params->addr.addr, peer_, sizeof(peer_)) != 0 || params->addr.type != addressType_) {
            return false;
        }
        if (params->status == QOSM_TRANS_CHANNEL_ESTABLISHED) {
            if (channelEstablished_ && (lcid_ != params->lcid || tcid_ != params->tcid)) {
                // An unrelated success must never replace the current channel.
                release.addr = params->addr;
                release.tcid = params->tcid;
                destroy = true;
            } else {
                channelPending_ = false;
                lcid_ = params->lcid;
                tcid_ = params->tcid;
                if (!initialized_ || !active_ || releasing_) {
                    releasing_ = true;
                    release.addr = params->addr;
                    release.tcid = tcid_;
                    destroy = true;
                } else {
                    channelEstablished_ = true;
                    established = true;
                    callback = callback_;
                }
            }
        } else if (params->status == QOSM_TRANS_CHANNEL_ESTABLISH_FAIL) {
            if (!channelPending_) return true;
            channelPending_ = false;
            if (active_) {
                callback = callback_;
                error = -1;
            }
        } else if (params->status == QOSM_TRANS_CHANNEL_RELEASED ||
            params->status == QOSM_TRANS_CHANNEL_RELEASE_FAIL) {
            if ((!channelEstablished_ && !releasing_) || params->lcid != lcid_ || params->tcid != tcid_) {
                return true;
            }
            // Keep release failures retryable; QoSM removes its record only on RELEASED.
            releasing_ = params->status == QOSM_TRANS_CHANNEL_RELEASE_FAIL;
            channelEstablished_ = false;
            dhcpBound_ = false;
            dhcpRequest_ = false;
            boundIp_ = 0;
            if (active_) {
                callback = callback_;
                error = params->status == QOSM_TRANS_CHANNEL_RELEASED ? 0 : -1;
            }
        }
        generation = generation_;
    }
    if (destroy) {
        int32_t ret = QOSM_TransChannelDestroy(&release);
        HILOGI("[IpShare][Channel] late/cancelled channel cleanup tcid=%{public}u ret=%{public}d",
            release.tcid, ret);
    }
    if (callback) callback(established, error, generation);
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
    uint64_t generation = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!active_ || !channelEstablished_ || info->lcid != lcid_ || info->tcid != tcid_ || dataLen > UINT16_MAX) {
            HILOGW("[DHCP][IpShare][RX] packet rejected: channel mismatch lcid=%{public}u tcid=%{public}u "
                "length=%{public}u", info->lcid, info->tcid, dataLen);
            return -1;
        }
        bound = DhcpBoundLocked();
        generation = generation_;
    }
    uint16_t length = static_cast<uint16_t>(dataLen);
    if (!ValidateIpv4(data, length, true)) {
        uint8_t version = data == nullptr || length == 0 ? 0 : data[0] >> 4;
        HILOGD("[DHCP][IpShare][RX] non-IPv4 payload ignored length=%{public}u version=%{public}u",
            length, version);
        return -1;
    }
    if (!AuthorizePacket(data, length, generation, true)) {
        HILOGW("[DHCP][IpShare][RX] packet rejected by IPv4 policy length=%{public}u dhcpBound=%{public}d",
            length, bound);
        return -1;
    }
    int32_t ret = tun_.Write(data, length);
    if (ret != 0) {
        HILOGE("[DHCP][IpShare][RX] delivery to TUN failed ret=%{public}d length=%{public}u", ret, length);
        return ret;
    }
    return 0;
}

int32_t NearlinkIpShareChannel::Send(const uint8_t *data, uint16_t length)
{
    uint16_t lcid = 0;
    uint8_t tcid = 0;
    bool bound = false;
    uint64_t generation = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!active_ || !channelEstablished_) {
            HILOGW("[DHCP][IpShare][TX] packet rejected: QoSM channel not established");
            return -1;
        }
        lcid = lcid_;
        tcid = tcid_;
        bound = DhcpBoundLocked();
        generation = generation_;
    }
    if (!ValidateIpv4(data, length, true)) {
        uint8_t version = data == nullptr || length == 0 ? 0 : data[0] >> 4;
        HILOGD("[DHCP][IpShare][TX] non-IPv4 payload ignored length=%{public}u version=%{public}u",
            length, version);
        return 0;
    }
    if (!AuthorizePacket(data, length, generation, false)) {
        HILOGW("[DHCP][IpShare][TX] packet rejected by IPv4 policy length=%{public}u dhcpBound=%{public}d",
            length, bound);
        return -1;
    }
    int32_t ret = IposlProfileSendIpv4(lcid, tcid, data, length);
    if (ret != 0) {
        HILOGE("[DHCP][IpShare][TX] DTAP send failed lcid=%{public}u tcid=%{public}u ret=%{public}d",
            lcid, tcid, ret);
        return -1;
    }
    return 0;
}

bool NearlinkIpShareChannel::DhcpBoundLocked()
{
    if (dhcpBound_ && std::chrono::steady_clock::now() >= leaseExpiry_) {
        dhcpBound_ = false;
        boundIp_ = 0;
    }
    return dhcpBound_;
}

bool NearlinkIpShareChannel::ObserveDhcp(const uint8_t *data, uint16_t length, uint64_t generation)
{
    if (!ValidateIpv4(data, length, false)) return false;
    uint16_t header = (data[0] & 0x0f) * 4;
    uint16_t udpLength = ReadUint16(data + header + 4);
    if (udpLength < UDP_HEADER_LENGTH + DHCP_OPTIONS_OFFSET || header + udpLength != length) return false;
    auto sumWords = [](const uint8_t *bytes, uint16_t size, uint32_t sum) {
        for (uint16_t i = 0; i < size; i += 2) {
            sum += uint32_t(bytes[i]) << 8;
            if (i + 1 < size) sum += bytes[i + 1];
        }
        while (sum >> 16) sum = (sum & 0xffff) + (sum >> 16);
        return sum;
    };
    if (sumWords(data, header, 0) != 0xffff) return false;
    if (ReadUint16(data + header + 6) != 0 &&
        sumWords(data + header, udpLength, sumWords(data + 12, 8, IPV4_PROTOCOL_UDP + udpLength)) != 0xffff) return false;
    const uint8_t *dhcp = data + header + UDP_HEADER_LENGTH;
    uint16_t end = udpLength - UDP_HEADER_LENGTH;
    bool request = ReadUint16(data + header) == DHCP_CLIENT_PORT;
    if (dhcp[0] != (request ? 1 : DHCP_BOOT_REPLY) || dhcp[1] != 1 || dhcp[2] != 6 ||
        memcmp(dhcp + DHCP_MAGIC_COOKIE_OFFSET, DHCP_MAGIC_COOKIE, sizeof(DHCP_MAGIC_COOKIE)) != 0) return false;
    auto read32 = [](const uint8_t *p) -> uint32_t {
        return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
    };
    auto unicast = [](uint32_t ip) { return ip != 0 && (ip >> 24) != 0 && (ip >> 24) != 127 &&
        (ip >> 24) < 224 && (ip & 0xff) != 255; };
    uint8_t message = 0;
    uint32_t requested = read32(dhcp + 12), server = 0, lease = 0;
    bool haveRequested = false, haveServer = false, haveLease = false, haveClient = false;
    bool ended = false;
    for (uint16_t offset = DHCP_OPTIONS_OFFSET; offset < end;) {
        uint8_t option = dhcp[offset++];
        if (option == DHCP_OPTION_PAD) continue;
        if (option == DHCP_OPTION_END) { ended = true; break; }
        if (offset == end) return false;
        uint8_t count = dhcp[offset++];
        if (count > end - offset) return false;
        if (option == DHCP_OPTION_MESSAGE_TYPE) {
            if (message != 0 || count != 1) return false;
            message = dhcp[offset];
        } else if (option == 50) {
            if (haveRequested || count != 4) return false;
            requested = read32(dhcp + offset);
            haveRequested = true;
        } else if (option == 54) {
            if (haveServer || count != 4) return false;
            server = read32(dhcp + offset);
            haveServer = true;
        } else if (option == 61) {
            if (haveClient || count != 7 || dhcp[offset] != 1 ||
                memcmp(dhcp + offset + 1, dhcp + 28, 6) != 0) return false;
            haveClient = true;
        } else if (option == 51) {
            if (haveLease || count != 4) return false;
            lease = read32(dhcp + offset);
            haveLease = true;
        }
        offset += count;
    }
    uint32_t relay = read32(dhcp + 24);
    // The platform DHCP server writes its gateway address into giaddr even on this
    // directly connected L3 TUN. Keep client requests relay-free, and accept that
    // server reply shape only when giaddr is the authenticated server identifier.
    if (!ended || dhcp[3] != 0 || (request ? relay != 0 : relay != 0 && relay != server)) return false;
    uint32_t source = read32(data + 12), destination = read32(data + 16);
    if (request) {
        if (source != 0 && (source != read32(dhcp + 12) || !unicast(source))) return false;
    } else {
        if (source != server || !unicast(server) ||
            (destination != 0xffffffffu && destination != read32(dhcp + 16) &&
                destination != read32(dhcp + 12))) return false;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (!active_ || !channelEstablished_ || generation != generation_) return false;
    if (memcmp(clientKey_, dhcp + 28, sizeof(clientKey_)) != 0) return false;
    if (request && source != 0 && (!DhcpBoundLocked() || source != boundIp_)) return false;
    auto now = std::chrono::steady_clock::now();
    uint32_t xid = read32(dhcp + 4);
    if (request && message == 1) {
        dhcpBound_ = false;
        boundIp_ = 0;
        dhcpRequest_ = false;
        dhcpDiscover_ = true;
        dhcpXid_ = xid;
        memcpy(dhcpKey_, dhcp + 28, sizeof(dhcpKey_));
        transactionExpiry_ = now + std::chrono::seconds(120);
        return true;
    }
    if (request && message == 3 && unicast(requested)) {
        // Permit INIT-REBOOT and renewal, but only for the explicitly authorized identity.
        memcpy(dhcpKey_, dhcp + 28, sizeof(dhcpKey_));
        dhcpXid_ = xid;
        requestedIp_ = requested;
        serverIp_ = server;
        dhcpRequest_ = true;
        transactionExpiry_ = now + std::chrono::seconds(120);
        return true;
    }
    if (request && (message == 4 || message == 7)) {
        if (!DhcpBoundLocked() || requested != boundIp_) return false;
        dhcpBound_ = false;
        boundIp_ = 0;
        dhcpRequest_ = false;
        dhcpDiscover_ = false;
        return true;
    }
    if (request || now >= transactionExpiry_ || xid != dhcpXid_ ||
        memcmp(dhcpKey_, dhcp + 28, sizeof(dhcpKey_)) != 0 || !unicast(server)) return false;
    if (message == 2) return dhcpDiscover_ && unicast(read32(dhcp + 16));
    if (!dhcpRequest_ || (serverIp_ != 0 && serverIp_ != server)) return false;
    if (message == 6) {
        dhcpBound_ = false;
        boundIp_ = 0;
        dhcpRequest_ = false;
        dhcpDiscover_ = false;
        return true;
    }
    if (message != DHCP_MESSAGE_ACK || read32(dhcp + 16) != requestedIp_ || lease == 0) return false;
    boundIp_ = requestedIp_;
    leaseExpiry_ = now + std::chrono::seconds(lease);
    dhcpBound_ = true;
    dhcpRequest_ = false;
    dhcpDiscover_ = false;
    HILOGI("[DHCP][IpShare] authorized REQUEST/ACK; IPv4 binding established");
    return true;
}

bool NearlinkIpShareChannel::AuthorizePacket(const uint8_t *data, uint16_t length,
    uint64_t generation, bool received)
{
    if (!ValidateIpv4(data, length, true)) return false;
    bool fromClient;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!active_ || !channelEstablished_ || generation != generation_) return false;
        fromClient = gateway_ == received;
    }
    if (ValidateIpv4(data, length, false)) {
        uint16_t header = (data[0] & 0x0f) * 4;
        if ((ReadUint16(data + header) == DHCP_CLIENT_PORT) != fromClient) return false;
        return ObserveDhcp(data, length, generation);
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (!active_ || !channelEstablished_ || generation != generation_ || !DhcpBoundLocked()) return false;
    // Gateway ingress must use the lease source; gateway egress must target it.
    // Public Internet reply sources remain valid. The terminal applies the mirror rule.
    const uint8_t *ip = data + (fromClient ? 12 : 16);
    uint32_t address = (uint32_t(ip[0]) << 24) | (uint32_t(ip[1]) << 16) |
        (uint32_t(ip[2]) << 8) | ip[3];
    return address == boundIp_;
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
