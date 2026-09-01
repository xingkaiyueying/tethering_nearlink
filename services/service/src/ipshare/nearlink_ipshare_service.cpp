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
        return IP_SHARE_OK;
    }
    IposlProfileCallbacks callbacks = {
        .onPeerSupported = &NearlinkIpShareService::OnPeerSupported,
        .onConfigured = &NearlinkIpShareService::OnConfigured,
    };
    if (IposlProfileInit(&callbacks) != IPOSL_SUCCESS ||
        NearlinkIpShareChannel::GetInstance().Initialize([this](bool established, int32_t error) {
            DoInIpShareThread([this, established, error]() { HandleChannelState(established, error); });
        }) != 0) {
        IposlProfileDeinit();
        return IP_SHARE_PROFILE_FAILED;
    }
    status_ = {};
    initialized_ = true;
    return IP_SHARE_OK;
}

void NearlinkIpShareService::Shutdown()
{
    StopNow();
    NearlinkIpShareChannel::GetInstance().Deinitialize();
    IposlProfileDeinit();
    std::lock_guard<std::mutex> lock(mutex_);
    initialized_ = false;
    observer_ = nullptr;
}

void NearlinkIpShareService::ResetForAdapterStop()
{
    StopNow();
    NearlinkIpShareChannel::GetInstance().Deinitialize();
    IposlProfileDeinit();
    std::lock_guard<std::mutex> lock(mutex_);
    initialized_ = false;
}

int32_t NearlinkIpShareService::ValidateSecurePeer(const std::string &peerAddress, uint8_t peer[6],
    uint8_t &addressType) const
{
    if (!IsValidAddress(peerAddress) || peer == nullptr) {
        return IP_SHARE_INVALID_ARGUMENT;
    }
    RawAddress address(peerAddress);
    auto *adapter = SleRemoteDeviceAdapter::GetInstance();
    if (adapter == nullptr || !adapter->IsBondedFromLocal(address) || !adapter->IsAcbConnected(address) ||
        !adapter->IsAcbEncrypted(address)) {
        return IP_SHARE_LINK_NOT_SECURE;
    }
    address.ConvertToUint8(peer, 6);
    addressType = adapter->GetPeerDeviceAddrType(address);
    return IP_SHARE_OK;
}

int32_t NearlinkIpShareService::IsPeerSupported(const std::string &peerAddress, bool &supported)
{
    uint8_t peer[6] = {};
    uint8_t addressType = 0;
    int32_t ret = ValidateSecurePeer(peerAddress, peer, addressType);
    if (ret != IP_SHARE_OK) {
        return ret;
    }
    std::unique_lock<std::mutex> lock(mutex_);
    if (!initialized_ || status_.role != NearlinkIpShareRole::NONE ||
        (status_.state != NearlinkIpShareState::IDLE && !probeInProgress_)) {
        return IP_SHARE_INVALID_STATE;
    }
    if (!probeInProgress_ && memcmp(supportedPeer_, peer, sizeof(supportedPeer_)) == 0 && peerSupported_) {
        supported = true;
        return IP_SHARE_OK;
    }
    if (!probeInProgress_) {
        probeInProgress_ = true;
        peerSupported_ = false;
        (void)memcpy(peer_, peer, sizeof(peer_));
        addressType_ = addressType;
        status_.state = NearlinkIpShareState::DISCOVERING;
        status_.peerAddress = peerAddress;
        auto peerCopy = std::array<uint8_t, 6> {};
        (void)memcpy(peerCopy.data(), peer, peerCopy.size());
        DoInIpShareThread([peerCopy, addressType]() {
            if (IposlProfileProbePeer(peerCopy.data(), addressType) != IPOSL_SUCCESS) {
                NearlinkIpShareService::GetInstance().HandlePeerSupported(
                    peerCopy.data(), false, IP_SHARE_PROFILE_FAILED);
            }
        });
    }
    bool completed = probeCondition_.wait_for(lock, SUPPORT_WAIT, [this]() { return !probeInProgress_; });
    if (!completed) {
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
    NotifyStatus(status, observer);
    return IP_SHARE_OK;
}

int32_t NearlinkIpShareService::StartGateway(const std::string &peerAddress)
{
    uint8_t peer[6] = {};
    uint8_t addressType = 0;
    int32_t ret = ValidateSecurePeer(peerAddress, peer, addressType);
    if (ret != IP_SHARE_OK ||
        (ret = BeginRole(NearlinkIpShareRole::GATEWAY, peerAddress, peer, addressType)) != IP_SHARE_OK) {
        return ret;
    }
    NearlinkIpShareChannel::GetInstance().SetPeer(peer, addressType);
    auto peerCopy = std::array<uint8_t, 6> {};
    (void)memcpy(peerCopy.data(), peer, peerCopy.size());
    DoInIpShareThread([this, peerCopy, addressType]() {
        if (IposlProfileStartServer(peerCopy.data(), addressType) != IPOSL_SUCCESS ||
            NearlinkIpShareChannel::GetInstance().CreateTun() != 0) {
            IposlProfileStopServer();
            SetState(NearlinkIpShareState::ERROR, "gateway-start", IP_SHARE_RESOURCE_FAILED);
            return;
        }
        SetState(NearlinkIpShareState::IFACE_READY);
    });
    return IP_SHARE_OK;
}

int32_t NearlinkIpShareService::StartTerminal(const std::string &gatewayAddress)
{
    uint8_t peer[6] = {};
    uint8_t addressType = 0;
    int32_t ret = ValidateSecurePeer(gatewayAddress, peer, addressType);
    if (ret != IP_SHARE_OK ||
        (ret = BeginRole(NearlinkIpShareRole::TERMINAL, gatewayAddress, peer, addressType)) != IP_SHARE_OK) {
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
        if (IposlProfileStartTerminal(peerCopy.data(), addressType, localCopy.data()) != IPOSL_SUCCESS) {
            NearlinkIpShareService::GetInstance().HandleConfigured(
                peerCopy.data(), false, IP_SHARE_PROFILE_FAILED);
        }
    });
    return IP_SHARE_OK;
}

int32_t NearlinkIpShareService::Stop()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!initialized_) {
            return IP_SHARE_INVALID_STATE;
        }
        if (status_.state == NearlinkIpShareState::IDLE) {
            return IP_SHARE_OK;
        }
        status_.state = NearlinkIpShareState::STOPPING;
    }
    DoInIpShareThread([this]() { StopNow(); });
    return IP_SHARE_OK;
}

void NearlinkIpShareService::StopNow()
{
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
}

int32_t NearlinkIpShareService::GetStatus(NearlinkIpShareStatus &status) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (!initialized_) {
        return IP_SHARE_INVALID_STATE;
    }
    status = status_;
    return IP_SHARE_OK;
}

int32_t NearlinkIpShareService::RegisterObserver(const sptr<INearlinkIpShareObserver> &observer)
{
    if (observer == nullptr) {
        return IP_SHARE_INVALID_ARGUMENT;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    observer_ = observer;
    return IP_SHARE_OK;
}

int32_t NearlinkIpShareService::UnregisterObserver()
{
    std::lock_guard<std::mutex> lock(mutex_);
    observer_ = nullptr;
    return IP_SHARE_OK;
}

void NearlinkIpShareService::OnPeerSupported(const uint8_t peer[6], bool supported, int32_t error)
{
    if (peer == nullptr) {
        return;
    }
    auto peerCopy = std::array<uint8_t, 6> {};
    (void)memcpy(peerCopy.data(), peer, peerCopy.size());
    DoInIpShareThread([peerCopy, supported, error]() {
        GetInstance().HandlePeerSupported(peerCopy.data(), supported, error);
    });
}

void NearlinkIpShareService::HandlePeerSupported(const uint8_t peer[6], bool supported, int32_t error)
{
    NearlinkIpShareStatus status;
    sptr<INearlinkIpShareObserver> observer;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!probeInProgress_ || memcmp(peer_, peer, sizeof(peer_)) != 0) {
            return;
        }
        (void)memcpy(supportedPeer_, peer, sizeof(supportedPeer_));
        peerSupported_ = supported && error == 0;
        probeInProgress_ = false;
        status_.state = error == 0 ? NearlinkIpShareState::IDLE : NearlinkIpShareState::ERROR;
        status_.errorStage = error == 0 ? "" : "support";
        status_.errorCode = error;
        status = status_;
        observer = observer_;
    }
    probeCondition_.notify_all();
    NotifyStatus(status, observer);
}

void NearlinkIpShareService::OnConfigured(const uint8_t peer[6], bool opened, int32_t error)
{
    if (peer == nullptr) {
        return;
    }
    auto peerCopy = std::array<uint8_t, 6> {};
    (void)memcpy(peerCopy.data(), peer, peerCopy.size());
    DoInIpShareThread([peerCopy, opened, error]() {
        GetInstance().HandleConfigured(peerCopy.data(), opened, error);
    });
}

void NearlinkIpShareService::HandleConfigured(const uint8_t peer[6], bool opened, int32_t error)
{
    NearlinkIpShareRole role;
    std::array<uint8_t, 6> activePeer {};
    uint8_t addressType = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (memcmp(peer_, peer, sizeof(peer_)) != 0 || status_.state == NearlinkIpShareState::STOPPING) {
            return;
        }
        role = status_.role;
        (void)memcpy(activePeer.data(), peer_, activePeer.size());
        addressType = addressType_;
    }
    if (error != 0) {
        SetState(NearlinkIpShareState::ERROR, "iposl-config", error);
        return;
    }
    SetState(NearlinkIpShareState::CONFIGURING);
    if (!opened) {
        return;
    }
    if (role == NearlinkIpShareRole::GATEWAY) {
        return;
    }
    if (NearlinkIpShareChannel::GetInstance().CreateTun() != 0) {
        SetState(NearlinkIpShareState::ERROR, "tun", IP_SHARE_RESOURCE_FAILED);
        return;
    }
    SetState(NearlinkIpShareState::IFACE_READY);
    if (NearlinkIpShareChannel::GetInstance().Open(activePeer.data(), addressType) != 0) {
        SetState(NearlinkIpShareState::ERROR, "channel", IP_SHARE_RESOURCE_FAILED);
    }
}

void NearlinkIpShareService::HandleChannelState(bool established, int32_t error)
{
    NearlinkIpShareState state;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        state = status_.state;
    }
    if (state == NearlinkIpShareState::STOPPING || state == NearlinkIpShareState::IDLE) {
        return;
    }
    if (established) {
        SetState(NearlinkIpShareState::CHANNEL_READY);
    } else if (error != 0) {
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
    NotifyStatus(status, observer);
}

void NearlinkIpShareService::NotifyStatus(const NearlinkIpShareStatus &status,
    const sptr<INearlinkIpShareObserver> &observer) const
{
    if (observer != nullptr) {
        observer->OnStatusChanged(status);
    }
}

}  // namespace OHOS::Nearlink
