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

#ifndef IPOSL_SSAP_PROBE_H
#define IPOSL_SSAP_PROBE_H

#include <vector>

#include "ssap_data.h"

namespace OHOS {
namespace Nearlink {
namespace IpShareDemo {

struct DemoSsapDiscoveryProbe {
    Service identityService;
    Service iposlConfigService;
};

/*
 * Stage-0 discovery probe: empty identity service plus IPoSL config service
 * with 4 non-empty properties and 1 node-config method. Values are a Demo
 * subset of T/XS 30001 fields so native AddService will not skip empty
 * properties. Full 0x01/0x02 semantics belong to stage 1.
 */
DemoSsapDiscoveryProbe BuildDemoSsapDiscoveryProbe();

}  // namespace IpShareDemo
}  // namespace Nearlink
}  // namespace OHOS

#endif  // IPOSL_SSAP_PROBE_H
