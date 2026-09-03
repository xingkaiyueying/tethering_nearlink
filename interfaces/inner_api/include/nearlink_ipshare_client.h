/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */
#ifndef NEARLINK_IPSHARE_CLIENT_H
#define NEARLINK_IPSHARE_CLIENT_H

#include <memory>
#include <string>

#include "nearlink_def.h"
#include "nearlink_ipshare_status.h"

namespace OHOS::Nearlink {

class NEARLINK_API NearlinkIpShareObserver {
public:
    virtual ~NearlinkIpShareObserver() = default;
    virtual void OnStatusChanged(const NearlinkIpShareStatus &status) = 0;
};

class NEARLINK_API NearlinkIpShareClient final {
public:
    static NearlinkIpShareClient &GetInstance();

    int32_t IsPeerSupported(const std::string &peerAddress, bool &supported) const;
    int32_t StartGateway(const std::string &peerAddress) const;
    int32_t StartTerminal(const std::string &gatewayAddress) const;
    int32_t Stop() const;
    int32_t GetStatus(NearlinkIpShareStatus &status) const;
    int32_t RegisterObserver(const std::shared_ptr<NearlinkIpShareObserver> &observer) const;
    int32_t UnregisterObserver() const;

private:
    NearlinkIpShareClient();
    ~NearlinkIpShareClient();
    struct impl;
    std::shared_ptr<impl> pimpl_;
};

}  // namespace OHOS::Nearlink
#endif  // NEARLINK_IPSHARE_CLIENT_H
