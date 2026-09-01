/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */
#ifndef NEARLINK_IPSHARE_OBSERVER_STUB_H
#define NEARLINK_IPSHARE_OBSERVER_STUB_H

#include "i_nearlink_ipshare_observer.h"
#include "iremote_stub.h"

namespace OHOS::Nearlink {

class NearlinkIpShareObserverStub : public IRemoteStub<INearlinkIpShareObserver> {
public:
    int32_t OnRemoteRequest(uint32_t code, MessageParcel &data, MessageParcel &reply,
        MessageOption &option) override;
};

}  // namespace OHOS::Nearlink
#endif  // NEARLINK_IPSHARE_OBSERVER_STUB_H
