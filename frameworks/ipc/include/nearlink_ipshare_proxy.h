/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */
#ifndef NEARLINK_IPSHARE_PROXY_H
#define NEARLINK_IPSHARE_PROXY_H

#include "i_nearlink_ipshare.h"
#include "iremote_proxy.h"

namespace OHOS::Nearlink {

class NearlinkIpShareProxy final : public IRemoteProxy<INearlinkIpShare> {
public:
    explicit NearlinkIpShareProxy(const sptr<IRemoteObject> &impl);
    ~NearlinkIpShareProxy() override = default;

    int32_t IsPeerSupported(const std::string &peerAddress, bool &supported) override;
    int32_t StartGateway(const std::string &peerAddress) override;
    int32_t StartTerminal(const std::string &gatewayAddress) override;
    int32_t Stop() override;
    int32_t GetStatus(NearlinkIpShareStatus &status) override;
    int32_t RegisterObserver(const sptr<INearlinkIpShareObserver> &observer) override;
    int32_t UnregisterObserver() override;

private:
    int32_t Transact(uint32_t code, MessageParcel &data, MessageParcel &reply);
    int32_t AddressCommand(uint32_t code, const std::string &address);
    static inline BrokerDelegator<NearlinkIpShareProxy> delegator_;
};

}  // namespace OHOS::Nearlink
#endif  // NEARLINK_IPSHARE_PROXY_H
