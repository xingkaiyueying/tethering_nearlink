/* Copyright (c) 2026 Huawei Device Co., Ltd. Licensed under the Apache License, Version 2.0. */
#include "nearlink_ipshare_link.h"

#include <algorithm>
#include <cstring>
#include <iterator>
#include "iposl_profile.h"
#include "log.h"
#include "nearlink_ipshare_channel.h"

namespace OHOS::Nearlink {
namespace {
uint32_t V4(const uint8_t *address)
{
    return (uint32_t(address[0]) << 24) | (uint32_t(address[1]) << 16) |
           (uint32_t(address[2]) << 8) | address[3];
}

size_t NdOffset(const uint8_t *data, size_t length)
{
    if (length < 40 || data[0] >> 4 != 6) {
        return 0;
    }
    size_t offset = 40;
    uint8_t protocol = data[6];
    for (unsigned count = 0; count < 8 && (protocol == 0 || protocol == 60 || protocol == 51); ++count) {
        if (length - offset < 2) {
            return 0;
        }
        size_t size = protocol == 51 ? (size_t(data[offset + 1]) + 2) * 4 : (size_t(data[offset + 1]) + 1) * 8;
        if (size > length - offset) {
            return 0;
        }
        protocol = data[offset];
        offset += size;
    }
    return protocol == 58 && length - offset >= 8 && data[offset] >= 133 && data[offset] <= 137 ? offset : 0;
}
} // namespace

NearlinkIpShareLink::~NearlinkIpShareLink()
{
    Close();
}

int32_t NearlinkIpShareLink::Open(uint64_t generation)
{
    {
        std::lock_guard lock(mutex_);
        generation_ = generation;
    }
    std::weak_ptr<NearlinkIpShareLink> weak = shared_from_this();
    int32_t ret = tun_.Open([weak](const uint8_t *data, uint16_t length) {
        if (auto link = weak.lock()) {
            link->FromTun(data, length);
        }
    }, "sleip0");
    HILOGI("[IpShare][SharedLink] open generation=%{public}llu iface=sleip0 code=%{public}d",
           static_cast<unsigned long long>(generation), ret);
    return ret;
}

void NearlinkIpShareLink::Close()
{
    {
        std::lock_guard lock(mutex_);
        if (generation_) {
            lastReport_ = {};
            ReportLocked("stop");
        }
        generation_ = 0;
        for (const auto &[epoch, weak] : members_) {
            if (auto channel = weak.lock()) {
                std::lock_guard channelLock(channel->mutex_);
                channel->enabled_ = false;
            }
        }
        members_.clear();
        owners_.clear();
        transactions_.clear();
    }
    tun_.Close(); // Join the reader after releasing the dispatch lock.
}

bool NearlinkIpShareLink::Add(const std::shared_ptr<NearlinkIpShareChannel> &channel, uint64_t epoch)
{
    std::lock_guard lock(mutex_);
    auto existing = members_.find(epoch);
    if (existing != members_.end()) {
        return existing->second.lock() == channel;
    }
    if (!generation_ || !epoch || !channel || members_.size() >= 32) {
        return false;
    }
    members_.emplace(epoch, channel);
    channel->AttachLink(shared_from_this());
    HILOGI("[IpShare][SharedLink] member added epoch=%{public}llu members=%{public}zu",
           static_cast<unsigned long long>(epoch), members_.size());
    return true;
}

void NearlinkIpShareLink::Remove(uint64_t epoch)
{
    std::lock_guard lock(mutex_);
    auto member = members_.find(epoch);
    if (member != members_.end()) {
        if (auto channel = member->second.lock()) {
            std::lock_guard channelLock(channel->mutex_);
            channel->enabled_ = false;
        }
    }
    members_.erase(epoch);
    for (auto it = owners_.begin(); it != owners_.end();) {
        it = it->second == epoch ? owners_.erase(it) : std::next(it);
    }
    HILOGI("[IpShare][SharedLink] member revoked epoch=%{public}llu members=%{public}zu",
           static_cast<unsigned long long>(epoch), members_.size());
}

void NearlinkIpShareLink::RefreshOwnersLocked(NearlinkIpShareChannel &channel, uint64_t epoch)
{
    for (auto it = owners_.begin(); it != owners_.end();) {
        it = it->second == epoch ? owners_.erase(it) : std::next(it);
    }
    std::lock_guard channelLock(channel.mutex_);
    if (!channel.active_ || !channel.enabled_ || channel.generation_ != epoch || !members_.count(epoch)) {
        return;
    }
    if (channel.DhcpBoundLocked()) {
        const uint8_t address[] = {uint8_t(channel.boundIp_ >> 24), uint8_t(channel.boundIp_ >> 16),
                                   uint8_t(channel.boundIp_ >> 8), uint8_t(channel.boundIp_)};
        owners_.emplace(std::string(reinterpret_cast<const char *>(address), sizeof(address)), epoch);
    }
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    channel.ipv6_.Expire(now);
    for (const auto &mapping : channel.ipv6_.Mappings()) {
        if (mapping.terminal && mapping.confirmed && !mapping.conflict) {
            owners_.emplace(std::string(reinterpret_cast<const char *>(mapping.address.data()), 16), epoch);
        }
    }
}

bool NearlinkIpShareLink::SourceAllowed(NearlinkIpShareChannel &source, const uint8_t *data, uint16_t length)
{
    if (data[0] >> 4 != 6 || std::all_of(data + 8, data + 24, [](uint8_t value) { return value == 0; })) {
        return true;
    }
    size_t nd = NdOffset(data, length);
    // Only control claims need a kernel lookup. Established data uses the
    // address evidence cache maintained by the channel.
    if (nd && NearlinkIpShareTun::IsIpv6AddressUsable(data + 8, "sleip0")) {
        return false;
    }
    auto owner = owners_.find(std::string(reinterpret_cast<const char *>(data + 8), 16));
    if (owner != owners_.end()) {
        auto member = members_.find(owner->second);
        auto channel = member == members_.end() ? nullptr : member->second.lock();
        if (channel && channel->OwnsAddress(data + 8, true)) {
            return channel.get() == &source;
        }
    }
    {
        std::lock_guard lock(source.mutex_);
        for (const auto &mapping : source.ipv6_.Mappings()) {
            if (!mapping.terminal && mapping.confirmed && !mapping.conflict &&
                std::equal(data + 8, data + 24, mapping.address.begin())) {
                return false;
            }
        }
    }
    for (const auto &[epoch, weak] : members_) {
        auto other = weak.lock();
        if (other && other.get() != &source && other->OwnsAddress(data + 8, true, true) &&
            !(source.OwnsAddress(data + 8, true) && !other->OwnsAddress(data + 8, true))) {
            return false;
        }
    }
    return true;
}

int32_t NearlinkIpShareLink::Receive(NearlinkIpShareChannel &source, const uint8_t *data, uint16_t length,
                                    uint64_t epoch)
{
    return Dispatch(&source, data, length, epoch);
}

void NearlinkIpShareLink::FromTun(const uint8_t *data, uint16_t length)
{
    (void)Dispatch(nullptr, data, length, 0);
}

int32_t NearlinkIpShareLink::Dispatch(NearlinkIpShareChannel *source, const uint8_t *data, uint16_t length,
                                     uint64_t epoch)
{
    uint8_t pi = data && length && data[0] >> 4 == 6 ? 2 : 1;
    if (!IposlCodecValidatePacket(pi, data, length)) {
        return -1;
    }
    const bool ipv6 = pi == 2;
    const size_t nd = NdOffset(data, length);
    const bool dhcp = !ipv6 && NearlinkIpShareChannel::IsDhcpPacket(data, length);
    NearlinkIpShareChannel::DhcpPacket transaction{};
    if (dhcp && !NearlinkIpShareChannel::ParseDhcpPacket(data, length, transaction)) {
        return -1;
    }
    std::array<uint8_t, 10> transactionKey{};
    std::copy(transaction.key, transaction.key + 6, transactionKey.begin());
    for (unsigned i = 0; i < 4; ++i) {
        transactionKey[6 + i] = transaction.xid >> (24 - i * 8);
    }
    std::vector<Member> targets;
    bool local = false;
    {
        std::lock_guard lock(mutex_);
        auto now = std::chrono::steady_clock::now();
        for (auto it = transactions_.begin(); it != transactions_.end();) {
            it = it->second.until <= now ? transactions_.erase(it) : std::next(it);
        }
        auto pending = transactions_.find(transactionKey);
        if (source && dhcp && pending != transactions_.end() && pending->second.epoch != epoch) {
            ++dropped_;
            ReportLocked("dhcp-retired-epoch", epoch, pi);
            return -1;
        }
        auto member = members_.find(epoch);
        if (!generation_ || (source && (member == members_.end() || member->second.lock().get() != source ||
            !SourceAllowed(*source, data, length) || !source->AuthorizePacket(data, length, epoch, true)))) {
            ++dropped_;
            ReportLocked("source", epoch, pi);
            return -1;
        }
        auto sourceOwner = owners_.find(std::string(reinterpret_cast<const char *>(data + (ipv6 ? 8 : 12)),
                                                   ipv6 ? 16 : 4));
        if (source && (dhcp || NdOffset(data, length) || sourceOwner == owners_.end())) {
            RefreshOwnersLocked(*source, epoch);
        }
        if (source && dhcp) {
            if (pending == transactions_.end() && transactions_.size() >= 128) {
                ++dropped_;
                ReportLocked("dhcp-budget");
                return -1;
            }
            transactions_[transactionKey] = {epoch, now + std::chrono::seconds(120)};
            pending = transactions_.find(transactionKey);
        }
        if (!source && dhcp && transaction.message == 5 && pending != transactions_.end()) {
            const uint8_t offered[] = {uint8_t(transaction.offered >> 24), uint8_t(transaction.offered >> 16),
                                       uint8_t(transaction.offered >> 8), uint8_t(transaction.offered)};
            auto existing = owners_.find(std::string(reinterpret_cast<const char *>(offered), sizeof(offered)));
            if (existing != owners_.end() && existing->second != pending->second.epoch) {
                auto member = members_.find(existing->second);
                auto channel = member == members_.end() ? nullptr : member->second.lock();
                if (channel && channel->OwnsAddress(offered, false)) {
                    ++dropped_;
                    ReportLocked("dhcp-address-conflict", pending->second.epoch, pi);
                    return -1;
                }
            }
        }
        const uint8_t *destination = data + (ipv6 ? 24 : 16);
        auto owner = owners_.find(std::string(reinterpret_cast<const char *>(destination), ipv6 ? 16 : 4));
        bool group = ipv6 ? destination[0] == 0xff : V4(destination) == 0xffffffff ||
                         (destination[0] >= 224 && destination[0] <= 239);
        bool onLink = ipv6 && (destination[0] == 0xfe && (destination[1] & 0xc0) == 0x80);
        if (source) {
            std::lock_guard channelLock(source->mutex_);
            if (ipv6) {
                auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
                onLink = source->ipv6_.OnLink(destination, seconds);
            } else if (source->subnetMask_) {
                onLink = (V4(destination) & source->subnetMask_) == (source->boundIp_ & source->subnetMask_);
                group = group || V4(destination) == (source->boundIp_ | ~source->subnetMask_);
            }
        }
        bool routerSolicitation = source && nd && data[nd] == 133;
        for (const auto &[targetEpoch, weak] : members_) {
            if (!dhcp && !group && owner != owners_.end() && owner->second != targetEpoch) {
                continue;
            }
            auto target = weak.lock();
            if (!target) {
                continue;
            }
            // Prefix/subnet evidence comes from this member's authenticated configuration.
            {
                std::lock_guard channelLock(target->mutex_);
                if (ipv6 && target->mode_ != 3) {
                    continue;
                }
                if (!source && !ipv6 && target->DhcpBoundLocked() && target->subnetMask_) {
                    group = group || V4(destination) == (target->boundIp_ | ~target->subnetMask_);
                }
            }
            if (target.get() == source || routerSolicitation || (source && dhcp)) {
                continue;
            }
            bool selected = dhcp ? pending != transactions_.end() && pending->second.epoch == targetEpoch &&
                                   target->MatchesDhcpReply(transaction) :
                            group || ((nd || (owner != owners_.end() && owner->second == targetEpoch)) &&
                                      target->OwnsAddress(destination, ipv6, nd != 0));
            if (selected) {
                targets.push_back({target, targetEpoch});
            }
        }
        local = source && (dhcp || routerSolicitation || group || (targets.empty() && !onLink));
        // Gateway addresses and ND also belong to the local kernel. Unknown link-local
        // unicast is injected once so the kernel can answer its own NS/NA and ICMP.
        if (source && targets.empty() && ipv6 && onLink &&
            NearlinkIpShareTun::IsIpv6AddressUsable(destination, "sleip0")) {
            local = true;
        }
        if (source && !ipv6 && targets.empty()) {
            std::lock_guard channelLock(source->mutex_);
            local = local || V4(destination) == source->serverIp_;
        }
        if (local) {
            if (!source->IsCurrentGeneration(epoch) || tun_.Write(data, length) != 0) {
                ++dropped_;
                ReportLocked("tun-write");
                return -1;
            }
            ++local_;
        }
        if (targets.empty() && !local) {
            ++dropped_;
            ReportLocked("unknown-destination", epoch, pi);
        }
    }
    // Synchronous fanout is bounded by 32 seats. Each exit checks its epoch again.
    for (const auto &target : targets) {
        int32_t ret = target.channel->Send(data, length, source != nullptr, target.epoch);
        std::lock_guard lock(mutex_);
        if (dhcp || nd) {
            RefreshOwnersLocked(*target.channel, target.epoch);
        }
        if (ret == 0) {
            ++forwarded_;
        } else {
            ++dropped_;
        }
        ReportLocked(ret == 0 ? "deliver" : "egress", target.epoch, pi);
    }
    return 0;
}

void NearlinkIpShareLink::ReportLocked(const char *reason, uint64_t epoch, uint8_t family)
{
    auto now = std::chrono::steady_clock::now();
    if (lastReport_ != std::chrono::steady_clock::time_point{} && now - lastReport_ < std::chrono::seconds(5)) {
        return;
    }
    lastReport_ = now;
    HILOGI("[IpShare][SharedLink] generation=%{public}llu members=%{public}zu reason=%{public}s "
           "epoch=%{public}llu pi=%{public}u "
           "local=%{public}llu delivered=%{public}llu dropped=%{public}llu",
           static_cast<unsigned long long>(generation_), members_.size(), reason,
           static_cast<unsigned long long>(epoch), family,
           static_cast<unsigned long long>(local_), static_cast<unsigned long long>(forwarded_),
           static_cast<unsigned long long>(dropped_));
}
} // namespace OHOS::Nearlink
