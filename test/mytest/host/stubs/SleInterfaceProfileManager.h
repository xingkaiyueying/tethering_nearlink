#pragma once
#include "SleInterfaceProfileCdsm.h"
namespace OHOS::Nearlink {
struct SleInterfaceProfileManager {
    static SleInterfaceProfileManager &GetInstance() { static SleInterfaceProfileManager m; return m; }
    ProfileCdsm *GetProfileService(const std::string &) { static ProfileCdsm c; return &c; }
};
}
