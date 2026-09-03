/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */
#ifndef NEARLINK_IPSHARE_SERVER_H
#define NEARLINK_IPSHARE_SERVER_H

#include "nearlink_ipshare_stub.h"

namespace OHOS::Nearlink {

class NearlinkIpShareServer final : public NearlinkIpShareStub {
public:
    int32_t IsPeerSupported(const std::string &peerAddress, bool &supported) override;
    int32_t StartGateway(const std::string &peerAddress) override;
    int32_t StartTerminal(const std::string &gatewayAddress) override;
    int32_t Stop() override;
    int32_t GetStatus(NearlinkIpShareStatus &status) override;
    int32_t RegisterObserver(const sptr<INearlinkIpShareObserver> &observer) override;
    int32_t UnregisterObserver() override;
};

}  // namespace OHOS::Nearlink
#endif  // NEARLINK_IPSHARE_SERVER_H
