/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */
#include "nearlink_ipshare_client.h"

#include <cstdio>
#include <new>

#include "i_nearlink_ipshare.h"
#include "nearlink_errorcode.h"
#include "nearlink_host.h"
#include "nearlink_ipshare_client_c.h"
#include "nearlink_ipshare_observer_stub.h"
#include "nearlink_sa_manager.h"
#include "nearlink_utils.h"

namespace OHOS::Nearlink {

namespace {
class ClientObserverStub final : public NearlinkIpShareObserverStub {
public:
    explicit ClientObserverStub(const std::shared_ptr<NearlinkIpShareObserver> &observer) : observer_(observer) {}

    void OnStatusChanged(const NearlinkIpShareStatus &status) override
    {
        auto observer = observer_.lock();
        if (observer != nullptr) {
            observer->OnStatusChanged(status);
        }
    }

private:
    std::weak_ptr<NearlinkIpShareObserver> observer_;
};
}

struct NearlinkIpShareClient::impl {
    impl()
    {
        auto info = std::make_shared<NearlinkRegisterInfo>(PROFILE_IPSHARE_SERVER);
        profileRegisterId = NearlinkSaManager::GetInstance().RegisterFunc(info);
    }

    ~impl()
    {
        NearlinkSaManager::GetInstance().DeregisterFunc(profileRegisterId);
    }

    int32_t profileRegisterId {INVALID_PROFILE_ID};
    sptr<ClientObserverStub> observerStub;
};

NearlinkIpShareClient::NearlinkIpShareClient() : pimpl_(std::make_shared<impl>()) {}
NearlinkIpShareClient::~NearlinkIpShareClient() = default;

NearlinkIpShareClient &NearlinkIpShareClient::GetInstance()
{
    static NearlinkIpShareClient instance;
    return instance;
}

static sptr<INearlinkIpShare> GetIpShareProxy()
{
    return GetProxy<INearlinkIpShare>(PROFILE_IPSHARE_SERVER);
}

static int32_t ValidateClientAddress(const std::string &address)
{
    if (!NearlinkHost::GetInstance().IsNearlinkSupport()) {
        return NL_ERR_API_NOT_SUPPORT;
    }
    if (!IS_SLE_ENABLED()) {
        return NL_ERR_SLE_OFF;
    }
    return IsValidAddress(address) ? NL_NO_ERROR : NL_ERR_INVALID_PARAM;
}

int32_t NearlinkIpShareClient::IsPeerSupported(const std::string &peerAddress, bool &supported) const
{
    int32_t ret = ValidateClientAddress(peerAddress);
    if (ret != NL_NO_ERROR) {
        return ret;
    }
    auto proxy = GetIpShareProxy();
    return proxy == nullptr ? NL_ERR_UNAVAILABLE_PROXY : proxy->IsPeerSupported(peerAddress, supported);
}

int32_t NearlinkIpShareClient::StartGateway(const std::string &peerAddress) const
{
    int32_t ret = ValidateClientAddress(peerAddress);
    if (ret != NL_NO_ERROR) {
        return ret;
    }
    auto proxy = GetIpShareProxy();
    return proxy == nullptr ? NL_ERR_UNAVAILABLE_PROXY : proxy->StartGateway(peerAddress);
}

int32_t NearlinkIpShareClient::StartTerminal(const std::string &gatewayAddress) const
{
    int32_t ret = ValidateClientAddress(gatewayAddress);
    if (ret != NL_NO_ERROR) {
        return ret;
    }
    auto proxy = GetIpShareProxy();
    return proxy == nullptr ? NL_ERR_UNAVAILABLE_PROXY : proxy->StartTerminal(gatewayAddress);
}

int32_t NearlinkIpShareClient::Stop() const
{
    auto proxy = GetIpShareProxy();
    return proxy == nullptr ? NL_ERR_UNAVAILABLE_PROXY : proxy->Stop();
}

int32_t NearlinkIpShareClient::GetStatus(NearlinkIpShareStatus &status) const
{
    auto proxy = GetIpShareProxy();
    return proxy == nullptr ? NL_ERR_UNAVAILABLE_PROXY : proxy->GetStatus(status);
}

int32_t NearlinkIpShareClient::RegisterObserver(const std::shared_ptr<NearlinkIpShareObserver> &observer) const
{
    if (observer == nullptr) {
        return NL_ERR_INVALID_PARAM;
    }
    auto proxy = GetIpShareProxy();
    if (proxy == nullptr) {
        return NL_ERR_UNAVAILABLE_PROXY;
    }
    sptr<ClientObserverStub> observerStub = new (std::nothrow) ClientObserverStub(observer);
    if (observerStub == nullptr) {
        return NL_ERR_INTERNAL_ERROR;
    }
    int32_t ret = proxy->RegisterObserver(observerStub);
    if (ret == NL_NO_ERROR) {
        pimpl_->observerStub = observerStub;
    }
    return ret;
}

int32_t NearlinkIpShareClient::UnregisterObserver() const
{
    auto proxy = GetIpShareProxy();
    if (proxy == nullptr) {
        return NL_ERR_UNAVAILABLE_PROXY;
    }
    int32_t ret = proxy->UnregisterObserver();
    if (ret == NL_NO_ERROR) {
        pimpl_->observerStub = nullptr;
    }
    return ret;
}

}  // namespace OHOS::Nearlink

namespace {
template <size_t Size>
void CopyText(char (&destination)[Size], const std::string &source)
{
    (void)std::snprintf(destination, Size, "%s", source.c_str());
}
}

extern "C" int32_t NlIpShareIsPeerSupported(const char *peerAddress, int32_t *supported)
{
    if (peerAddress == nullptr || supported == nullptr) {
        return OHOS::Nearlink::NL_ERR_INVALID_PARAM;
    }
    bool value = false;
    int32_t ret = OHOS::Nearlink::NearlinkIpShareClient::GetInstance().IsPeerSupported(peerAddress, value);
    *supported = value ? 1 : 0;
    return ret;
}

extern "C" int32_t NlIpShareStartGateway(const char *peerAddress)
{
    return peerAddress == nullptr ? OHOS::Nearlink::NL_ERR_INVALID_PARAM :
        OHOS::Nearlink::NearlinkIpShareClient::GetInstance().StartGateway(peerAddress);
}

extern "C" int32_t NlIpShareStartTerminal(const char *gatewayAddress)
{
    return gatewayAddress == nullptr ? OHOS::Nearlink::NL_ERR_INVALID_PARAM :
        OHOS::Nearlink::NearlinkIpShareClient::GetInstance().StartTerminal(gatewayAddress);
}

extern "C" int32_t NlIpShareStop(void)
{
    return OHOS::Nearlink::NearlinkIpShareClient::GetInstance().Stop();
}

extern "C" int32_t NlIpShareGetStatus(NlIpShareStatusC *status)
{
    if (status == nullptr) {
        return OHOS::Nearlink::NL_ERR_INVALID_PARAM;
    }
    OHOS::Nearlink::NearlinkIpShareStatus value;
    int32_t ret = OHOS::Nearlink::NearlinkIpShareClient::GetInstance().GetStatus(value);
    if (ret != OHOS::Nearlink::NL_NO_ERROR) {
        return ret;
    }
    *status = {};
    status->role = static_cast<int32_t>(value.role);
    status->state = static_cast<int32_t>(value.state);
    CopyText(status->peerAddress, value.peerAddress);
    CopyText(status->ifaceName, value.ifaceName);
    CopyText(status->ipv4Address, value.ipv4Address);
    status->hasUpstream = value.hasUpstream ? 1 : 0;
    CopyText(status->errorStage, value.errorStage);
    status->errorCode = value.errorCode;
    return OHOS::Nearlink::NL_NO_ERROR;
}
