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

#ifndef IPOSL_DEMO_UUID_H
#define IPOSL_DEMO_UUID_H

#include <cstddef>
#include <string>

namespace OHOS {
namespace Nearlink {

/* Matches NAPI CheckBaseUuid(): first 32 chars of a 36-char UUID string. */
constexpr const char *NEARLINK_STANDARD_UUID_BASE_PREFIX = "37BEA880-FC70-11EA-B720-00000000";
constexpr size_t NEARLINK_STANDARD_UUID_BASE_PREFIX_LEN = 32;
constexpr size_t NEARLINK_UUID_STRING_LEN = 36;

namespace IpShareDemo {

constexpr const char *DEMO_IP_NETWORK_SHARING_SERVICE_UUID = "7C2305C3-570A-4FDD-BB7A-01BF1A714683";
constexpr const char *DEMO_IPOSL_CONFIG_SERVICE_UUID = "5B285005-4555-4FA5-B957-C29C0D8A60C3";
constexpr const char *DEMO_IPOSL_TERMINAL_CAPABILITY_UUID = "FA361B52-CB0A-49ED-8427-1A675ACCD9DB";
constexpr const char *DEMO_IPOSL_TERMINAL_STATE_UUID = "E4720AB0-E62A-42D2-AD96-98627CEEAEDB";
constexpr const char *DEMO_IPOSL_GATEWAY_CAPABILITY_UUID = "2E68C566-E314-466D-9629-402F16CAB219";
constexpr const char *DEMO_IPOSL_GATEWAY_STATE_UUID = "43117FB2-909E-4B70-B6F9-1052D729C89B";
constexpr const char *DEMO_IPOSL_NODE_CONFIG_METHOD_UUID = "7AA3120E-F0D2-4560-B711-A5B618B7A32B";

constexpr size_t DEMO_UUID_COUNT = 7;

inline const char *const DEMO_UUID_TABLE[DEMO_UUID_COUNT] = {
    DEMO_IP_NETWORK_SHARING_SERVICE_UUID,
    DEMO_IPOSL_CONFIG_SERVICE_UUID,
    DEMO_IPOSL_TERMINAL_CAPABILITY_UUID,
    DEMO_IPOSL_TERMINAL_STATE_UUID,
    DEMO_IPOSL_GATEWAY_CAPABILITY_UUID,
    DEMO_IPOSL_GATEWAY_STATE_UUID,
    DEMO_IPOSL_NODE_CONFIG_METHOD_UUID,
};

inline bool IsHexDigit(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

inline char ToUpperHex(char c)
{
    if (c >= 'a' && c <= 'f') {
        return static_cast<char>(c - 'a' + 'A');
    }
    return c;
}

inline bool IsValidUuidStringFormat(const std::string &uuid)
{
    if (uuid.size() != NEARLINK_UUID_STRING_LEN) {
        return false;
    }
    if (uuid[8] != '-' || uuid[13] != '-' || uuid[18] != '-' || uuid[23] != '-') {
        return false;
    }
    for (size_t i = 0; i < NEARLINK_UUID_STRING_LEN; ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            continue;
        }
        if (!IsHexDigit(uuid[i])) {
            return false;
        }
    }
    return true;
}

inline bool IsNearlinkStandardUuidBase(const std::string &uuid)
{
    if (!IsValidUuidStringFormat(uuid)) {
        return false;
    }
    for (size_t i = 0; i < NEARLINK_STANDARD_UUID_BASE_PREFIX_LEN; ++i) {
        if (ToUpperHex(uuid[i]) != NEARLINK_STANDARD_UUID_BASE_PREFIX[i]) {
            return false;
        }
    }
    return true;
}

inline int HexValue(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return -1;
}

inline bool RoundTripUuidString(const std::string &uuid, std::string &outUpper)
{
    if (!IsValidUuidStringFormat(uuid)) {
        return false;
    }
    unsigned char bytes[16] = {0};
    size_t byteIndex = 0;
    for (size_t i = 0; i < NEARLINK_UUID_STRING_LEN;) {
        if (uuid[i] == '-') {
            ++i;
            continue;
        }
        int hi = HexValue(uuid[i]);
        int lo = HexValue(uuid[i + 1]);
        if (hi < 0 || lo < 0 || byteIndex >= 16) {
            return false;
        }
        bytes[byteIndex++] = static_cast<unsigned char>((hi << 4) | lo);
        i += 2;
    }
    if (byteIndex != 16) {
        return false;
    }
    static const char *kHex = "0123456789ABCDEF";
    outUpper.clear();
    outUpper.reserve(NEARLINK_UUID_STRING_LEN);
    for (size_t i = 0; i < 16; ++i) {
        outUpper.push_back(kHex[(bytes[i] >> 4) & 0xF]);
        outUpper.push_back(kHex[bytes[i] & 0xF]);
        if (i == 3 || i == 5 || i == 7 || i == 9) {
            outUpper.push_back('-');
        }
    }
    std::string expected;
    expected.resize(NEARLINK_UUID_STRING_LEN);
    for (size_t i = 0; i < NEARLINK_UUID_STRING_LEN; ++i) {
        expected[i] = ToUpperHex(uuid[i]);
    }
    return outUpper == expected;
}

}  // namespace IpShareDemo
}  // namespace Nearlink
}  // namespace OHOS

#endif  // IPOSL_DEMO_UUID_H
