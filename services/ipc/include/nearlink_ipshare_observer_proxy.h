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
#ifndef NEARLINK_IPSHARE_OBSERVER_PROXY_H
#define NEARLINK_IPSHARE_OBSERVER_PROXY_H

#include "i_nearlink_ipshare_observer.h"
#include "iremote_proxy.h"

namespace OHOS::Nearlink {

class NearlinkIpShareObserverProxy final : public IRemoteProxy<INearlinkIpShareObserver> {
public:
    explicit NearlinkIpShareObserverProxy(const sptr<IRemoteObject> &impl);
    ~NearlinkIpShareObserverProxy() override = default;
    void OnStatusChanged(const NearlinkIpShareStatus &status) override;

private:
    static inline BrokerDelegator<NearlinkIpShareObserverProxy> delegator_;
};

}  // namespace OHOS::Nearlink
#endif  // NEARLINK_IPSHARE_OBSERVER_PROXY_H
