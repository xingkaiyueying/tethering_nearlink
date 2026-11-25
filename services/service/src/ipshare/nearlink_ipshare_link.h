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
#ifndef NEARLINK_IPSHARE_LINK_H
#define NEARLINK_IPSHARE_LINK_H

#include <chrono>
#include <array>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <vector>
#include "nearlink_ipshare_tun.h"

namespace OHOS::Nearlink {
class NearlinkIpShareChannel;

// One gateway session owns the TUN. Members own their authenticated transports.
class NearlinkIpShareLink final : public std::enable_shared_from_this<NearlinkIpShareLink> {
public:
    ~NearlinkIpShareLink();
    int32_t Open(uint64_t generation);
    void Close();
    bool Add(const std::shared_ptr<NearlinkIpShareChannel> &channel, uint64_t epoch);
    void Remove(uint64_t epoch);
    void RefreshLocalAddresses(uint64_t generation);
    int32_t Receive(NearlinkIpShareChannel &source, const uint8_t *data, uint16_t length, uint64_t epoch);

private:
    struct Member {
        std::shared_ptr<NearlinkIpShareChannel> channel;
        uint64_t epoch;
    };
    void FromTun(const uint8_t *data, uint16_t length);
    int32_t Dispatch(NearlinkIpShareChannel *source, const uint8_t *data, uint16_t length, uint64_t epoch);
    bool SourceAllowed(NearlinkIpShareChannel &source, const uint8_t *data, uint16_t length);
    void RefreshOwnersLocked(NearlinkIpShareChannel &channel, uint64_t epoch);
    void UpdateOwnerLocked(const std::string &address, uint64_t epoch);
    bool IsLocalIpv6Locked(const uint8_t address[16], std::chrono::steady_clock::time_point now) const;
    void ReportLocked(const char *reason, uint64_t epoch = 0, uint8_t family = 0);
    std::mutex mutex_;
    NearlinkIpShareTun tun_;
    uint64_t generation_{0};
    std::map<uint64_t, std::weak_ptr<NearlinkIpShareChannel>> members_;
    std::map<std::string, uint64_t> owners_;
    std::set<std::array<uint8_t, 16>> localIpv6_;
    uint32_t localIfindex_{0};
    uint64_t localRefreshTicket_{0};
    std::chrono::steady_clock::time_point localCacheUntil_{};
    struct Transaction {
        uint64_t epoch;
        std::chrono::steady_clock::time_point until;
    };
    std::map<std::array<uint8_t, 10>, Transaction> transactions_;
    uint64_t local_{0};
    uint64_t forwarded_{0};
    uint64_t dropped_{0};
    std::chrono::steady_clock::time_point lastReport_{};
};
} // namespace OHOS::Nearlink
#endif
