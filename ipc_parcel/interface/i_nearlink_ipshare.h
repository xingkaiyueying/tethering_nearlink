/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */
#ifndef I_NEARLINK_IPSHARE_H
#define I_NEARLINK_IPSHARE_H

#include <string>

#include "i_nearlink_ipshare_observer.h"
#include "iremote_broker.h"
#include "nearlink_ipshare_status.h"

namespace OHOS::Nearlink {

inline const std::string PROFILE_IPSHARE_SERVER = "NearlinkIpShare";

class INearlinkIpShare : public IRemoteBroker {
public:
    DECLARE_INTERFACE_DESCRIPTOR(u"ohos.ipc.INearlinkIpShare");

    virtual int32_t IsPeerSupported(const std::string &peerAddress, bool &supported) = 0;
    virtual int32_t StartGateway(const std::string &peerAddress) = 0;
    virtual int32_t StartTerminal(const std::string &gatewayAddress) = 0;
    virtual int32_t Stop() = 0;
    virtual int32_t GetStatus(NearlinkIpShareStatus &status) = 0;
    virtual int32_t RegisterObserver(const sptr<INearlinkIpShareObserver> &observer) = 0;
    virtual int32_t UnregisterObserver() = 0;
};

}  // namespace OHOS::Nearlink
#endif  // I_NEARLINK_IPSHARE_H
