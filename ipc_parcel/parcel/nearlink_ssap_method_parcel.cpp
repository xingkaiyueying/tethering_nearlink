/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "securec.h"
#include "nearlink_uuid_parcel.h"
#include "log.h"
#include "nearlink_ssap_method_parcel.h"

namespace OHOS {
namespace Nearlink {
const uint32_t SSAP_METHOD_PARCEL_SIZE_MAX = 0x1000;
bool NearlinkSsapMethodParcel::Marshalling(Parcel &parcel) const
{
    if (!parcel.WriteUint16(handle_)) {
        return false;
    }
    NearlinkUuidParcel uuid(uuid_);
    if (!parcel.WriteParcelable(&uuid)) {
        return false;
    }
    if (parameter_.size() > SSAP_METHOD_PARCEL_SIZE_MAX || !parcel.WriteUint32(parameter_.size())) {
        return false;
    }
    HILOGI("parameter_ size:%{public}lu", parameter_.size());
    for (size_t i = 0; i < parameter_.size(); i++) {
        if (!parcel.WriteUint8(parameter_[i])) {
            return false;
        }
    }
    if (result_.size() > SSAP_METHOD_PARCEL_SIZE_MAX || !parcel.WriteUint32(result_.size())) {
        return false;
    }
    HILOGI("result_ size:%{public}lu", result_.size());
    for (size_t i = 0; i < result_.size(); i++) {
        if (!parcel.WriteUint8(result_[i])) {
            return false;
        }
    }
    if (!parcel.WriteInt32(permission_)) {
        return false;
    }
    return true;
}

NearlinkSsapMethodParcel *NearlinkSsapMethodParcel::Unmarshalling(Parcel &parcel)
{
    NearlinkSsapMethodParcel *method = new (std::nothrow) NearlinkSsapMethodParcel();
    if (method != nullptr && !method->ReadFromParcel(parcel)) {
        delete method;
        method = nullptr;
    }
    return method;
}

bool NearlinkSsapMethodParcel::WriteToParcel(Parcel &parcel)
{
    return Marshalling(parcel);
}

bool NearlinkSsapMethodParcel::ReadFromParcel(Parcel &parcel)
{
    if (!parcel.ReadUint16(handle_)) {
        return false;
    }
    std::shared_ptr<NearlinkUuidParcel> uuid(parcel.ReadParcelable<NearlinkUuidParcel>());
    if (!uuid) {
        return false;
    }
    uuid_ = NearlinkUuidParcel(*uuid);
    uint32_t ps;
    if (!parcel.ReadUint32(ps)|| ps > SSAP_METHOD_PARCEL_SIZE_MAX) {
        HILOGE("read parcel parameter size error, parameter size=0x%{public}x", ps);
        return false;
    }
    HILOGI("ps:%{public}u", ps);
    std::vector<uint8_t> parameter;
    uint8_t p;
    for (size_t i = 0; i < ps; i++) {
        if (!parcel.ReadUint8(p)) {
            return false;
        }
        parameter.emplace_back(p);
    }
    parameter_ = std::move(parameter);
    HILOGI("parameter_ size:%{public}lu", parameter_.size());
    uint32_t rs;
    if (!parcel.ReadUint32(rs)|| rs > SSAP_METHOD_PARCEL_SIZE_MAX) {
        HILOGE("read parcel result size error, result size=0x%{public}x", rs);
        return false;
    }
    HILOGI("rs:%{public}u", rs);
    std::vector<uint8_t> result;
    uint8_t r;
    for (size_t i = 0; i < rs; i++) {
        if (!parcel.ReadUint8(r)) {
            return false;
        }
        result.emplace_back(r);
    }
    result_ = std::move(result);
    HILOGI("result_ size:%{public}lu", result_.size());
    int32_t permission = 0;
    if (!parcel.ReadInt32(permission)) {
        return false;
    }
    permission_ = static_cast<uint16_t>(permission);
    return true;
}
}  // namespace Nearlink
}  // namespace OHOS
