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

#ifndef SSAP_DATA_H
#define SSAP_DATA_H

#include "raw_address.h"
#include "sle_uuid.h"
#include "nearlink_def_types.h"
#include "ssap_def.h"

namespace OHOS {
namespace Nearlink {
struct Descriptor {
    Descriptor() : handle_(INVALID_ENTRY_HANDLE), type_(INVALID_ENTRY_TYPE),
        writeInd_(SSAP_DESC_WRITE_IND_NONE), permission_(0) {}

    Descriptor(uint16_t handle, uint8_t type)
        : handle_(handle), type_(type), writeInd_(SSAP_DESC_WRITE_IND_NONE), permission_(0) {}

    Descriptor(uint16_t handle, uint8_t type, std::vector<uint8_t> &&value)
        : handle_(handle), type_(type), value_(std::move(value)),
          writeInd_(SSAP_DESC_WRITE_IND_NONE), permission_(0) {}

    Descriptor(uint16_t handle, uint8_t type, std::vector<uint8_t> &&value, uint16_t permission)
        : handle_(handle), type_(type), value_(value),
          writeInd_(SSAP_DESC_WRITE_IND_NONE), permission_(permission) {}

    uint16_t handle_;
    /*
     * init value = INVALID_ENTRY_TYPE
     * if this's property desc, see ssap_property_descriptor_type_t
     */
    uint8_t type_;
    std::vector<uint8_t> value_;
    // see SsapDescWriteInd
    uint8_t writeInd_;
    // see ssap_permission_bitmap_t
    uint16_t permission_;
};

struct Event {
    Event() : handle_(INVALID_ENTRY_HANDLE) {}

    Event(uint16_t handle, const Uuid &uuid)
        : handle_(handle), uuid_(uuid) {}

    Event(uint16_t handle, const Uuid &uuid, std::vector<uint8_t> &&value)
        : handle_(handle), uuid_(uuid), parameter_(std::move(value)) {}

    uint16_t handle_;
    Uuid uuid_;
    std::vector<uint8_t> parameter_;
};

struct Method  {
    Method() : handle_(INVALID_ENTRY_HANDLE), permission_(0) {}

    explicit Method(uint16_t handle)
        : handle_(handle), permission_(0) {}

    Method(uint16_t handle, const Uuid &uuid)
        : handle_(handle), uuid_(uuid), permission_(0) {}

    Method(uint16_t handle, const std::vector<uint8_t> &value)
        : handle_(handle), parameter_(value), permission_(0) {}

    Method(uint16_t handle, std::vector<uint8_t> &&value)
        : handle_(handle), result_(std::move(value)), permission_(0) {}

    Method(uint16_t handle, const Uuid &uuid, std::vector<uint8_t> &&value)
        : handle_(handle), uuid_(uuid), parameter_(std::move(value)), permission_(0) {}

    Method(uint16_t handle, const Uuid &uuid, const std::vector<uint8_t> &parameter,
        const std::vector<uint8_t> &result, uint32_t permission)
        : handle_(handle), uuid_(uuid), parameter_(parameter), result_(result), permission_(permission) {}

    uint16_t handle_;
    Uuid uuid_;
    std::vector<uint8_t> parameter_;
    std::vector<uint8_t> result_;
    uint16_t permission_;
};

struct Property {
    Property() : handle_(INVALID_ENTRY_HANDLE), opInd_(0), permission_(0) {}

    explicit Property(uint16_t handle)
        : handle_(handle), opInd_(0), permission_(0) {}

    Property(uint16_t handle, const Uuid &uuid)
        : handle_(handle), uuid_(uuid), opInd_(0), permission_(0) {}

    Property(uint16_t handle, const std::vector<uint8_t> &value)
        : handle_(handle), value_(value), opInd_(0), permission_(0) {}

    Property(uint16_t handle, std::vector<uint8_t> &&value)
        : handle_(handle), value_(std::move(value)), opInd_(0), permission_(0) {}

    Property(uint16_t handle, const Uuid &uuid, uint32_t op)
        : handle_(handle), uuid_(uuid), opInd_(op), permission_(0) {}

    Property(uint16_t handle, const Uuid &uuid, std::vector<uint8_t> &&value)
        : handle_(handle), uuid_(uuid), value_(std::move(value)), opInd_(0), permission_(0) {}

    Property(uint16_t handle, const Uuid &uuid, const std::vector<uint8_t> &value, uint32_t op, uint32_t permission)
        : handle_(handle), uuid_(uuid), value_(value), opInd_(op), permission_(permission) {}

    uint16_t handle_;
    Uuid uuid_;
    std::vector<uint8_t> value_;
    uint32_t opInd_;
    std::vector<Descriptor> descriptors_;
    uint16_t permission_;
};

struct Service {
    Service() : handle_(INVALID_ENTRY_HANDLE), startHandle_(INVALID_ENTRY_HANDLE),
        endHandle_(INVALID_ENTRY_HANDLE), isPrimary_(false), opInd_(0) {}

    explicit Service(uint16_t handle)
        : handle_(handle), startHandle_(INVALID_ENTRY_HANDLE),
          endHandle_(INVALID_ENTRY_HANDLE), isPrimary_(false), opInd_(0) {}

    Service(uint16_t handle, uint16_t startHandle, uint16_t endHandle, const Uuid &uuid)
        : handle_(handle), startHandle_(startHandle), endHandle_(endHandle),
          isPrimary_(false), uuid_(uuid), opInd_(0) {}

    Service(uint16_t handle, uint16_t startHandle, uint16_t endHandle, bool isPrimary, const Uuid &uuid)
        : handle_(handle), startHandle_(startHandle), endHandle_(endHandle),
          isPrimary_(isPrimary), uuid_(uuid), opInd_(0) {}

    uint16_t handle_;
    uint16_t startHandle_;
    uint16_t endHandle_;
    bool isPrimary_;
    Uuid uuid_;
    uint32_t opInd_ = 0;
    std::vector<Service> includeServices_;
    std::vector<Property> properties_;
    std::vector<Method> methods_;
    std::vector<Event> events_;
    std::vector<Descriptor> descriptors_;
};

struct SsapDevice {
    SsapDevice() : transport_(SSAP_TRANSPORT_INVALID) {}

    SsapDevice(const RawAddress &addr, uint8_t transport)
        : transport_(transport), addr_(addr) {}

    bool operator==(const SsapDevice& rhs) const
    {
        return ((addr_ == rhs.addr_) && (transport_ == rhs.transport_));
    }

    bool operator<(const SsapDevice& rhs) const
    {
        if (addr_ < rhs.addr_) {
            return true;
        }
        if (rhs.addr_ < addr_) {
            return false;
        }
        return transport_ < rhs.transport_;
    }

    // see SsapTransportType
    uint8_t transport_;
    RawAddress addr_;
};
} // namespace Nearlink
} // namespace OHOS
#endif // SSAP_DATA_H
