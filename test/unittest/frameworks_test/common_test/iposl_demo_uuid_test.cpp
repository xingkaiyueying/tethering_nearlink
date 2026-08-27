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

#include <gtest/gtest.h>
#include <set>
#include <string>

#include "iposl_demo_uuid.h"
#include "nearlink_uuid.h"
#include "nearlink_utils.h"
#include "sle_uuid.h"

namespace OHOS {
namespace Nearlink {
namespace TEST {
using namespace testing::ext;
using IpShareDemo::DEMO_UUID_COUNT;
using IpShareDemo::DEMO_UUID_TABLE;
using IpShareDemo::IsNearlinkStandardUuidBase;
using IpShareDemo::RoundTripUuidString;

class IposlDemoUuidTest : public testing::Test {
public:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    void SetUp() {}
    void TearDown() {}
};

/**
 * @tc.name: DemoUuidTableIsUniqueAndPrivate128Bit
 * @tc.desc: Frozen Demo UUIDs pass format, 128-bit type, standard-base exclusion and round-trip checks.
 * @tc.type: FUNC
 */
HWTEST_F(IposlDemoUuidTest, DemoUuidTableIsUniqueAndPrivate128Bit, TestSize.Level1)
{
    std::set<std::string> unique;
    for (size_t i = 0; i < DEMO_UUID_COUNT; ++i) {
        const std::string uuidStr = DEMO_UUID_TABLE[i];
        unique.insert(uuidStr);

        EXPECT_TRUE(IsValidUuid(uuidStr)) << uuidStr;
        EXPECT_FALSE(IsNearlinkStandardUuidBase(uuidStr)) << uuidStr;

        Uuid utilsUuid = Uuid::ConvertFromString(uuidStr);
        EXPECT_EQ(utilsUuid.GetUuidType(), Uuid::UUID128_BYTES_TYPE) << uuidStr;
        EXPECT_EQ(utilsUuid.ToString(), uuidStr) << uuidStr;

        UUID frameworkUuid = UUID::FromString(uuidStr);
        EXPECT_EQ(frameworkUuid.ToString(), uuidStr) << uuidStr;

        uint8_t bytes[Uuid::UUID128_BYTES_TYPE] = {0};
        EXPECT_TRUE(utilsUuid.ConvertToBytesLE(bytes, sizeof(bytes)));
        Uuid fromBytes = Uuid::ConvertFromBytesSle(bytes, sizeof(bytes));
        EXPECT_EQ(fromBytes.ToString(), uuidStr) << uuidStr;

        std::string hostRoundTrip;
        EXPECT_TRUE(RoundTripUuidString(uuidStr, hostRoundTrip));
        EXPECT_EQ(hostRoundTrip, uuidStr);
    }
    EXPECT_EQ(unique.size(), DEMO_UUID_COUNT);
}

/**
 * @tc.name: StandardSleUuidBaseIsRejectedForDemo
 * @tc.desc: 37BEA880-FC70-11EA-B720-00000000 prefix is the SLE standard base and must not be used by Demo.
 * @tc.type: FUNC
 */
HWTEST_F(IposlDemoUuidTest, StandardSleUuidBaseIsRejectedForDemo, TestSize.Level1)
{
    const std::string standardDis = "37BEA880-FC70-11EA-B720-000000000609";
    EXPECT_TRUE(IsValidUuid(standardDis));
    EXPECT_TRUE(IsNearlinkStandardUuidBase(standardDis));
    EXPECT_EQ(Uuid::ConvertFromString(standardDis).GetUuidType(), Uuid::UUID16_BYTES_TYPE);
}

/**
 * @tc.name: InvalidUuidFormatIsRejected
 * @tc.desc: Prefix helper requires a syntactically valid 36-character UUID.
 * @tc.type: FUNC
 */
HWTEST_F(IposlDemoUuidTest, InvalidUuidFormatIsRejected, TestSize.Level1)
{
    EXPECT_FALSE(IsNearlinkStandardUuidBase(""));
    EXPECT_FALSE(IsNearlinkStandardUuidBase("7C2305C3-570A-4FDD-BB7A-01BF1A71468"));
    EXPECT_FALSE(IsValidUuid("7C2305C3-570A-4FDD-BB7A-01BF1A71468G"));
}
}  // namespace TEST
}  // namespace Nearlink
}  // namespace OHOS
