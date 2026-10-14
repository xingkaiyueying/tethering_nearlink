#pragma once
#include "raw_address.h"
#include "ThreadUtil.h"
namespace OHOS::Nearlink {
inline bool mockSecure = true;
struct SleRemoteDeviceAdapter {
    static SleRemoteDeviceAdapter *GetInstance() { static SleRemoteDeviceAdapter a; return &a; }
    bool IsBondedFromLocal(const RawAddress &) { ++mockSecurityQueries; return mockSecure; }
    bool IsAcbConnected(const RawAddress &) { ++mockSecurityQueries; return mockSecure; }
    bool IsAcbEncrypted(const RawAddress &) { ++mockSecurityQueries; return mockSecure; }
    uint8_t GetPeerDeviceAddrType(const RawAddress &) { ++mockSecurityQueries; return 0; }
};
}
