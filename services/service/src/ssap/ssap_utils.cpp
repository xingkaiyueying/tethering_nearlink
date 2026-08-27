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
#include "ssap_utils.h"

#include "ssap_def.h"
#include "nlstk_ssap_app_link.h"
#include "nlstk_ssap_app_server.h"
#include "SleRemoteDeviceAdapter.h"
#include "securec.h"
#include "ssap_log.h"

#define RANGE_STREAM_SIZE 4

namespace OHOS {
namespace Nearlink {

SLE_Addr_S ConvertToSleAddr(const RawAddress &addr)
{
    SLE_Addr_S sleAddr;
    memset_s(&sleAddr, sizeof(SLE_Addr_S), 0, sizeof(SLE_Addr_S));
    addr.ConvertToUint8(sleAddr.addr);
    sleAddr.type = SleRemoteDeviceAdapter::GetInstance()->GetPeerDeviceAddrType(addr);
    return sleAddr;
}

NLSTK_SsapUuid_S ConvertToSleUuid(const Uuid &uuid)
{
    NLSTK_SsapUuid_S sleUuid;
    memset_s(&sleUuid, sizeof(NLSTK_SsapUuid_S), 0, sizeof(NLSTK_SsapUuid_S));
    uuid.ConvertToBytesLE(sleUuid.uuid);
    return sleUuid;
}

int ConvertFromPDUError(int errorCode)
{
    if (errorCode != SsapStatus::SSAP_SUCCESS) {
        return SsapStatus::SSAP_SUCCESS - errorCode;
    }
    return SsapStatus::SSAP_SUCCESS;
}

bool FillMethodsToStackService(const Service &srcService, NLSTK_ServiceParam_S *dstService)
{
    if (dstService == nullptr) {
        SSAP_LOGE("dstService is nullptr");
        return false;
    }
    uint32_t methodNum = static_cast<uint32_t>(srcService.methods_.size());
    dstService->serviceMethodNum = static_cast<uint16_t>(methodNum);
    dstService->method = nullptr;
    if (methodNum == 0) {
        return true;
    }
    dstService->method = new (std::nothrow) NLSTK_SsapServiceMethodParam_S[methodNum];
    if (dstService->method == nullptr) {
        SSAP_LOGE("method is nullptr");
        return false;
    }
    (void)memset_s(dstService->method, sizeof(NLSTK_SsapServiceMethodParam_S) * methodNum, 0x00,
        sizeof(NLSTK_SsapServiceMethodParam_S) * methodNum);
    for (uint32_t i = 0; i < methodNum; i++) {
        const Method &srcMethod = srcService.methods_[i];
        dstService->method[i].type = (srcMethod.uuid_.GetUuidType() == Uuid::UUID16_BYTES_TYPE)
            ? ITEM_TYPE_STD_METHOD : ITEM_TYPE_VENDOR_METHOD;
        dstService->method[i].uuid = ConvertToSleUuid(srcMethod.uuid_);
        dstService->method[i].permission.permissionValue = static_cast<uint8_t>(srcMethod.permission_);
    }
    return true;
}

int ConvertStateFromStackSsapState(uint8_t state)
{
    switch (state) {
        case SSAP_CONNECT_STATE_DISCONNECTED:
            return static_cast<int>(SleConnectState::DISCONNECTED);
        case SSAP_CONNECT_STATE_CONNECTING:
            return static_cast<int>(SleConnectState::CONNECTING);
        case SSAP_CONNECT_STATE_CONNECTED:
            return static_cast<int>(SleConnectState::CONNECTED);
        case SSAP_CONNECT_STATE_DISCONNECTING:
            return static_cast<int>(SleConnectState::DISCONNECTING);
        default:
            return static_cast<int>(SleConnectState::DISCONNECTED);
    }
}
} // namespace Nearlink
} // namespace OHOS
