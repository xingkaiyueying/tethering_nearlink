/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
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
