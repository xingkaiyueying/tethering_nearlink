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

#include "iposl_ssap_probe.h"

#include <utility>

#include "iposl_demo_uuid.h"

namespace OHOS {
namespace Nearlink {
namespace IpShareDemo {
namespace {
constexpr uint8_t DEMO_VERSION = 0x01;
constexpr uint8_t DEMO_IP_TYPE_IPV4 = 0x01;
constexpr uint8_t DEMO_COMM_UNICAST = 0x01;
constexpr uint8_t DEMO_PI_IPV4 = 0x01;
constexpr uint8_t DEMO_STATE_IDLE = 0x00;
constexpr uint8_t DEMO_NAPT_BIT = 0x02; /* T/XS 30001 NAT/NAPT bitmap Bit1 */
constexpr uint32_t DEMO_PROPERTY_READ = 0x01;
}  // namespace

static Property MakeProperty(const char *uuidStr, const std::vector<uint8_t> &value)
{
    Property property(0, Uuid::ConvertFromString(uuidStr), value, DEMO_PROPERTY_READ, 0);
    return property;
}

DemoSsapDiscoveryProbe BuildDemoSsapDiscoveryProbe()
{
    DemoSsapDiscoveryProbe probe;
    probe.identityService.isPrimary_ = true;
    probe.identityService.uuid_ = Uuid::ConvertFromString(DEMO_IP_NETWORK_SHARING_SERVICE_UUID);

    probe.iposlConfigService.isPrimary_ = true;
    probe.iposlConfigService.uuid_ = Uuid::ConvertFromString(DEMO_IPOSL_CONFIG_SERVICE_UUID);

    const std::vector<uint8_t> terminalCapability = {
        DEMO_VERSION, DEMO_IP_TYPE_IPV4, DEMO_COMM_UNICAST, DEMO_PI_IPV4, 0x00};
    const std::vector<uint8_t> terminalState = {DEMO_STATE_IDLE, DEMO_IP_TYPE_IPV4};
    const std::vector<uint8_t> gatewayCapability = {
        DEMO_VERSION, DEMO_IP_TYPE_IPV4, DEMO_COMM_UNICAST, DEMO_PI_IPV4, DEMO_NAPT_BIT};
    const std::vector<uint8_t> gatewayState = {DEMO_STATE_IDLE, DEMO_IP_TYPE_IPV4};

    probe.iposlConfigService.properties_.push_back(
        MakeProperty(DEMO_IPOSL_TERMINAL_CAPABILITY_UUID, terminalCapability));
    probe.iposlConfigService.properties_.push_back(MakeProperty(DEMO_IPOSL_TERMINAL_STATE_UUID, terminalState));
    probe.iposlConfigService.properties_.push_back(
        MakeProperty(DEMO_IPOSL_GATEWAY_CAPABILITY_UUID, gatewayCapability));
    probe.iposlConfigService.properties_.push_back(MakeProperty(DEMO_IPOSL_GATEWAY_STATE_UUID, gatewayState));

    Method method(0, Uuid::ConvertFromString(DEMO_IPOSL_NODE_CONFIG_METHOD_UUID));
    method.permission_ = 0;
    probe.iposlConfigService.methods_.push_back(std::move(method));
    return probe;
}

}  // namespace IpShareDemo
}  // namespace Nearlink
}  // namespace OHOS
