/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */
#include "nearlink_ipshare_observer_stub.h"

#include <memory>

#include "ipc_types.h"
#include "nearlink_service_ipc_interface_code.h"

namespace OHOS::Nearlink {

int32_t NearlinkIpShareObserverStub::OnRemoteRequest(uint32_t code, MessageParcel &data, MessageParcel &reply,
    MessageOption &option)
{
    if (GetDescriptor() != data.ReadInterfaceToken()) {
        return ERR_INVALID_STATE;
    }
    if (code != NL_IPSHARE_OBSERVER_STATUS_CHANGED) {
        return IPCObjectStub::OnRemoteRequest(code, data, reply, option);
    }
    std::shared_ptr<NearlinkIpShareStatus> status(data.ReadParcelable<NearlinkIpShareStatus>());
    if (status == nullptr) {
        return TRANSACTION_ERR;
    }
    OnStatusChanged(*status);
    return NO_ERROR;
}

}  // namespace OHOS::Nearlink
