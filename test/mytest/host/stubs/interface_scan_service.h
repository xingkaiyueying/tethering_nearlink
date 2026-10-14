#pragma once
#include <cstdint>
#include <string>
#include "nearlink_def_types.h"
namespace OHOS::Nearlink {
inline uint8_t mockScanAddressType = static_cast<uint8_t>(SLE_ADDR_TYPE::SLE_ADDR_TYPE_END);
struct InterfaceScanService {
    static InterfaceScanService &GetInstance() { static InterfaceScanService s; return s; }
    uint8_t GetDeviceAddrType(const std::string &) { return mockScanAddressType; }
};
}
