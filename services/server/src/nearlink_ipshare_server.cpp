/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */
#include "nearlink_ipshare_server.h"

#include "nearlink_ipshare_service.h"

namespace OHOS::Nearlink {

int32_t NearlinkIpShareServer::IsPeerSupported(const std::string &peerAddress, bool &supported)
{
    return NearlinkIpShareService::GetInstance().IsPeerSupported(peerAddress, supported);
}

int32_t NearlinkIpShareServer::StartGateway(const std::string &peerAddress)
{
    return NearlinkIpShareService::GetInstance().StartGateway(peerAddress);
}

int32_t NearlinkIpShareServer::StartTerminal(const std::string &gatewayAddress)
{
    return NearlinkIpShareService::GetInstance().StartTerminal(gatewayAddress);
}

int32_t NearlinkIpShareServer::Stop()
{
    return NearlinkIpShareService::GetInstance().Stop();
}

int32_t NearlinkIpShareServer::GetStatus(NearlinkIpShareStatus &status)
{
    return NearlinkIpShareService::GetInstance().GetStatus(status);
}

int32_t NearlinkIpShareServer::RegisterObserver(const sptr<INearlinkIpShareObserver> &observer)
{
    return NearlinkIpShareService::GetInstance().RegisterObserver(observer);
}

int32_t NearlinkIpShareServer::UnregisterObserver()
{
    return NearlinkIpShareService::GetInstance().UnregisterObserver();
}

}  // namespace OHOS::Nearlink
