/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */
#include "nearlink_ipshare_observer_proxy.h"

#include "ipc_types.h"
#include "nearlink_service_ipc_interface_code.h"

namespace OHOS::Nearlink {

NearlinkIpShareObserverProxy::NearlinkIpShareObserverProxy(const sptr<IRemoteObject> &impl)
    : IRemoteProxy<INearlinkIpShareObserver>(impl)
{}

void NearlinkIpShareObserverProxy::OnStatusChanged(const NearlinkIpShareStatus &status)
{
    MessageParcel data;
    MessageParcel reply;
    if (!data.WriteInterfaceToken(GetDescriptor()) || !data.WriteParcelable(&status)) {
        return;
    }
    auto remote = Remote();
    if (remote == nullptr) {
        return;
    }
    MessageOption option(MessageOption::TF_ASYNC);
    (void)remote->SendRequest(NL_IPSHARE_OBSERVER_STATUS_CHANGED, data, reply, option);
}

}  // namespace OHOS::Nearlink
