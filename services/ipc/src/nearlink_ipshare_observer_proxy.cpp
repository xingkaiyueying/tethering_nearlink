/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */
#include "nearlink_ipshare_observer_proxy.h"

#include "ipc_types.h"
#include "log.h"
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
        HILOGE("[IpShare][Observer] status notification marshal failed");
        return;
    }
    auto remote = Remote();
    if (remote == nullptr) {
        HILOGE("[IpShare][Observer] status notification failed: remote is null");
        return;
    }
    MessageOption option(MessageOption::TF_ASYNC);
    int32_t ret = remote->SendRequest(NL_IPSHARE_OBSERVER_STATUS_CHANGED, data, reply, option);
    if (ret != NO_ERROR) {
        HILOGE("[IpShare][Observer] status notification send failed ret=%{public}d", ret);
        return;
    }
    HILOGI("[IpShare][Observer] status notification sent role=%{public}d state=%{public}d error=%{public}d",
        static_cast<int32_t>(status.role), static_cast<int32_t>(status.state), status.errorCode);
}

}  // namespace OHOS::Nearlink
