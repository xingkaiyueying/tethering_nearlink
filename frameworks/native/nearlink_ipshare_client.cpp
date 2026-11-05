/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "nearlink_ipshare_client.h"

#include <mutex>
#include <new>

#include "i_nearlink_ipshare.h"
#include "log.h"
#include "nearlink_errorcode.h"
#include "nearlink_host.h"
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
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (status.generation < generation_ || (status.generation == generation_ && status.sequence <= sequence_)) {
                return;
            }
            generation_ = status.generation;
            sequence_ = status.sequence;
        }
        auto observer = observer_.lock();
        if (observer != nullptr) {
            observer->OnStatusChanged(status);
        }
    }

private:
    std::weak_ptr<NearlinkIpShareObserver> observer_;
    std::mutex mutex_;
    uint64_t generation_{0};
    uint64_t sequence_{0};
};
} // namespace

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

    int32_t profileRegisterId{INVALID_PROFILE_ID};
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
        HILOGE("[IpShare][Client] request rejected: NearLink is not supported");
        return NL_ERR_API_NOT_SUPPORT;
    }
    if (!IS_SLE_ENABLED()) {
        HILOGE("[IpShare][Client] request rejected: SLE is disabled");
        return NL_ERR_SLE_OFF;
    }
    if (!IsValidAddress(address)) {
        HILOGE("[IpShare][Client] request rejected: invalid peer address");
        return NL_ERR_INVALID_PARAM;
    }
    return NL_NO_ERROR;
}

int32_t NearlinkIpShareClient::IsPeerSupported(const std::string &peerAddress, bool &supported) const
{
    int32_t ret = ValidateClientAddress(peerAddress);
    if (ret != NL_NO_ERROR) {
        return ret;
    }
    auto proxy = GetIpShareProxy();
    if (proxy == nullptr) {
        HILOGE("[IpShare][Client] support probe failed: proxy unavailable");
        return NL_ERR_UNAVAILABLE_PROXY;
    }
    ret = proxy->IsPeerSupported(peerAddress, supported);
    return ret;
}

int32_t NearlinkIpShareClient::CompleteGatewayPeerRelease(uint64_t generation) const
{
    auto proxy = GetIpShareProxy();
    return proxy == nullptr ? NL_ERR_UNAVAILABLE_PROXY : proxy->CompleteGatewayPeerRelease(generation);
}

int32_t NearlinkIpShareClient::StartGatewayAny(int32_t mode, int32_t maxTerminals) const
{
    if (!NearlinkHost::GetInstance().IsNearlinkSupport() || !IS_SLE_ENABLED()) {
        return NL_ERR_SLE_OFF;
    }
    if (!IsIpShareMode(mode) || maxTerminals < 1 || maxTerminals > 32) {
        return NL_ERR_INVALID_PARAM;
    }
    auto proxy = GetIpShareProxy();
    return proxy == nullptr ? NL_ERR_UNAVAILABLE_PROXY : proxy->StartGatewayAny(mode, maxTerminals);
}

int32_t NearlinkIpShareClient::GetSupportedMaxTerminals() const
{
    auto proxy = GetIpShareProxy();
    return proxy == nullptr ? NL_ERR_UNAVAILABLE_PROXY : proxy->GetSupportedMaxTerminals();
}

int32_t NearlinkIpShareClient::StartTerminal(const std::string &gatewayAddress) const
{
    int32_t ret = ValidateClientAddress(gatewayAddress);
    if (ret != NL_NO_ERROR) {
        return ret;
    }
    auto proxy = GetIpShareProxy();
    if (proxy == nullptr) {
        HILOGE("[IpShare][Client] terminal start failed: proxy unavailable");
        return NL_ERR_UNAVAILABLE_PROXY;
    }
    ret = proxy->StartTerminal(gatewayAddress);
    return ret;
}

int32_t NearlinkIpShareClient::Stop() const
{
    auto proxy = GetIpShareProxy();
    if (proxy == nullptr) {
        HILOGE("[IpShare][Client] stop failed: proxy unavailable");
        return NL_ERR_UNAVAILABLE_PROXY;
    }
    int32_t ret = proxy->Stop();
    return ret;
}

int32_t NearlinkIpShareClient::GetStatus(NearlinkIpShareStatus &status) const
{
    auto proxy = GetIpShareProxy();
    if (proxy == nullptr) {
        HILOGE("[IpShare][Client] status query failed: proxy unavailable");
        return NL_ERR_UNAVAILABLE_PROXY;
    }
    int32_t ret = proxy->GetStatus(status);
    if (ret != NL_NO_ERROR) {
        HILOGE("[IpShare][Client] status query failed ret=%{public}d", ret);
    }
    return ret;
}

int32_t NearlinkIpShareClient::RegisterObserver(const std::shared_ptr<NearlinkIpShareObserver> &observer) const
{
    if (observer == nullptr) {
        HILOGE("[IpShare][Client] observer registration failed: observer is null");
        return NL_ERR_INVALID_PARAM;
    }
    auto proxy = GetIpShareProxy();
    if (proxy == nullptr) {
        HILOGE("[IpShare][Client] observer registration failed: proxy unavailable");
        return NL_ERR_UNAVAILABLE_PROXY;
    }
    sptr<ClientObserverStub> observerStub = new (std::nothrow) ClientObserverStub(observer);
    if (observerStub == nullptr) {
        HILOGE("[IpShare][Client] observer registration failed: allocation failed");
        return NL_ERR_INTERNAL_ERROR;
    }
    int32_t ret = proxy->RegisterObserver(observerStub);
    if (ret == NL_NO_ERROR) {
        pimpl_->observerStub = observerStub;
        NearlinkIpShareStatus snapshot;
        if (proxy->GetStatus(snapshot) == 0) {
            observerStub->OnStatusChanged(snapshot);
        }
    }
    return ret;
}

int32_t NearlinkIpShareClient::UnregisterObserver() const
{
    auto proxy = GetIpShareProxy();
    if (proxy == nullptr) {
        HILOGE("[IpShare][Client] observer unregistration failed: proxy unavailable");
        return NL_ERR_UNAVAILABLE_PROXY;
    }
    int32_t ret = proxy->UnregisterObserver();
    if (ret == NL_NO_ERROR) {
        pimpl_->observerStub = nullptr;
    }
    return ret;
}

int32_t NearlinkIpShareClient::QueryNearlinkIpShareCapabilities(const std::string &peerAddress,
                                                                NearlinkIpShareCapabilities &capabilities) const
{
    int32_t ret = ValidateClientAddress(peerAddress);
    if (ret != 0) {
        return ret;
    }
    auto proxy = GetIpShareProxy();
    return proxy == nullptr ? NL_ERR_UNAVAILABLE_PROXY
                            : proxy->QueryNearlinkIpShareCapabilities(peerAddress, capabilities);
}

int32_t NearlinkIpShareClient::StartNearlinkTerminalWithMode(const std::string &peerAddress, int32_t mode) const
{
    int32_t ret = ValidateClientAddress(peerAddress);
    if (ret != 0) {
        return ret;
    }
    auto proxy = GetIpShareProxy();
    return proxy == nullptr ? NL_ERR_UNAVAILABLE_PROXY : proxy->StartNearlinkTerminalWithMode(peerAddress, mode);
}

int32_t NearlinkIpShareClient::UpdateValidatedAddress(const NearlinkIpShareAddressEvidence &address) const
{
    auto proxy = GetIpShareProxy();
    return proxy == nullptr ? NL_ERR_UNAVAILABLE_PROXY : proxy->UpdateValidatedAddress(address);
}

} // namespace OHOS::Nearlink
