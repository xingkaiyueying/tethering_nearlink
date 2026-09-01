/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */
#include "nearlink_ipshare_tun.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "log.h"

namespace OHOS::Nearlink {
namespace {
constexpr char IP_SHARE_TUN_DEVICE[] = "/dev/tun";
constexpr char IP_SHARE_IFACE[] = "sleip0";
constexpr size_t IP_SHARE_PACKET_MAX = 1500;
}

NearlinkIpShareTun::~NearlinkIpShareTun()
{
    Close();
}

int32_t NearlinkIpShareTun::Open(const PacketCallback &callback)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (fd_ >= 0) {
        return 0;
    }
    if (!callback) {
        return -EINVAL;
    }
    int fd = open(IP_SHARE_TUN_DEVICE, O_RDWR | O_CLOEXEC | O_NONBLOCK);
    if (fd < 0) {
        HILOGE("[IpShare] open /dev/tun failed errno=%{public}d", errno);
        return -errno;
    }
    struct ifreq request = {};
    request.ifr_flags = IFF_TUN | IFF_NO_PI;
    (void)strncpy(request.ifr_name, IP_SHARE_IFACE, IFNAMSIZ - 1);
    if (ioctl(fd, TUNSETIFF, &request) < 0) {
        int error = errno;
        (void)close(fd);
        HILOGE("[IpShare] TUNSETIFF failed errno=%{public}d", error);
        return -error;
    }
    callback_ = callback;
    fd_ = fd;
    running_.store(true);
    reader_ = std::thread(&NearlinkIpShareTun::ReadLoop, this);
    return 0;
}

void NearlinkIpShareTun::Close()
{
    int fd = -1;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        running_.store(false);
        fd = fd_;
        fd_ = -1;
    }
    if (fd >= 0) {
        (void)close(fd);
    }
    if (reader_.joinable()) {
        reader_.join();
    }
    std::lock_guard<std::mutex> lock(mutex_);
    callback_ = nullptr;
}

int32_t NearlinkIpShareTun::Write(const uint8_t *data, uint16_t length)
{
    if (data == nullptr || length == 0) {
        return -EINVAL;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (fd_ < 0) {
        return -ENODEV;
    }
    ssize_t written = write(fd_, data, length);
    return written == length ? 0 : (written < 0 ? -errno : -EIO);
}

bool NearlinkIpShareTun::IsOpen() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return fd_ >= 0;
}

void NearlinkIpShareTun::ReadLoop()
{
    uint8_t packet[IP_SHARE_PACKET_MAX] = {0};
    while (running_.load()) {
        int fd = -1;
        PacketCallback callback;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            fd = fd_;
            callback = callback_;
        }
        if (fd < 0) {
            break;
        }
        struct pollfd pollFd = {.fd = fd, .events = POLLIN, .revents = 0};
        int pollResult = poll(&pollFd, 1, 200);
        if (pollResult <= 0 || (pollFd.revents & POLLIN) == 0) {
            continue;
        }
        ssize_t length = read(fd, packet, sizeof(packet));
        if (length > 0 && length <= static_cast<ssize_t>(UINT16_MAX) && callback) {
            callback(packet, static_cast<uint16_t>(length));
        }
    }
}

}  // namespace OHOS::Nearlink
