/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */
#ifndef NEARLINK_IPSHARE_SERVICE_H
#define NEARLINK_IPSHARE_SERVICE_H

#include <condition_variable>
#include <mutex>
#include <string>

#include "i_nearlink_ipshare_observer.h"
#include "nearlink_ipshare_status.h"

namespace OHOS::Nearlink {

class NearlinkIpShareService final {
public:
    static NearlinkIpShareService &GetInstance();

    int32_t Initialize();
    void Shutdown();
    void ResetForAdapterStop();
    int32_t IsPeerSupported(const std::string &peerAddress, bool &supported);
    int32_t StartGateway(const std::string &peerAddress);
    int32_t StartTerminal(const std::string &gatewayAddress);
    int32_t Stop();
    int32_t GetStatus(NearlinkIpShareStatus &status) const;
    int32_t RegisterObserver(const sptr<INearlinkIpShareObserver> &observer);
    int32_t UnregisterObserver();

private:
    NearlinkIpShareService() = default;
    static void OnPeerSupported(const uint8_t peer[6], bool supported, int32_t error);
    static void OnConfigured(const uint8_t peer[6], bool opened, int32_t error);
    void HandlePeerSupported(const uint8_t peer[6], bool supported, int32_t error);
    void HandleConfigured(const uint8_t peer[6], bool opened, int32_t error);
    void HandleChannelState(bool established, int32_t error);
    int32_t ValidateSecurePeer(const std::string &peerAddress, uint8_t peer[6], uint8_t &addressType) const;
    int32_t BeginRole(NearlinkIpShareRole role, const std::string &peerAddress,
        const uint8_t peer[6], uint8_t addressType);
    void SetState(NearlinkIpShareState state, const std::string &errorStage = "", int32_t error = 0);
    void NotifyStatus(const NearlinkIpShareStatus &status, const sptr<INearlinkIpShareObserver> &observer) const;
    void StopNow();

    mutable std::mutex mutex_;
    std::condition_variable probeCondition_;
    NearlinkIpShareStatus status_;
    sptr<INearlinkIpShareObserver> observer_;
    uint8_t peer_[6] {};
    uint8_t addressType_ {0};
    uint8_t supportedPeer_[6] {};
    bool initialized_ {false};
    bool probeInProgress_ {false};
    bool peerSupported_ {false};
};

}  // namespace OHOS::Nearlink
#endif  // NEARLINK_IPSHARE_SERVICE_H
