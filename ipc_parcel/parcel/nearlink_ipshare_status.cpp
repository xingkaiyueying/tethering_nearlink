/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */
#include "nearlink_ipshare_status.h"

#include <new>

namespace OHOS::Nearlink {

bool NearlinkIpShareStatus::Marshalling(Parcel &parcel) const
{
    return parcel.WriteInt32(static_cast<int32_t>(role)) &&
        parcel.WriteInt32(static_cast<int32_t>(state)) &&
        parcel.WriteString(peerAddress) &&
        parcel.WriteString(ifaceName) &&
        parcel.WriteString(ipv4Address) &&
        parcel.WriteBool(hasUpstream) &&
        parcel.WriteString(errorStage) &&
        parcel.WriteInt32(errorCode);
}

NearlinkIpShareStatus *NearlinkIpShareStatus::Unmarshalling(Parcel &parcel)
{
    auto *status = new (std::nothrow) NearlinkIpShareStatus();
    if (status != nullptr && !status->ReadFromParcel(parcel)) {
        delete status;
        status = nullptr;
    }
    return status;
}

bool NearlinkIpShareStatus::ReadFromParcel(Parcel &parcel)
{
    int32_t roleValue = 0;
    int32_t stateValue = 0;
    if (!parcel.ReadInt32(roleValue) || !parcel.ReadInt32(stateValue) ||
        !parcel.ReadString(peerAddress) || !parcel.ReadString(ifaceName) ||
        !parcel.ReadString(ipv4Address) || !parcel.ReadBool(hasUpstream) ||
        !parcel.ReadString(errorStage) || !parcel.ReadInt32(errorCode)) {
        return false;
    }
    if (roleValue < static_cast<int32_t>(NearlinkIpShareRole::NONE) ||
        roleValue > static_cast<int32_t>(NearlinkIpShareRole::TERMINAL) ||
        stateValue < static_cast<int32_t>(NearlinkIpShareState::IDLE) ||
        stateValue > static_cast<int32_t>(NearlinkIpShareState::ERROR)) {
        return false;
    }
    role = static_cast<NearlinkIpShareRole>(roleValue);
    state = static_cast<NearlinkIpShareState>(stateValue);
    return true;
}

}  // namespace OHOS::Nearlink
