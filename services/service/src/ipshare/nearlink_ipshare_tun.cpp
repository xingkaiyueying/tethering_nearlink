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
        HILOGI("[IpShare][Tun] open skipped: interface already open");
        return 0;
    }
    if (!callback) {
        HILOGE("[IpShare][Tun] open failed: packet callback is null");
        return -EINVAL;
    }
    int fd = open(IP_SHARE_TUN_DEVICE, O_RDWR | O_CLOEXEC | O_NONBLOCK);
    if (fd < 0) {
        HILOGE("[IpShare][Tun] open /dev/tun failed errno=%{public}d", errno);
        return -errno;
    }
    struct ifreq request = {};
    request.ifr_flags = IFF_TUN | IFF_NO_PI;
    (void)strncpy(request.ifr_name, IP_SHARE_IFACE, IFNAMSIZ - 1);
    if (ioctl(fd, TUNSETIFF, &request) < 0) {
        int error = errno;
        (void)close(fd);
        HILOGE("[IpShare][Tun] TUNSETIFF failed errno=%{public}d", error);
        return -error;
    }
    callback_ = callback;
    fd_ = fd;
    running_.store(true);
    reader_ = std::thread(&NearlinkIpShareTun::ReadLoop, this);
    HILOGI("[IpShare][Tun] interface opened name=%{public}s flags=IFF_TUN|IFF_NO_PI", IP_SHARE_IFACE);
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
    HILOGI("[IpShare][Tun] interface closed hadFd=%{public}d", fd >= 0);
}

int32_t NearlinkIpShareTun::Write(const uint8_t *data, uint16_t length)
{
    if (data == nullptr || length == 0) {
        HILOGE("[IpShare][Tun] write rejected: invalid packet");
        return -EINVAL;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (fd_ < 0) {
        HILOGE("[IpShare][Tun] write failed: interface is closed");
        return -ENODEV;
    }
    ssize_t written = write(fd_, data, length);
    if (written == length) {
        return 0;
    }
    int32_t ret = written < 0 ? -errno : -EIO;
    HILOGE("[IpShare][Tun] write failed expected=%{public}u written=%{public}zd ret=%{public}d", length, written,
        ret);
    return ret;
}

bool NearlinkIpShareTun::IsOpen() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return fd_ >= 0;
}

void NearlinkIpShareTun::ReadLoop()
{
    uint8_t packet[IP_SHARE_PACKET_MAX] = {0};
    HILOGI("[IpShare][Tun] read loop started");
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
        if (pollResult < 0) {
            HILOGE("[IpShare][Tun] poll failed errno=%{public}d", errno);
            continue;
        }
        if (pollResult == 0 || (pollFd.revents & POLLIN) == 0) {
            continue;
        }
        ssize_t length = read(fd, packet, sizeof(packet));
        if (length > 0 && length <= static_cast<ssize_t>(UINT16_MAX) && callback) {
            callback(packet, static_cast<uint16_t>(length));
        } else if (length < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            HILOGE("[IpShare][Tun] read failed errno=%{public}d", errno);
        } else if (length > static_cast<ssize_t>(UINT16_MAX)) {
            HILOGE("[IpShare][Tun] read rejected: packet too large length=%{public}zd", length);
        } else if (length > 0 && !callback) {
            HILOGE("[IpShare][Tun] read dropped: packet callback is null");
        }
    }
    HILOGI("[IpShare][Tun] read loop stopped");
}

}  // namespace OHOS::Nearlink
