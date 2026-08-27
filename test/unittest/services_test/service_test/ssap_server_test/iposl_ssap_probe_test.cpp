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

#include <cstring>
#include <gtest/gtest.h>

#include "iposl_demo_uuid.h"
#include "iposl_ssap_probe.h"
#include "nlstk_ssap_app_server.h"
#include "ssap_type.h"
#include "ssap_utils.h"

namespace OHOS {
namespace Nearlink {
namespace TEST {
using namespace testing::ext;
using IpShareDemo::BuildDemoSsapDiscoveryProbe;
using IpShareDemo::DEMO_IPOSL_NODE_CONFIG_METHOD_UUID;

class IposlSsapProbeTest : public testing::Test {
public:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    void SetUp() {}
    void TearDown() {}
};

/**
 * @tc.name: DemoProbeHasEmptyIdentityAndIposlConfigMethod
 * @tc.desc: Stage-0 probe is 1 empty identity service plus IPoSL config with 4 non-empty properties and 1 method.
 * @tc.type: FUNC
 */
HWTEST_F(IposlSsapProbeTest, DemoProbeHasEmptyIdentityAndIposlConfigMethod, TestSize.Level1)
{
    auto probe = BuildDemoSsapDiscoveryProbe();
    EXPECT_TRUE(probe.identityService.properties_.empty());
    EXPECT_TRUE(probe.identityService.methods_.empty());
    EXPECT_EQ(probe.iposlConfigService.properties_.size(), 4u);
    EXPECT_EQ(probe.iposlConfigService.methods_.size(), 1u);
    for (const auto &property : probe.iposlConfigService.properties_) {
        EXPECT_FALSE(property.value_.empty());
    }
    EXPECT_EQ(probe.iposlConfigService.methods_[0].uuid_.ToString(), DEMO_IPOSL_NODE_CONFIG_METHOD_UUID);
}

/**
 * @tc.name: FillMethodsCopiesVendorMethodIntoStackParam
 * @tc.desc: SSAP service model methods are copied into NLSTK_ServiceParam_S::method.
 * @tc.type: FUNC
 */
HWTEST_F(IposlSsapProbeTest, FillMethodsCopiesVendorMethodIntoStackParam, TestSize.Level1)
{
    auto probe = BuildDemoSsapDiscoveryProbe();
    NLSTK_ServiceParam_S stackService = {};
    ASSERT_TRUE(FillMethodsToStackService(probe.iposlConfigService, &stackService));
    ASSERT_EQ(stackService.serviceMethodNum, 1);
    ASSERT_NE(stackService.method, nullptr);
    EXPECT_EQ(stackService.method[0].type, ITEM_TYPE_VENDOR_METHOD);

    NLSTK_SsapUuid_S expected = ConvertToSleUuid(probe.iposlConfigService.methods_[0].uuid_);
    EXPECT_EQ(memcmp(stackService.method[0].uuid.uuid, expected.uuid, sizeof(expected.uuid)), 0);

    delete[] stackService.method;
    stackService.method = nullptr;
}

/**
 * @tc.name: FillMethodsNullDstFails
 * @tc.desc: Method fill rejects a null stack parameter.
 * @tc.type: FUNC
 */
HWTEST_F(IposlSsapProbeTest, FillMethodsNullDstFails, TestSize.Level1)
{
    auto probe = BuildDemoSsapDiscoveryProbe();
    EXPECT_FALSE(FillMethodsToStackService(probe.iposlConfigService, nullptr));
}

/**
 * @tc.name: EmptyIdentityHasNoMethodsOnStack
 * @tc.desc: Empty identity service produces zero stack methods.
 * @tc.type: FUNC
 */
HWTEST_F(IposlSsapProbeTest, EmptyIdentityHasNoMethodsOnStack, TestSize.Level1)
{
    auto probe = BuildDemoSsapDiscoveryProbe();
    NLSTK_ServiceParam_S stackService = {};
    ASSERT_TRUE(FillMethodsToStackService(probe.identityService, &stackService));
    EXPECT_EQ(stackService.serviceMethodNum, 0);
    EXPECT_EQ(stackService.method, nullptr);
}
}  // namespace TEST
}  // namespace Nearlink
}  // namespace OHOS
