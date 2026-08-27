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
#ifndef SSAP_UTILS_H
#define SSAP_UTILS_H

#include "nearlink_def.h"
#include "raw_address.h"
#include "sle_uuid.h"
#include "ssap_inner_def.h"
#include "ssap_log.h"

#include "ssap_data.h"
#include "ssap_type.h"
#include "nlstk_public_define.h"

struct NLSTK_ServiceParam;

namespace OHOS {
namespace Nearlink {
SLE_Addr_S ConvertToSleAddr(const RawAddress &addr);
NLSTK_SsapUuid_S ConvertToSleUuid(const Uuid &uuid);
int ConvertFromPDUError(int errorCode);
int ConvertStateFromStackSsapState(uint8_t state);
bool FillMethodsToStackService(const Service &srcService, struct NLSTK_ServiceParam *dstService);
} // namespace Nearlink
} // namespace OHOS

#endif // SSAP_UTILS_H