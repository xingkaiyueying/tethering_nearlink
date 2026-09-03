/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */
#ifndef NEARLINK_IPSHARE_STATUS_H
#define NEARLINK_IPSHARE_STATUS_H

#include <cstdint>
#include <string>

#include "parcel.h"

namespace OHOS::Nearlink {

enum class NearlinkIpShareRole : int32_t {
    NONE = 0,
    GATEWAY = 1,
    TERMINAL = 2,
};

enum class NearlinkIpShareState : int32_t {
    IDLE = 0,
    STARTING = 1,
    DISCOVERING = 2,
    CONFIGURING = 3,
    IFACE_READY = 4,
    CHANNEL_READY = 5,
    DHCP = 6,
    SERVING = 7,
    SERVING_NO_UPSTREAM = 8,
    ACTIVE = 9,
    STOPPING = 10,
    ERROR = 11,
};

class NearlinkIpShareStatus final : public Parcelable {
public:
    NearlinkIpShareRole role {NearlinkIpShareRole::NONE};
    NearlinkIpShareState state {NearlinkIpShareState::IDLE};
    std::string peerAddress;
    std::string ifaceName;
    std::string ipv4Address;
    bool hasUpstream {false};
    std::string errorStage;
    int32_t errorCode {0};

    bool Marshalling(Parcel &parcel) const override;
    static NearlinkIpShareStatus *Unmarshalling(Parcel &parcel);
    bool ReadFromParcel(Parcel &parcel);
};

}  // namespace OHOS::Nearlink
#endif  // NEARLINK_IPSHARE_STATUS_H
