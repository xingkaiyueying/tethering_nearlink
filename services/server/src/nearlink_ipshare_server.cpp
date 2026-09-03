/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */
#include "nearlink_ipshare_server.h"

#include "log.h"
#include "nearlink_ipshare_service.h"

namespace OHOS::Nearlink {

int32_t NearlinkIpShareServer::IsPeerSupported(const std::string &peerAddress, bool &supported)
{
    int32_t ret = NearlinkIpShareService::GetInstance().IsPeerSupported(peerAddress, supported);
    HILOGI("[IpShare][Server] support probe ret=%{public}d supported=%{public}d", ret, supported);
    return ret;
}

int32_t NearlinkIpShareServer::StartGateway(const std::string &peerAddress)
{
    int32_t ret = NearlinkIpShareService::GetInstance().StartGateway(peerAddress);
    HILOGI("[IpShare][Server] gateway start ret=%{public}d", ret);
    return ret;
}

int32_t NearlinkIpShareServer::StartTerminal(const std::string &gatewayAddress)
{
    int32_t ret = NearlinkIpShareService::GetInstance().StartTerminal(gatewayAddress);
    HILOGI("[IpShare][Server] terminal start ret=%{public}d", ret);
    return ret;
}

int32_t NearlinkIpShareServer::Stop()
{
    int32_t ret = NearlinkIpShareService::GetInstance().Stop();
    HILOGI("[IpShare][Server] stop ret=%{public}d", ret);
    return ret;
}

int32_t NearlinkIpShareServer::GetStatus(NearlinkIpShareStatus &status)
{
    int32_t ret = NearlinkIpShareService::GetInstance().GetStatus(status);
    if (ret != 0) {
        HILOGE("[IpShare][Server] status failed ret=%{public}d", ret);
    } else {
        HILOGI("[IpShare][Server] status ret=0 role=%{public}d state=%{public}d error=%{public}d",
            static_cast<int32_t>(status.role), static_cast<int32_t>(status.state), status.errorCode);
    }
    return ret;
}

int32_t NearlinkIpShareServer::RegisterObserver(const sptr<INearlinkIpShareObserver> &observer)
{
    int32_t ret = NearlinkIpShareService::GetInstance().RegisterObserver(observer);
    HILOGI("[IpShare][Server] observer registration ret=%{public}d", ret);
    return ret;
}

int32_t NearlinkIpShareServer::UnregisterObserver()
{
    int32_t ret = NearlinkIpShareService::GetInstance().UnregisterObserver();
    HILOGI("[IpShare][Server] observer unregistration ret=%{public}d", ret);
    return ret;
}

}  // namespace OHOS::Nearlink
