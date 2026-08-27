/*
 * Host-side check for frozen Demo UUID table. No OpenHarmony runtime required.
 * Build: g++ -std=c++17 -I../../utils/include iposl_demo_uuid_host_test.cpp
 */
#include "iposl_demo_uuid.h"

#include <cstdio>
#include <set>
#include <string>

using OHOS::Nearlink::IpShareDemo::DEMO_UUID_COUNT;
using OHOS::Nearlink::IpShareDemo::DEMO_UUID_TABLE;
using OHOS::Nearlink::IpShareDemo::IsNearlinkStandardUuidBase;
using OHOS::Nearlink::IpShareDemo::IsValidUuidStringFormat;
using OHOS::Nearlink::IpShareDemo::RoundTripUuidString;

int main()
{
    std::set<std::string> unique;
    int failures = 0;
    for (size_t i = 0; i < DEMO_UUID_COUNT; ++i) {
        std::string uuid = DEMO_UUID_TABLE[i];
        unique.insert(uuid);
        std::string roundTrip;
        if (!IsValidUuidStringFormat(uuid)) {
            std::fprintf(stderr, "format fail: %s\n", uuid.c_str());
            ++failures;
        }
        if (IsNearlinkStandardUuidBase(uuid)) {
            std::fprintf(stderr, "standard base hit: %s\n", uuid.c_str());
            ++failures;
        }
        if (!RoundTripUuidString(uuid, roundTrip) || roundTrip != uuid) {
            std::fprintf(stderr, "round-trip fail: %s -> %s\n", uuid.c_str(), roundTrip.c_str());
            ++failures;
        }
    }
    if (unique.size() != DEMO_UUID_COUNT) {
        std::fprintf(stderr, "uuid table has duplicates\n");
        ++failures;
    }
    const std::string standardSample = "37BEA880-FC70-11EA-B720-000000000609";
    if (!IsNearlinkStandardUuidBase(standardSample)) {
        std::fprintf(stderr, "standard sample should match base prefix\n");
        ++failures;
    }
    if (failures != 0) {
        std::fprintf(stderr, "FAILED %d\n", failures);
        return 1;
    }
    std::printf("PASS %zu demo uuids\n", DEMO_UUID_COUNT);
    return 0;
}
