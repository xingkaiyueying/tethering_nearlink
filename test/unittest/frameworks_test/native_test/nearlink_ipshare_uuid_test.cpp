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

#include <array>
#include <regex>
#include <set>
#include <string>

#include <gtest/gtest.h>

#include "nearlink_uuid.h"
#include "sle_uuid.h"

namespace OHOS {
namespace Nearlink {
namespace TEST {
namespace {
constexpr std::array<const char *, 7> DEMO_IPSHARE_UUIDS = {
    "7C2305C3-570A-4FDD-BB7A-01BF1A714683",
    "5B285005-4555-4FA5-B957-C29C0D8A60C3",
    "FA361B52-CB0A-49ED-8427-1A675ACCD9DB",
    "E4720AB0-E62A-42D2-AD96-98627CEEAEDB",
    "2E68C566-E314-466D-9629-402F16CAB219",
    "43117FB2-909E-4B70-B6F9-1052D729C89B",
    "7AA3120E-F0D2-4560-B711-A5B618B7A32B",
};

constexpr const char *STANDARD_UUID_BASE_PREFIX = "37BEA880-FC70-11EA-B720-";
const std::regex UUID128_PATTERN("^[0-9A-F]{8}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{12}$");
}

TEST(NearlinkIpShareUuidTest, DemoUuidsAreUniquePrivate128BitRoundTrips)
{
    std::set<std::string> uniqueUuids;
    for (const char *value : DEMO_IPSHARE_UUIDS) {
        const std::string uuidString(value);
        EXPECT_EQ(36U, uuidString.size());
        EXPECT_TRUE(std::regex_match(uuidString, UUID128_PATTERN));
        EXPECT_TRUE(uniqueUuids.insert(uuidString).second);
        EXPECT_NE(0U, uuidString.rfind(STANDARD_UUID_BASE_PREFIX, 0));

        Uuid stackUuid = Uuid::ConvertFromString(uuidString);
        EXPECT_EQ(Uuid::UUID128_BYTES_TYPE, stackUuid.GetUuidType());
        EXPECT_EQ(uuidString, stackUuid.ConvertToString());
        EXPECT_EQ(uuidString, Uuid::ConvertFrom128Bits(stackUuid.ConvertTo128Bits()).ConvertToString());

        UUID frameworkUuid = UUID::FromString(uuidString);
        EXPECT_EQ(uuidString, frameworkUuid.ToString());
        EXPECT_EQ(uuidString, UUID::ConvertFrom128Bits(frameworkUuid.ConvertTo128Bits()).ToString());
    }
}
} // namespace TEST
} // namespace Nearlink
} // namespace OHOS
