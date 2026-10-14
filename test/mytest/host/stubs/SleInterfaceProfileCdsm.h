#pragma once
#include <vector>
#include "raw_address.h"
namespace OHOS::Nearlink {
inline constexpr int NL_NO_ERROR = 0;
inline const std::string PROFILE_NAME_CDSM = "CdsmService";
struct NearlinkCdsmInfo { RawAddress addr_; };
inline bool mockCdsmKnown = false;
inline std::vector<NearlinkCdsmInfo> mockCdsmMembers;
struct ProfileCdsm {
    int CdsmGetAllMemberInfo(const RawAddress &, std::vector<NearlinkCdsmInfo> &members)
    { members = mockCdsmMembers; return mockCdsmKnown ? 0 : -1; }
};
}
