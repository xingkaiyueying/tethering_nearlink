/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */
#include "nearlink_ipshare_observer_stub.h"

#include <memory>

#include "ipc_types.h"
#include "log.h"
#include "nearlink_service_ipc_interface_code.h"

namespace OHOS::Nearlink {

int32_t NearlinkIpShareObserverStub::OnRemoteRequest(uint32_t code, MessageParcel &data, MessageParcel &reply,
    MessageOption &option)
{
    if (GetDescriptor() != data.ReadInterfaceToken()) {
        HILOGE("[IpShare][Observer] status notification rejected: interface token mismatch");
        return ERR_INVALID_STATE;
    }
    if (code != NL_IPSHARE_OBSERVER_STATUS_CHANGED) {
        return IPCObjectStub::OnRemoteRequest(code, data, reply, option);
    }
    std::shared_ptr<NearlinkIpShareStatus> status(data.ReadParcelable<NearlinkIpShareStatus>());
    if (status == nullptr) {
        HILOGE("[IpShare][Observer] status notification rejected: parcel missing");
        return TRANSACTION_ERR;
    }
    HILOGI("[IpShare][Observer] status received role=%{public}d state=%{public}d error=%{public}d",
        static_cast<int32_t>(status->role), static_cast<int32_t>(status->state), status->errorCode);
    OnStatusChanged(*status);
    return NO_ERROR;
}

}  // namespace OHOS::Nearlink
