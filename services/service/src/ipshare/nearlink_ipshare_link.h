/* Copyright (c) 2026 Huawei Device Co., Ltd. Licensed under the Apache License, Version 2.0. */
#ifndef NEARLINK_IPSHARE_LINK_H
#define NEARLINK_IPSHARE_LINK_H

#include <chrono>
#include <array>
#include <map>
#include <memory>
#include <mutex>
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
    void ReportLocked(const char *reason, uint64_t epoch = 0, uint8_t family = 0);
    void TraceDnsLocked(const char *stage, const uint8_t *data, uint16_t length,
                         uint64_t sourceEpoch, uint64_t targetEpoch = 0, int32_t code = 0);
    std::mutex mutex_;
    NearlinkIpShareTun tun_;
    uint64_t generation_{0};
    std::map<uint64_t, std::weak_ptr<NearlinkIpShareChannel>> members_;
    std::map<std::string, uint64_t> owners_;
    struct Transaction {
        uint64_t epoch;
        std::chrono::steady_clock::time_point until;
    };
    std::map<std::array<uint8_t, 10>, Transaction> transactions_;
    uint64_t local_{0}, forwarded_{0}, dropped_{0};
    std::chrono::steady_clock::time_point lastReport_{};
    std::chrono::steady_clock::time_point traceWindow_{};
    uint64_t traceCount_{0}, traceSuppressed_{0};
};
} // namespace OHOS::Nearlink
#endif
