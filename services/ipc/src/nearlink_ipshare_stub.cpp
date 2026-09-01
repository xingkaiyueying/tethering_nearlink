/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */
#include "nearlink_ipshare_stub.h"

#include "ipc_types.h"
#include "nearlink_errorcode.h"
#include "nearlink_permission_manager.h"
#include "nearlink_service_ipc_interface_code.h"

namespace OHOS::Nearlink {

NearlinkIpShareStub::NearlinkIpShareStub()
{
    auto permission = CHECK_PERM(true, MULTI_PERM(ACCESS_NEARLINK, CONNECTIVITY_INTERNAL));
    memberFuncMap_ = {
        {NL_IPSHARE_IS_PEER_SUPPORTED, {IsPeerSupportedInner, permission}},
        {NL_IPSHARE_START_GATEWAY, {StartGatewayInner, permission}},
        {NL_IPSHARE_START_TERMINAL, {StartTerminalInner, permission}},
        {NL_IPSHARE_STOP, {StopInner, permission}},
        {NL_IPSHARE_GET_STATUS, {GetStatusInner, permission}},
        {NL_IPSHARE_REGISTER_OBSERVER, {RegisterObserverInner, permission}},
        {NL_IPSHARE_UNREGISTER_OBSERVER, {UnregisterObserverInner, permission}},
    };
}

int32_t NearlinkIpShareStub::OnRemoteRequest(uint32_t code, MessageParcel &data, MessageParcel &reply,
    MessageOption &option)
{
    CHECK_PERMISSION_AND_EXECUTE(NearlinkIpShareStub);
}

int32_t NearlinkIpShareStub::IsPeerSupportedInner(NearlinkIpShareStub *stub, MessageParcel &data,
    MessageParcel &reply)
{
    std::string address;
    if (!data.ReadString(address)) {
        return TRANSACTION_ERR;
    }
    bool supported = false;
    int32_t ret = stub->IsPeerSupported(address, supported);
    if (!reply.WriteInt32(ret) || (ret == 0 && !reply.WriteBool(supported))) {
        return TRANSACTION_ERR;
    }
    return NO_ERROR;
}

int32_t NearlinkIpShareStub::StartGatewayInner(NearlinkIpShareStub *stub, MessageParcel &data,
    MessageParcel &reply)
{
    std::string address;
    if (!data.ReadString(address) || !reply.WriteInt32(stub->StartGateway(address))) {
        return TRANSACTION_ERR;
    }
    return NO_ERROR;
}

int32_t NearlinkIpShareStub::StartTerminalInner(NearlinkIpShareStub *stub, MessageParcel &data,
    MessageParcel &reply)
{
    std::string address;
    if (!data.ReadString(address) || !reply.WriteInt32(stub->StartTerminal(address))) {
        return TRANSACTION_ERR;
    }
    return NO_ERROR;
}

int32_t NearlinkIpShareStub::StopInner(NearlinkIpShareStub *stub, MessageParcel &data, MessageParcel &reply)
{
    (void)data;
    return reply.WriteInt32(stub->Stop()) ? NO_ERROR : TRANSACTION_ERR;
}

int32_t NearlinkIpShareStub::GetStatusInner(NearlinkIpShareStub *stub, MessageParcel &data, MessageParcel &reply)
{
    (void)data;
    NearlinkIpShareStatus status;
    int32_t ret = stub->GetStatus(status);
    if (!reply.WriteInt32(ret) || (ret == 0 && !reply.WriteParcelable(&status))) {
        return TRANSACTION_ERR;
    }
    return NO_ERROR;
}

int32_t NearlinkIpShareStub::RegisterObserverInner(NearlinkIpShareStub *stub, MessageParcel &data,
    MessageParcel &reply)
{
    sptr<IRemoteObject> remote = data.ReadRemoteObject();
    sptr<INearlinkIpShareObserver> observer = iface_cast<INearlinkIpShareObserver>(remote);
    if (observer == nullptr || !reply.WriteInt32(stub->RegisterObserver(observer))) {
        return TRANSACTION_ERR;
    }
    return NO_ERROR;
}

int32_t NearlinkIpShareStub::UnregisterObserverInner(NearlinkIpShareStub *stub, MessageParcel &data,
    MessageParcel &reply)
{
    (void)data;
    return reply.WriteInt32(stub->UnregisterObserver()) ? NO_ERROR : TRANSACTION_ERR;
}

}  // namespace OHOS::Nearlink
