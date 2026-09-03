/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */
#ifndef NEARLINK_IPSHARE_CHANNEL_H
#define NEARLINK_IPSHARE_CHANNEL_H

#include <cstdint>
#include <functional>
#include <mutex>

#include "dtap.h"
#include "nearlink_ipshare_tun.h"
#include "qosm_trans_channel.h"

namespace OHOS::Nearlink {

class NearlinkIpShareChannel final {
public:
    static constexpr uint16_t IP_SHARE_PORT = 30200;
    using StateCallback = std::function<void(bool, int32_t)>;

    static NearlinkIpShareChannel &GetInstance();

    int32_t Initialize(const StateCallback &callback);
    void Deinitialize();
    int32_t CreateTun();
    int32_t Open(const uint8_t peer[6], uint8_t addressType);
    void Close();
    void SetPeer(const uint8_t peer[6], uint8_t addressType);
    void SetDhcpBound(bool bound);

    static bool IsIpSharePort(uint16_t port);
    static bool IsAcceptingPort(uint16_t port);
    static bool HandleChannelStatus(const QOSM_TransChannelRspParams_S *params);
    static int OnIpv4Received(DTAP_Data_Info_S *info, SDF_Buff_S *buffer);

private:
    NearlinkIpShareChannel() = default;
    bool ConsumeStatus(const QOSM_TransChannelRspParams_S *params);
    bool CanAccept(uint16_t port);
    int Receive(DTAP_Data_Info_S *info, SDF_Buff_S *buffer);
    int32_t Send(const uint8_t *data, uint16_t length);
    static bool ValidateIpv4(const uint8_t *data, uint16_t length, bool dhcpBound);

    std::mutex mutex_;
    NearlinkIpShareTun tun_;
    StateCallback callback_;
    uint8_t peer_[6] {};
    uint8_t addressType_ {0};
    uint16_t lcid_ {0};
    uint8_t tcid_ {0};
    bool initialized_ {false};
    bool channelPending_ {false};
    bool channelEstablished_ {false};
    bool dhcpBound_ {false};
};

}  // namespace OHOS::Nearlink
#endif  // NEARLINK_IPSHARE_CHANNEL_H
