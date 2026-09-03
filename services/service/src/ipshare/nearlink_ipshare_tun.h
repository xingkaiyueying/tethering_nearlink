/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */
#ifndef NEARLINK_IPSHARE_TUN_H
#define NEARLINK_IPSHARE_TUN_H

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>

namespace OHOS::Nearlink {

class NearlinkIpShareTun final {
public:
    using PacketCallback = std::function<void(const uint8_t *, uint16_t)>;

    NearlinkIpShareTun() = default;
    ~NearlinkIpShareTun();

    int32_t Open(const PacketCallback &callback);
    void Close();
    int32_t Write(const uint8_t *data, uint16_t length);
    bool IsOpen() const;

private:
    void ReadLoop();

    mutable std::mutex mutex_;
    int fd_ {-1};
    std::atomic_bool running_ {false};
    PacketCallback callback_;
    std::thread reader_;
};

}  // namespace OHOS::Nearlink
#endif  // NEARLINK_IPSHARE_TUN_H
