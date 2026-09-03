/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */
#ifndef I_NEARLINK_IPSHARE_OBSERVER_H
#define I_NEARLINK_IPSHARE_OBSERVER_H

#include "iremote_broker.h"
#include "nearlink_ipshare_status.h"

namespace OHOS::Nearlink {

class INearlinkIpShareObserver : public IRemoteBroker {
public:
    DECLARE_INTERFACE_DESCRIPTOR(u"ohos.ipc.INearlinkIpShareObserver");
    virtual void OnStatusChanged(const NearlinkIpShareStatus &status) = 0;
};

}  // namespace OHOS::Nearlink
#endif  // I_NEARLINK_IPSHARE_OBSERVER_H
