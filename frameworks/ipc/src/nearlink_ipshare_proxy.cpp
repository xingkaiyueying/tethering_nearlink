/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */
#include "nearlink_ipshare_proxy.h"

#include <memory>

#include "ipc_types.h"
#include "nearlink_errorcode.h"
#include "nearlink_service_ipc_interface_code.h"

namespace OHOS::Nearlink {

NearlinkIpShareProxy::NearlinkIpShareProxy(const sptr<IRemoteObject> &impl)
    : IRemoteProxy<INearlinkIpShare>(impl)
{}

int32_t NearlinkIpShareProxy::Transact(uint32_t code, MessageParcel &data, MessageParcel &reply)
{
    auto remote = Remote();
    if (remote == nullptr) {
        return NL_ERR_UNAVAILABLE_PROXY;
    }
    MessageOption option(MessageOption::TF_SYNC);
    return remote->SendRequest(code, data, reply, option) == NO_ERROR ? NL_NO_ERROR : NL_ERR_IPC_TRANS_FAILED;
}

int32_t NearlinkIpShareProxy::AddressCommand(uint32_t code, const std::string &address)
{
    MessageParcel data;
    MessageParcel reply;
    if (!data.WriteInterfaceToken(GetDescriptor()) || !data.WriteString(address)) {
        return NL_ERR_IPC_TRANS_FAILED;
    }
    int32_t ret = Transact(code, data, reply);
    return ret == NL_NO_ERROR ? reply.ReadInt32() : ret;
}

int32_t NearlinkIpShareProxy::IsPeerSupported(const std::string &peerAddress, bool &supported)
{
    MessageParcel data;
    MessageParcel reply;
    if (!data.WriteInterfaceToken(GetDescriptor()) || !data.WriteString(peerAddress)) {
        return NL_ERR_IPC_TRANS_FAILED;
    }
    int32_t ret = Transact(NL_IPSHARE_IS_PEER_SUPPORTED, data, reply);
    if (ret != NL_NO_ERROR) {
        return ret;
    }
    ret = reply.ReadInt32();
    if (ret == NL_NO_ERROR && !reply.ReadBool(supported)) {
        return NL_ERR_IPC_TRANS_FAILED;
    }
    return ret;
}

int32_t NearlinkIpShareProxy::StartGateway(const std::string &peerAddress)
{
    return AddressCommand(NL_IPSHARE_START_GATEWAY, peerAddress);
}

int32_t NearlinkIpShareProxy::StartTerminal(const std::string &gatewayAddress)
{
    return AddressCommand(NL_IPSHARE_START_TERMINAL, gatewayAddress);
}

int32_t NearlinkIpShareProxy::Stop()
{
    MessageParcel data;
    MessageParcel reply;
    if (!data.WriteInterfaceToken(GetDescriptor())) {
        return NL_ERR_IPC_TRANS_FAILED;
    }
    int32_t ret = Transact(NL_IPSHARE_STOP, data, reply);
    return ret == NL_NO_ERROR ? reply.ReadInt32() : ret;
}

int32_t NearlinkIpShareProxy::GetStatus(NearlinkIpShareStatus &status)
{
    MessageParcel data;
    MessageParcel reply;
    if (!data.WriteInterfaceToken(GetDescriptor())) {
        return NL_ERR_IPC_TRANS_FAILED;
    }
    int32_t ret = Transact(NL_IPSHARE_GET_STATUS, data, reply);
    if (ret != NL_NO_ERROR) {
        return ret;
    }
    ret = reply.ReadInt32();
    if (ret != NL_NO_ERROR) {
        return ret;
    }
    std::shared_ptr<NearlinkIpShareStatus> result(reply.ReadParcelable<NearlinkIpShareStatus>());
    if (result == nullptr) {
        return NL_ERR_IPC_TRANS_FAILED;
    }
    status = *result;
    return NL_NO_ERROR;
}

int32_t NearlinkIpShareProxy::RegisterObserver(const sptr<INearlinkIpShareObserver> &observer)
{
    if (observer == nullptr) {
        return NL_ERR_INVALID_PARAM;
    }
    MessageParcel data;
    MessageParcel reply;
    if (!data.WriteInterfaceToken(GetDescriptor()) || !data.WriteRemoteObject(observer->AsObject())) {
        return NL_ERR_IPC_TRANS_FAILED;
    }
    int32_t ret = Transact(NL_IPSHARE_REGISTER_OBSERVER, data, reply);
    return ret == NL_NO_ERROR ? reply.ReadInt32() : ret;
}

int32_t NearlinkIpShareProxy::UnregisterObserver()
{
    MessageParcel data;
    MessageParcel reply;
    if (!data.WriteInterfaceToken(GetDescriptor())) {
        return NL_ERR_IPC_TRANS_FAILED;
    }
    int32_t ret = Transact(NL_IPSHARE_UNREGISTER_OBSERVER, data, reply);
    return ret == NL_NO_ERROR ? reply.ReadInt32() : ret;
}

}  // namespace OHOS::Nearlink
