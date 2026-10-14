#pragma once
#include "SleRemoteDeviceAdapter.h"
namespace OHOS::Nearlink {
inline bool mockBonded = true, mockConnected = true, mockEncrypted = true;
inline uint8_t mockAddressType = 0;
struct SleRemoteDeviceManager {
    static SleRemoteDeviceManager *GetInstance() { static SleRemoteDeviceManager m; return &m; }
    bool IsBondedFromLocal(const RawAddress &) { return mockSecure && mockBonded; }
    bool IsAcbConnected(const RawAddress &) { return mockSecure && mockConnected; }
    bool IsAcbEncrypted(const RawAddress &) { return mockSecure && mockEncrypted; }
    uint8_t GetPeerDeviceAddrType(const RawAddress &) { return mockAddressType; }
};
}
