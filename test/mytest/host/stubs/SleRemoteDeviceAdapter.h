#pragma once
#include <atomic>
#include "raw_address.h"
namespace OHOS::Nearlink {
inline bool mockSecure = true;
inline bool mockBonded = true, mockConnected = true, mockEncrypted = true;
inline std::atomic<unsigned> mockSecurityQueries{0};
struct SleRemoteDeviceAdapter {
    static SleRemoteDeviceAdapter *GetInstance() { static SleRemoteDeviceAdapter a; return &a; }
    bool IsBondedFromLocal(const RawAddress &) { ++mockSecurityQueries; return mockSecure && mockBonded; }
    bool IsAcbConnected(const RawAddress &) { ++mockSecurityQueries; return mockSecure && mockConnected; }
    bool IsAcbEncrypted(const RawAddress &) { ++mockSecurityQueries; return mockSecure && mockEncrypted; }
    uint8_t GetPeerDeviceAddrType(const RawAddress &) { ++mockSecurityQueries; return 0; }
};
}
