/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */
#include "nearlink_ipshare_service.h"

#include <array>
#include <chrono>
#include <cstring>

#include "SleProperties.h"
#include "SleRemoteDeviceAdapter.h"
#include "ThreadUtil.h"
#include "iposl_profile.h"
#include "log.h"
#include "nearlink_ipshare_channel.h"
#include "nearlink_utils.h"
#include "raw_address.h"

namespace OHOS::Nearlink {
namespace {
constexpr int32_t IP_SHARE_OK = 0;
constexpr int32_t IP_SHARE_INVALID_ARGUMENT = -1;
constexpr int32_t IP_SHARE_INVALID_STATE = -2;
constexpr int32_t IP_SHARE_LINK_NOT_SECURE = -3;
constexpr int32_t IP_SHARE_PROFILE_FAILED = -4;
constexpr int32_t IP_SHARE_RESOURCE_FAILED = -5;
constexpr auto SUPPORT_WAIT = std::chrono::seconds(30);
}

NearlinkIpShareService &NearlinkIpShareService::GetInstance()
{
    static NearlinkIpShareService instance;
    return instance;
}

int32_t NearlinkIpShareService::Initialize()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) {
        HILOGI("[IpShare][Service] initialize skipped: already initialized");
        return IP_SHARE_OK;
    }
    HILOGI("[IpShare][Service] initialize started");
    IposlProfileCallbacks callbacks = {
        .onPeerSupported = &NearlinkIpShareService::OnPeerSupported,
        .onConfigured = &NearlinkIpShareService::OnConfigured,
    };
    int32_t profileRet = IposlProfileInit(&callbacks);
    if (profileRet != IPOSL_SUCCESS) {
        HILOGE("[IpShare][Service] initialize failed at IPoSL profile ret=%{public}d", profileRet);
        return IP_SHARE_PROFILE_FAILED;
    }
    int32_t channelRet = NearlinkIpShareChannel::GetInstance().Initialize([this](bool established, int32_t error) {
        DoInIpShareThread([this, established, error]() { HandleChannelState(established, error); });
    });
    if (channelRet != 0) {
        HILOGE("[IpShare][Service] initialize failed at IPv4 channel ret=%{public}d", channelRet);
        IposlProfileDeinit();
        return IP_SHARE_PROFILE_FAILED;
    }
    status_ = {};
    initialized_ = true;
    HILOGI("[IpShare][Service] initialize completed");
    return IP_SHARE_OK;
}

void NearlinkIpShareService::Shutdown()
{
    HILOGI("[IpShare][Service] shutdown started");
    StopNow();
    NearlinkIpShareChannel::GetInstance().Deinitialize();
    IposlProfileDeinit();
    std::lock_guard<std::mutex> lock(mutex_);
    initialized_ = false;
    observer_ = nullptr;
    HILOGI("[IpShare][Service] shutdown completed");
}

void NearlinkIpShareService::ResetForAdapterStop()
{
    HILOGI("[IpShare][Service] adapter reset started");
    StopNow();
    NearlinkIpShareChannel::GetInstance().Deinitialize();
    IposlProfileDeinit();
    std::lock_guard<std::mutex> lock(mutex_);
    initialized_ = false;
    HILOGI("[IpShare][Service] adapter reset completed");
}

int32_t NearlinkIpShareService::ValidateSecurePeer(const std::string &peerAddress, uint8_t peer[6],
    uint8_t &addressType) const
{
    if (!IsValidAddress(peerAddress) || peer == nullptr) {
        HILOGE("[IpShare][Service] secure-peer validation failed: invalid argument");
        return IP_SHARE_INVALID_ARGUMENT;
    }
    RawAddress address(peerAddress);
    auto *adapter = SleRemoteDeviceAdapter::GetInstance();
    if (adapter == nullptr) {
        HILOGE("[IpShare][Service] secure-peer validation failed: remote-device adapter unavailable");
        return IP_SHARE_LINK_NOT_SECURE;
    }
    if (!adapter->IsBondedFromLocal(address)) {
        HILOGE("[IpShare][Service] secure-peer validation failed: peer is not bonded");
        return IP_SHARE_LINK_NOT_SECURE;
    }
    if (!adapter->IsAcbConnected(address)) {
        HILOGE("[IpShare][Service] secure-peer validation failed: ACB is disconnected");
        return IP_SHARE_LINK_NOT_SECURE;
    }
    if (!adapter->IsAcbEncrypted(address)) {
        HILOGE("[IpShare][Service] secure-peer validation failed: ACB is not encrypted");
        return IP_SHARE_LINK_NOT_SECURE;
    }
    address.ConvertToUint8(peer, 6);
    addressType = adapter->GetPeerDeviceAddrType(address);
    return IP_SHARE_OK;
}

int32_t NearlinkIpShareService::IsPeerSupported(const std::string &peerAddress, bool &supported)
{
    HILOGI("[IpShare][Service] support probe started");
    uint8_t peer[6] = {};
    uint8_t addressType = 0;
    int32_t ret = ValidateSecurePeer(peerAddress, peer, addressType);
    if (ret != IP_SHARE_OK) {
        HILOGE("[IpShare][Service] support probe failed before discovery ret=%{public}d", ret);
        return ret;
    }
    std::unique_lock<std::mutex> lock(mutex_);
    if (!initialized_ || status_.role != NearlinkIpShareRole::NONE ||
        (status_.state != NearlinkIpShareState::IDLE && !probeInProgress_)) {
        HILOGE("[IpShare][Service] support probe rejected initialized=%{public}d role=%{public}d state=%{public}d",
            initialized_, static_cast<int32_t>(status_.role), static_cast<int32_t>(status_.state));
        return IP_SHARE_INVALID_STATE;
    }
    if (!probeInProgress_ && memcmp(supportedPeer_, peer, sizeof(supportedPeer_)) == 0 && peerSupported_) {
        supported = true;
        HILOGI("[IpShare][Service] support probe completed from cache supported=1");
        return IP_SHARE_OK;
    }
    if (!probeInProgress_) {
        probeInProgress_ = true;
        peerSupported_ = false;
        (void)memcpy(peer_, peer, sizeof(peer_));
        addressType_ = addressType;
        status_.state = NearlinkIpShareState::DISCOVERING;
        status_.peerAddress = peerAddress;
        HILOGI("[IpShare][Service] support probe dispatched to IPoSL thread addressType=%{public}u", addressType);
        auto peerCopy = std::array<uint8_t, 6> {};
        (void)memcpy(peerCopy.data(), peer, peerCopy.size());
        DoInIpShareThread([peerCopy, addressType]() {
            int32_t ret = IposlProfileProbePeer(peerCopy.data(), addressType);
            if (ret != IPOSL_SUCCESS) {
                HILOGE("[IpShare][Service] support probe failed to start IPoSL discovery ret=%{public}d", ret);
                NearlinkIpShareService::GetInstance().HandlePeerSupported(
                    peerCopy.data(), false, IP_SHARE_PROFILE_FAILED);
            }
        });
    }
    bool completed = probeCondition_.wait_for(lock, SUPPORT_WAIT, [this]() { return !probeInProgress_; });
    if (!completed) {
        HILOGE("[IpShare][Service] support probe timed out");
        probeInProgress_ = false;
        peerSupported_ = false;
        status_.state = NearlinkIpShareState::ERROR;
        status_.errorStage = "support-timeout";
        status_.errorCode = IP_SHARE_PROFILE_FAILED;
        NearlinkIpShareStatus status = status_;
        sptr<INearlinkIpShareObserver> observer = observer_;
        lock.unlock();
        DoInIpShareThread([]() { IposlProfileStopClient(); });
        NotifyStatus(status, observer);
        supported = false;
        return IP_SHARE_PROFILE_FAILED;
    }
    supported = completed && peerSupported_ && memcmp(supportedPeer_, peer, sizeof(supportedPeer_)) == 0;
    HILOGI("[IpShare][Service] support probe completed supported=%{public}d", supported);
    return IP_SHARE_OK;
}

int32_t NearlinkIpShareService::BeginRole(NearlinkIpShareRole role, const std::string &peerAddress,
    const uint8_t peer[6], uint8_t addressType)
{
    NearlinkIpShareStatus status;
    sptr<INearlinkIpShareObserver> observer;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_ || probeInProgress_ || status_.state != NearlinkIpShareState::IDLE ||
            status_.role != NearlinkIpShareRole::NONE) {
            HILOGE("[IpShare][Service] role start rejected initialized=%{public}d probe=%{public}d role=%{public}d "
                "state=%{public}d", initialized_, probeInProgress_, static_cast<int32_t>(status_.role),
                static_cast<int32_t>(status_.state));
            return IP_SHARE_INVALID_STATE;
        }
        status_ = {};
        status_.role = role;
        status_.state = NearlinkIpShareState::STARTING;
        status_.peerAddress = peerAddress;
        status_.ifaceName = "sleip0";
        (void)memcpy(peer_, peer, sizeof(peer_));
        addressType_ = addressType;
        status = status_;
        observer = observer_;
    }
    HILOGI("[IpShare][Service] role start accepted role=%{public}d addressType=%{public}u",
        static_cast<int32_t>(role), addressType);
    NotifyStatus(status, observer);
    return IP_SHARE_OK;
}

int32_t NearlinkIpShareService::StartGateway(const std::string &peerAddress)
{
    HILOGI("[IpShare][Service] gateway start started");
    uint8_t peer[6] = {};
    uint8_t addressType = 0;
    int32_t ret = ValidateSecurePeer(peerAddress, peer, addressType);
    if (ret != IP_SHARE_OK ||
        (ret = BeginRole(NearlinkIpShareRole::GATEWAY, peerAddress, peer, addressType)) != IP_SHARE_OK) {
        HILOGE("[IpShare][Service] gateway start rejected ret=%{public}d", ret);
        return ret;
    }
    NearlinkIpShareChannel::GetInstance().SetPeer(peer, addressType);
    auto peerCopy = std::array<uint8_t, 6> {};
    (void)memcpy(peerCopy.data(), peer, peerCopy.size());
    DoInIpShareThread([this, peerCopy, addressType]() {
        int32_t serverRet = IposlProfileStartServer(peerCopy.data(), addressType);
        if (serverRet != IPOSL_SUCCESS) {
            HILOGE("[IpShare][Service] gateway start failed at IPoSL server ret=%{public}d", serverRet);
            IposlProfileStopServer();
            SetState(NearlinkIpShareState::ERROR, "gateway-start", IP_SHARE_RESOURCE_FAILED);
            return;
        }
        int32_t tunRet = NearlinkIpShareChannel::GetInstance().CreateTun();
        if (tunRet != 0) {
            HILOGE("[IpShare][Service] gateway start failed at TUN ret=%{public}d", tunRet);
            IposlProfileStopServer();
            SetState(NearlinkIpShareState::ERROR, "gateway-start", IP_SHARE_RESOURCE_FAILED);
            return;
        }
        HILOGI("[IpShare][Service] gateway IPoSL server and TUN are ready");
        SetState(NearlinkIpShareState::IFACE_READY);
    });
    HILOGI("[IpShare][Service] gateway start dispatched to IPoSL thread");
    return IP_SHARE_OK;
}

int32_t NearlinkIpShareService::StartTerminal(const std::string &gatewayAddress)
{
    HILOGI("[IpShare][Service] terminal start started");
    uint8_t peer[6] = {};
    uint8_t addressType = 0;
    int32_t ret = ValidateSecurePeer(gatewayAddress, peer, addressType);
    if (ret != IP_SHARE_OK ||
        (ret = BeginRole(NearlinkIpShareRole::TERMINAL, gatewayAddress, peer, addressType)) != IP_SHARE_OK) {
        HILOGE("[IpShare][Service] terminal start rejected ret=%{public}d", ret);
        return ret;
    }
    NearlinkIpShareChannel::GetInstance().SetPeer(peer, addressType);
    SLE_Addr_S local = SleProperties::GetInstance().GetLocalSleAddress();
    auto peerCopy = std::array<uint8_t, 6> {};
    auto localCopy = std::array<uint8_t, 6> {};
    (void)memcpy(peerCopy.data(), peer, peerCopy.size());
    (void)memcpy(localCopy.data(), local.addr, localCopy.size());
    SetState(NearlinkIpShareState::DISCOVERING);
    DoInIpShareThread([peerCopy, localCopy, addressType]() {
        int32_t ret = IposlProfileStartTerminal(peerCopy.data(), addressType, localCopy.data());
        if (ret != IPOSL_SUCCESS) {
            HILOGE("[IpShare][Service] terminal start failed at IPoSL client ret=%{public}d", ret);
            NearlinkIpShareService::GetInstance().HandleConfigured(
                peerCopy.data(), false, IP_SHARE_PROFILE_FAILED);
        }
    });
    HILOGI("[IpShare][Service] terminal start dispatched; waiting for IPoSL callbacks");
    return IP_SHARE_OK;
}

int32_t NearlinkIpShareService::Stop()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_) {
            HILOGE("[IpShare][Service] stop rejected: not initialized");
            return IP_SHARE_INVALID_STATE;
        }
        if (status_.state == NearlinkIpShareState::IDLE) {
            HILOGI("[IpShare][Service] stop completed: already idle");
            return IP_SHARE_OK;
        }
        status_.state = NearlinkIpShareState::STOPPING;
    }
    HILOGI("[IpShare][Service] stop dispatched");
    DoInIpShareThread([this]() { StopNow(); });
    return IP_SHARE_OK;
}

void NearlinkIpShareService::StopNow()
{
    HILOGI("[IpShare][Service] stop cleanup started");
    IposlProfileStopClient();
    IposlProfileStopServer();
    NearlinkIpShareChannel::GetInstance().Close();
    NearlinkIpShareStatus status;
    sptr<INearlinkIpShareObserver> observer;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        status_ = {};
        probeInProgress_ = false;
        peerSupported_ = false;
        (void)memset(peer_, 0, sizeof(peer_));
        addressType_ = 0;
        status = status_;
        observer = observer_;
    }
    probeCondition_.notify_all();
    NotifyStatus(status, observer);
    HILOGI("[IpShare][Service] stop cleanup completed state=IDLE");
}

int32_t NearlinkIpShareService::GetStatus(NearlinkIpShareStatus &status) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_) {
        HILOGE("[IpShare][Service] status query rejected: not initialized");
        return IP_SHARE_INVALID_STATE;
    }
    status = status_;
    HILOGD("[IpShare][Service] status query role=%{public}d state=%{public}d error=%{public}d",
        static_cast<int32_t>(status.role), static_cast<int32_t>(status.state), status.errorCode);
    return IP_SHARE_OK;
}

int32_t NearlinkIpShareService::RegisterObserver(const sptr<INearlinkIpShareObserver> &observer)
{
    if (observer == nullptr) {
        HILOGE("[IpShare][Service] observer registration rejected: observer is null");
        return IP_SHARE_INVALID_ARGUMENT;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    observer_ = observer;
    HILOGI("[IpShare][Service] observer registered");
    return IP_SHARE_OK;
}

int32_t NearlinkIpShareService::UnregisterObserver()
{
    std::lock_guard<std::mutex> lock(mutex_);
    observer_ = nullptr;
    HILOGI("[IpShare][Service] observer unregistered");
    return IP_SHARE_OK;
}

void NearlinkIpShareService::OnPeerSupported(const uint8_t peer[6], bool supported, int32_t error)
{
    if (peer == nullptr) {
        HILOGE("[IpShare][Service] support callback ignored: peer is null");
        return;
    }
    auto peerCopy = std::array<uint8_t, 6> {};
    (void)memcpy(peerCopy.data(), peer, peerCopy.size());
    DoInIpShareThread([peerCopy, supported, error]() {
        GetInstance().HandlePeerSupported(peerCopy.data(), supported, error);
    });
    HILOGI("[IpShare][Service] support callback queued supported=%{public}d error=%{public}d", supported, error);
}

void NearlinkIpShareService::HandlePeerSupported(const uint8_t peer[6], bool supported, int32_t error)
{
    NearlinkIpShareStatus status;
    sptr<INearlinkIpShareObserver> observer;
    bool reportedSupported = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!probeInProgress_ || memcmp(peer_, peer, sizeof(peer_)) != 0) {
            HILOGW("[IpShare][Service] support callback ignored: stale or no probe");
            return;
        }
        (void)memcpy(supportedPeer_, peer, sizeof(supportedPeer_));
        peerSupported_ = supported && error == 0;
        reportedSupported = peerSupported_;
        probeInProgress_ = false;
        status_.state = error == 0 ? NearlinkIpShareState::IDLE : NearlinkIpShareState::ERROR;
        status_.errorStage = error == 0 ? "" : "support";
        status_.errorCode = error;
        status = status_;
        observer = observer_;
    }
    probeCondition_.notify_all();
    NotifyStatus(status, observer);
    HILOGI("[IpShare][Service] support callback handled supported=%{public}d error=%{public}d", reportedSupported,
        error);
}

void NearlinkIpShareService::OnConfigured(const uint8_t peer[6], bool opened, int32_t error)
{
    if (peer == nullptr) {
        HILOGE("[IpShare][Service] configuration callback ignored: peer is null");
        return;
    }
    auto peerCopy = std::array<uint8_t, 6> {};
    (void)memcpy(peerCopy.data(), peer, peerCopy.size());
    DoInIpShareThread([peerCopy, opened, error]() {
        GetInstance().HandleConfigured(peerCopy.data(), opened, error);
    });
    HILOGI("[IpShare][Service] configuration callback queued opened=%{public}d error=%{public}d", opened, error);
}

void NearlinkIpShareService::HandleConfigured(const uint8_t peer[6], bool opened, int32_t error)
{
    NearlinkIpShareRole role;
    std::array<uint8_t, 6> activePeer {};
    uint8_t addressType = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (memcmp(peer_, peer, sizeof(peer_)) != 0 || status_.state == NearlinkIpShareState::STOPPING) {
            HILOGW("[IpShare][Service] configuration callback ignored: stale peer or stopping");
            return;
        }
        role = status_.role;
        (void)memcpy(activePeer.data(), peer_, activePeer.size());
        addressType = addressType_;
    }
    if (error != 0) {
        HILOGE("[IpShare][Service] IPoSL configuration failed error=%{public}d", error);
        SetState(NearlinkIpShareState::ERROR, "iposl-config", error);
        return;
    }
    SetState(NearlinkIpShareState::CONFIGURING);
    if (!opened) {
        HILOGI("[IpShare][Service] IPoSL configuration completed; awaiting enable response");
        return;
    }
    if (role == NearlinkIpShareRole::GATEWAY) {
        HILOGI("[IpShare][Service] gateway enable confirmed by peer");
        return;
    }
    int32_t tunRet = NearlinkIpShareChannel::GetInstance().CreateTun();
    if (tunRet != 0) {
        HILOGE("[IpShare][Service] terminal enable failed at TUN ret=%{public}d", tunRet);
        SetState(NearlinkIpShareState::ERROR, "tun", IP_SHARE_RESOURCE_FAILED);
        return;
    }
    SetState(NearlinkIpShareState::IFACE_READY);
    int32_t channelRet = NearlinkIpShareChannel::GetInstance().Open(activePeer.data(), addressType);
    if (channelRet != 0) {
        HILOGE("[IpShare][Service] terminal enable failed at QoSM channel ret=%{public}d", channelRet);
        SetState(NearlinkIpShareState::ERROR, "channel", IP_SHARE_RESOURCE_FAILED);
        return;
    }
    HILOGI("[IpShare][Service] terminal QoSM channel request submitted");
}

void NearlinkIpShareService::HandleChannelState(bool established, int32_t error)
{
    NearlinkIpShareState state;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        state = status_.state;
    }
    if (state == NearlinkIpShareState::STOPPING || state == NearlinkIpShareState::IDLE) {
        HILOGW("[IpShare][Service] channel callback ignored state=%{public}d", static_cast<int32_t>(state));
        return;
    }
    if (established) {
        HILOGI("[IpShare][Service] QoSM channel established");
        SetState(NearlinkIpShareState::CHANNEL_READY);
    } else if (error != 0) {
        HILOGE("[IpShare][Service] QoSM channel failed error=%{public}d", error);
        SetState(NearlinkIpShareState::ERROR, "channel", error);
    }
}

void NearlinkIpShareService::SetState(NearlinkIpShareState state, const std::string &errorStage, int32_t error)
{
    NearlinkIpShareStatus status;
    sptr<INearlinkIpShareObserver> observer;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        status_.state = state;
        status_.errorStage = errorStage;
        status_.errorCode = error;
        status = status_;
        observer = observer_;
    }
    HILOGI("[IpShare][Service] state transition role=%{public}d state=%{public}d stage=%{public}s error=%{public}d",
        static_cast<int32_t>(status.role), static_cast<int32_t>(state), errorStage.c_str(), error);
    NotifyStatus(status, observer);
}

void NearlinkIpShareService::NotifyStatus(const NearlinkIpShareStatus &status,
    const sptr<INearlinkIpShareObserver> &observer) const
{
    if (observer != nullptr) {
        observer->OnStatusChanged(status);
    } else {
        HILOGD("[IpShare][Service] status observer not registered");
    }
}

}  // namespace OHOS::Nearlink
