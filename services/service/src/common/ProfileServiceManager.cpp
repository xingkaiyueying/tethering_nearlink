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
#include "ProfileServiceManager.h"

#include <algorithm>

#include "nearlink_def.h"
#include "log.h"

#include "parameters.h"
#include "SleServiceManager.h"
#include "ClassCreator.h"
#include "ProfileInfo.h"
#include "profile_list.h"
#include "ThreadUtil.h"
#include "SleServiceFfrtLog.h"
#include "nearlink_dft_ue.h"
#include "nearlink_dft_excep.h"
#include "nearlink_system_config.h"
#include "nearlink_ipshare_service.h"

namespace OHOS {
namespace Nearlink {
class ProfileServicesContextCallback : public utility::IContextCallback {
public:
    explicit ProfileServicesContextCallback(ProfileServiceManager &psm) : psm_(psm){};
    ~ProfileServicesContextCallback() = default;

    void OnEnable(const std::string &name, bool ret)
    {
        LOG_INFO("name=%{public}s, ret=%{public}d\n", name.c_str(), ret);
        psm_.OnEnable(name, ret);
    }

    void OnDisable(const std::string &name, bool ret)
    {
        LOG_INFO("name=%{public}s, ret=%{public}d\n", name.c_str(), ret);
        psm_.OnDisable(name, ret);
    }

private:
    ProfileServiceManager &psm_;
};

SleInterfaceProfileManager &SleInterfaceProfileManager::GetInstance()
{
    return ProfileServiceManager::GetInstance();
}

ProfileServiceManager &ProfileServiceManager::GetInstance()
{
    // C++11 static local variable initialization is thread-safe.
    static ProfileServiceManager instance;
    return instance;
}

void ProfileServiceManager::Initialize()
{
    GetInstance().Start();
}

void ProfileServiceManager::Uninitialize()
{
    GetInstance().Stop();
}

// ProfileServiceManager
enum class ServiceStateID {
    TURNING_ON = SleStateID::STATE_TURNING_ON,
    TURN_ON = SleStateID::STATE_TURN_ON,
    TURNING_OFF = SleStateID::STATE_TURNING_OFF,
    TURN_OFF = SleStateID::STATE_TURN_OFF,
    WAIT_TURN_ON,
};

struct ProfileServiceManager::impl {
    impl() {}

    ProfilesList<SleInterfaceProfile *> startedProfiles_ = {};
    ProfilesList<ServiceStateID> profilesState_ = {};
    std::unique_ptr<ProfileServicesContextCallback> contextCallback_ = nullptr;
    std::unordered_set<SleUuid> systemSupportProfileServices;

    SLE_DISALLOW_COPY_AND_ASSIGN(impl);
};

ProfileServiceManager::ProfileServiceManager()
    : pimpl(std::make_unique<ProfileServiceManager::impl>())
{
    // context callback create
    pimpl->contextCallback_ = std::make_unique<ProfileServicesContextCallback>(*this);
    // set system support profile services
    SetSystemSupportProfileServices();
}

ProfileServiceManager::~ProfileServiceManager()
{}

void ProfileServiceManager::Start() const
{
    LOG_INFO("start");

    if (SleServiceManager::GetInstance()->GetAdapter(SleTransport::ADAPTER_SLE)) {
        int32_t ret = NearlinkIpShareService::GetInstance().Initialize();
        if (ret != 0) {
            HILOGE("[IpShare][Profile] initialize failed during service start ret=%{public}d", ret);
        } else {
            HILOGI("[IpShare][Profile] initialize completed during service start");
        }
        CreateSleProfileServices();
    } else {
        HILOGE("[IpShare][Profile] initialize skipped: SLE adapter unavailable");
    }
}

void ProfileServiceManager::CreateSleProfileServices() const
{
    LOG_INFO("start");

    for (auto &sp : GET_CONFIG_PROFILES(SleTransport::ADAPTER_SLE)) {
        if (sp.mName == PROFILE_NAME_ICCE && !NearlinkSystemConfig::IsIcceProfileSupported()) {
            HILOGE("[ICCE Profile] not support icce profile.");
            continue;
        }
        bool isAudioSupported = NearlinkSystemConfig::IsAudioSupported();
        if (!isAudioSupported &&
            ((sp.mName == PROFILE_NAME_MCP_SERVER) || (sp.mName == PROFILE_NAME_CDSM) ||
            (sp.mName == PROFILE_NAME_ASC) || (sp.mName == PROFILE_NAME_TWS) || (sp.mName == PROFILE_NAME_VCP) ||
            (sp.mName == PROFILE_NAME_CCP) || (sp.mName == PROFILE_NAME_VAS) || (sp.mName == PROFILE_NAME_MIC))) {
            HILOGE("profile not support:%{public}s.", sp.mName.c_str());
            continue;
        }

        SleInterfaceProfile *newProfile = ClassCreator<SleInterfaceProfile>::NewInstance(sp.mName);
        if (newProfile != nullptr) {
            LOG_INFO("%{public}s", sp.mName.c_str());
            newProfile->GetContext()->RegisterCallback(*(pimpl->contextCallback_));
            pimpl->startedProfiles_.SetProfile(SleTransport::ADAPTER_SLE, sp.mName, newProfile);
            pimpl->profilesState_.SetProfile(SleTransport::ADAPTER_SLE, sp.mName, ServiceStateID::TURN_OFF);
        } else {
            LOG_ERROR("%{public}s is not registered!!!", sp.mName.c_str());
        }
    }
}

void ProfileServiceManager::Stop() const
{
    LOG_INFO("start");

    HILOGI("[IpShare][Profile] shutdown started");
    NearlinkIpShareService::GetInstance().Shutdown();

    for (auto &sp : GET_SUPPORT_PROFILES()) {
        SleInterfaceProfile *profile = nullptr;
        if (pimpl->startedProfiles_.Find(SleTransport::ADAPTER_SLB, sp.mName, profile)) {
            pimpl->startedProfiles_.SetProfile(SleTransport::ADAPTER_SLB, sp.mName, nullptr);
            delete profile;
        } else if (pimpl->startedProfiles_.Find(SleTransport::ADAPTER_SLE, sp.mName, profile)) {
            pimpl->startedProfiles_.SetProfile(SleTransport::ADAPTER_SLE, sp.mName, nullptr);
            delete profile;
        } else {
            // Nothing to do
        }
    }

    pimpl->startedProfiles_.Clear();
    pimpl->profilesState_.Clear();
    HILOGI("[IpShare][Profile] shutdown completed");
}

SleInterfaceProfile *ProfileServiceManager::GetProfileService(const std::string &name) const
{
    SleInterfaceProfile *profile = nullptr;

    if (pimpl->startedProfiles_.Find(name, profile)) {
        return profile;
    } else {
        return nullptr;
    }
}

bool ProfileServiceManager::Enable(const SleTransport transport) const
{
    LOG_INFO("transport is %{public}d", transport);

    if (IsAllEnabled(transport)) {
        LOG_DEBUG("OK");
        SleServiceManager::GetInstance()->OnProfileServicesEnableComplete(transport, true);
    } else {
        EnableProfiles(transport);
        if (IsAllEnabled(transport)) {
            LOG_DEBUG("OK");
            SleServiceManager::GetInstance()->OnProfileServicesEnableComplete(transport, true);
        }
    }
    return true;
}

void ProfileServiceManager::OnAllEnabled(const SleTransport transport) const
{
    LOG_INFO("%{public}d", transport);

    if (transport == SleTransport::ADAPTER_SLE) {
        int32_t ret = NearlinkIpShareService::GetInstance().Initialize();
        if (ret != 0) {
            HILOGE("[IpShare][Profile] reinitialize failed ret=%{public}d", ret);
        } else {
            HILOGI("[IpShare][Profile] reinitialize completed");
        }
    }

    if (pimpl->startedProfiles_.IsEmpty(transport) ||
        pimpl->profilesState_.GetProfiles(transport) == nullptr) {
        LOG_INFO("empty %{public}d", transport);
        return;
    }

    FOR_EACH_LIST(it, pimpl->profilesState_, transport)
    {
        pimpl->profilesState_.SetProfile(transport, it.first, ServiceStateID::TURN_ON);
    }
}

bool ProfileServiceManager::IsAllEnabled(const SleTransport transport) const
{
    if (pimpl->startedProfiles_.IsEmpty(transport)) {
        LOG_DEBUG("empty %{public}d", transport);
        return true;
    }

    if (pimpl->profilesState_.GetProfiles(transport) == nullptr) {
        LOG_DEBUG("empty %{public}d", transport);
        return false;
    }
    const bool any = std::any_of(pimpl->profilesState_.GetProfiles(transport)->begin(),
        pimpl->profilesState_.GetProfiles(transport)->end(),
        [](const auto &temp) -> bool { return temp.second != ServiceStateID::TURN_ON; });
    if (any) {
        LOG_DEBUG("false transport: %{public}d", transport);
        return false;
    }
    return true;
}

void ProfileServiceManager::EnableProfiles(const SleTransport transport) const
{
    if (pimpl->profilesState_.GetProfiles(transport) == nullptr) {
        LOG_INFO("empty %{public}d", transport);
        return;
    }
    FOR_EACH_LIST(it, pimpl->profilesState_, transport)
    {
        std::string name = it.first;
        SleTransport otherTransport =
            (transport == SleTransport::ADAPTER_SLB) ? SleTransport::ADAPTER_SLE : SleTransport::ADAPTER_SLB;
        ServiceStateID otherTransportState = ServiceStateID::TURN_OFF;

        if (pimpl->profilesState_.Find(otherTransport, name, otherTransportState)) {
            switch (otherTransportState) {
                case ServiceStateID::TURN_ON:
                    LOG_INFO("TURN_ON otherTransport %{public}d %{public}s", otherTransport, name.c_str());
                    pimpl->profilesState_.SetProfile(transport, name, ServiceStateID::TURN_ON);
                    break;
                case ServiceStateID::TURNING_OFF:
                    LOG_INFO("TURNING_OFF otherTransport %{public}d %{public}s", otherTransport, name.c_str());
                    pimpl->profilesState_.SetProfile(transport, name, ServiceStateID::WAIT_TURN_ON);
                    break;
                case ServiceStateID::TURNING_ON:
                    LOG_INFO("TURNING_ON otherTransport %{public}d %{public}s", otherTransport, name.c_str());
                    pimpl->profilesState_.SetProfile(transport, name, ServiceStateID::TURNING_ON);
                    break;
                default:
                    break;
            }
        }

        if (pimpl->profilesState_.Get(transport, name) == ServiceStateID::TURN_OFF) {
            pimpl->profilesState_.SetProfile(transport, name, ServiceStateID::TURNING_ON);
            LOG_INFO("transport %{public}d %{public}s enable", transport, name.c_str());
            SleInterfaceProfile *profile = nullptr;
            if (pimpl->startedProfiles_.Find(transport, name, profile)) {
                profile->GetContext()->Enable();
            } else {
                LOG_INFO("startedProfiles_ is not find");
            }
        }
    }
}

void ProfileServiceManager::OnEnable(const std::string &name, bool ret) const
{
    LOG_INFO("name=%{public}s, ret=%{public}d\n", name.c_str(), ret);
    DoInServiceManagerThread([this, name, ret]() -> void {
        EnableCompleteProcess(name, ret);
    });
}

void ProfileServiceManager::EnableCompleteProcess(const std::string &name, bool ret) const
{
    ServiceStateID newState = ret ? ServiceStateID::TURN_ON : ServiceStateID::TURN_OFF;
    std::string profileName = name;

    ServiceStateID state = ServiceStateID::TURN_OFF;
    if ((pimpl->profilesState_.Find(SleTransport::ADAPTER_SLB, profileName, state)) &&
        (state == ServiceStateID::TURNING_ON)) {
        LOG_INFO("BREDR %{public}s complete ret %{public}d", profileName.c_str(), ret);
        pimpl->profilesState_.SetProfile(SleTransport::ADAPTER_SLB, profileName, newState);
        if (!IsProfilesTurning(SleTransport::ADAPTER_SLB)) {
            EnableCompleteNotify(SleTransport::ADAPTER_SLB);
        }
    }

    if ((pimpl->profilesState_.Find(SleTransport::ADAPTER_SLE, profileName, state)) &&
        (state == ServiceStateID::TURNING_ON)) {
        LOG_INFO("profileName: %{public}s, complete ret: %{public}d", profileName.c_str(), ret);
        pimpl->profilesState_.SetProfile(SleTransport::ADAPTER_SLE, profileName, newState);
        if (!IsProfilesTurning(SleTransport::ADAPTER_SLE)) {
            EnableCompleteNotify(SleTransport::ADAPTER_SLE);
        }
    }
}

bool ProfileServiceManager::IsProfilesTurning(const SleTransport transport) const
{
    if (pimpl->startedProfiles_.IsEmpty(transport) ||
        pimpl->profilesState_.GetProfiles(transport) == nullptr) {
        LOG_INFO("empty profile, transport: %{public}d", transport);
        return false;
    }
    const bool any = std::any_of(pimpl->profilesState_.GetProfiles(transport)->begin(),
        pimpl->profilesState_.GetProfiles(transport)->end(),
        [](const auto &temp) -> bool {
            return temp.second != ServiceStateID::TURN_ON && temp.second != ServiceStateID::TURN_OFF;
        });
    if (any) {
        return true;
    }
    return false;
}

void ProfileServiceManager::EnableCompleteNotify(const SleTransport transport) const
{
    if (pimpl->profilesState_.GetProfiles(transport) == nullptr) {
        LOG_INFO("empty profile, transport: %{public}d", transport);
        return;
    }
    int turnOnProfileCount = std::count_if(pimpl->profilesState_.GetProfiles(transport)->begin(),
        pimpl->profilesState_.GetProfiles(transport)->end(),
        [](const auto &temp) -> bool { return temp.second == ServiceStateID::TURN_ON; });
    if (turnOnProfileCount == pimpl->profilesState_.Size(transport)) {
        LOG_INFO("OK transport %{public}d turnOnProfileCount %{public}d", transport, turnOnProfileCount);
        SleServiceManager::GetInstance()->OnProfileServicesEnableComplete(transport, true);
    } else {
        LOG_INFO("NG transport %{public}d turnOnProfileCount %{public}d", transport, turnOnProfileCount);
        SleServiceManager::GetInstance()->OnProfileServicesEnableComplete(transport, false);
    }
}

bool ProfileServiceManager::Disable(const SleTransport transport) const
{
    LOG_INFO("transport is %{public}d", transport);
    if (transport == SleTransport::ADAPTER_SLE) {
        NearlinkIpShareService::GetInstance().ResetForAdapterStop();
    }
    auto adapter = SleServiceManager::GetInstance()->GetAdapter(SleTransport::ADAPTER_SLE);
    if (adapter != nullptr) {
        adapter->CancelAllConnection();
    }
    if (IsAllDisabled(transport)) {
        SleServiceManager::GetInstance()->OnProfileServicesDisableComplete(transport, true);
    } else {
        DisableProfiles(transport);
        if (IsAllDisabled(transport)) {
            LOG_INFO("disableProfiles and onProfileServicesDisableComplete");
            SleServiceManager::GetInstance()->OnProfileServicesDisableComplete(transport, true);
        }
    }

    return true;
}

void ProfileServiceManager::OnAllDisabled(const SleTransport transport) const
{
    LOG_INFO("transport: %{public}d", transport);

    if (pimpl->startedProfiles_.IsEmpty(transport) ||
        pimpl->profilesState_.GetProfiles(transport) == nullptr) {
        LOG_INFO("empty profile, transport: %{public}d", transport);
        return;
    }

    FOR_EACH_LIST(it, pimpl->profilesState_, transport)
    {
        pimpl->profilesState_.SetProfile(transport, it.first, ServiceStateID::TURN_OFF);
    }
}

bool ProfileServiceManager::IsAllDisabled(const SleTransport transport) const
{
    if (pimpl->startedProfiles_.IsEmpty(transport)) {
        LOG_INFO("startedProfiles_ is empty, transport: %{public}d", transport);
        return true;
    }

    if (pimpl->profilesState_.GetProfiles(transport) == nullptr) {
        LOG_INFO("empty profile, transport: %{public}d", transport);
        return false;
    }
    const bool any = std::any_of(pimpl->profilesState_.GetProfiles(transport)->begin(),
        pimpl->profilesState_.GetProfiles(transport)->end(),
        [](const auto &temp) -> bool { return temp.second != ServiceStateID::TURN_OFF; });
    if (any) {
        return false;
    }
    return true;
}

void ProfileServiceManager::DisableProfiles(const SleTransport transport) const
{
    if (pimpl->profilesState_.GetProfiles(transport) == nullptr) {
        LOG_INFO("empty profile, transport: %{public}d", transport);
        return;
    }
    FOR_EACH_LIST(it, pimpl->profilesState_, transport)
    {
        ServiceStateID otherTransportState = ServiceStateID::TURN_OFF;
        SleTransport otherTransport =
            (transport == SleTransport::ADAPTER_SLB) ? SleTransport::ADAPTER_SLE : SleTransport::ADAPTER_SLB;
        std::string name = it.first;

        if (pimpl->profilesState_.Find(otherTransport, name, otherTransportState)) {
            switch (otherTransportState) {
                case ServiceStateID::TURN_ON:
                case ServiceStateID::TURNING_ON:
                    LOG_INFO("otherTransportState: %{public}d otherTransport: %{public}d name: %{public}s",
                        otherTransportState, otherTransport, name.c_str());
                    pimpl->profilesState_.SetProfile(transport, name, ServiceStateID::TURN_OFF);
                    break;
                default:
                    break;
            }
        }

        if (pimpl->profilesState_.Get(transport, name) == ServiceStateID::TURN_ON) {
            pimpl->profilesState_.SetProfile(transport, name, ServiceStateID::TURNING_OFF);
            LOG_INFO("transport %{public}d name: %{public}s disable", transport, name.c_str());
            SleInterfaceProfile *profile = nullptr;
            if (pimpl->startedProfiles_.Find(transport, name, profile)) {
                profile->GetContext()->Disable();
            }
        }
    }
}

void ProfileServiceManager::OnDisable(const std::string &name, bool ret) const
{
    LOG_INFO("name=%{public}s, ret=%{public}d", name.c_str(), ret);
    DoInServiceManagerThread([this, name, ret]() -> void {
        DisableCompleteProcess(name, ret);
    });
}

void ProfileServiceManager::DisableCompleteProcess(const std::string &name, bool ret) const
{
    std::string profileName = name;

    ServiceStateID state = ServiceStateID::TURN_OFF;
    if ((pimpl->profilesState_.Find(SleTransport::ADAPTER_SLB, profileName, state)) &&
        (state == ServiceStateID::TURNING_OFF)) {
        LOG_INFO("BREDR profileName %{public}s complete ret %{public}d", profileName.c_str(), ret);
        pimpl->profilesState_.SetProfile(SleTransport::ADAPTER_SLB, profileName, ServiceStateID::TURN_OFF);
        if (!IsProfilesTurning(SleTransport::ADAPTER_SLB)) {
            DisableCompleteNotify(SleTransport::ADAPTER_SLB);
        }
        CheckWaitEnableProfiles(profileName, SleTransport::ADAPTER_SLE);
    }
    if ((pimpl->profilesState_.Find(SleTransport::ADAPTER_SLE, profileName, state)) &&
        (state == ServiceStateID::TURNING_OFF)) {
        LOG_INFO("SLE profileName %{public}s complete ret %{public}d", profileName.c_str(), ret);
        pimpl->profilesState_.SetProfile(SleTransport::ADAPTER_SLE, profileName, ServiceStateID::TURN_OFF);
        if (!IsProfilesTurning(SleTransport::ADAPTER_SLE)) {
            DisableCompleteNotify(SleTransport::ADAPTER_SLE);
        }
        CheckWaitEnableProfiles(profileName, SleTransport::ADAPTER_SLB);
    }
}

void ProfileServiceManager::DisableCompleteNotify(const SleTransport transport) const
{
    if (pimpl->profilesState_.GetProfiles(transport) == nullptr) {
        LOG_INFO("empty profile, transport: %{public}d", transport);
        return;
    }
    int turnOffProfileCount = std::count_if(pimpl->profilesState_.GetProfiles(transport)->begin(),
        pimpl->profilesState_.GetProfiles(transport)->end(),
        [](const auto &temp) -> bool { return temp.second == ServiceStateID::TURN_OFF; });
    if (turnOffProfileCount == pimpl->profilesState_.Size(transport)) {
        LOG_INFO(
            "OK transport %{public}d turnOffProfileCount %{public}d", static_cast<int>(transport), turnOffProfileCount);
        SleServiceManager::GetInstance()->OnProfileServicesDisableComplete(transport, true);
    } else {
        LOG_INFO(
            "NG transport %{public}d turnOffProfileCount %{public}d", static_cast<int>(transport), turnOffProfileCount);
        SleServiceManager::GetInstance()->OnProfileServicesDisableComplete(transport, false);
    }
}

void ProfileServiceManager::CheckWaitEnableProfiles(const std::string &name, const SleTransport transport) const
{
    ServiceStateID state = ServiceStateID::TURN_OFF;
    if ((pimpl->profilesState_.Find(transport, name, state)) && (state == ServiceStateID::WAIT_TURN_ON)) {
        LOG_INFO("%{public}s ::WAIT_TURN_ON", name.c_str());
        SleInterfaceProfile *profile = nullptr;
        if (pimpl->startedProfiles_.Find(transport, name, profile)) {
            pimpl->profilesState_.SetProfile(transport, name, ServiceStateID::TURNING_ON);
            profile->GetContext()->Enable();
        }
    }
}

void ProfileServiceManager::GetProfileServicesSupportedUuids(std::vector<std::string> &uuids) const
{
    for (auto &sp : GET_SUPPORT_PROFILES()) {
        ServiceStateID state = ServiceStateID::TURN_OFF;
        if (pimpl->profilesState_.Find(SleTransport::ADAPTER_SLB, sp.mName, state) &&
            (state == ServiceStateID::TURN_ON) && (sp.mUuid != "") &&
            (std::find(uuids.begin(), uuids.end(), sp.mUuid) == uuids.end())) {
            uuids.push_back(sp.mUuid);
        }
        if (pimpl->profilesState_.Find(SleTransport::ADAPTER_SLE, sp.mName, state) &&
            (state == ServiceStateID::TURN_ON) && (sp.mUuid != "") &&
            (std::find(uuids.begin(), uuids.end(), sp.mUuid) == uuids.end())) {
            uuids.push_back(sp.mUuid);
        }
    }
}

std::vector<uint32_t> ProfileServiceManager::GetProfileServicesList() const
{
    std::vector<uint32_t> profileServicesList;
    for (auto &sp : GET_SUPPORT_PROFILES()) {
        if (pimpl->startedProfiles_.Contains(sp.mName)) {
            profileServicesList.push_back(sp.mId);
        }
    }
    return profileServicesList;
}

void ProfileServiceManager::SetSystemSupportProfileServices()
{
    // 记录手机支持业务
    pimpl->systemSupportProfileServices.insert(SleUuid::SLE_STANDARD_SERVICE_HID_UUID);
    pimpl->systemSupportProfileServices.insert(SleUuid::SLE_STANDARD_SERVICE_HID_UUID_PEN);
    pimpl->systemSupportProfileServices.insert(SleUuid::UUID_BATTERY_SERVICE);
    pimpl->systemSupportProfileServices.insert(SleUuid::UUID_BATTERY_SERVICE_PEN);
    pimpl->systemSupportProfileServices.insert(SleUuid::UUID_DEVICE_INFORMATION_SERVICE);
    pimpl->systemSupportProfileServices.insert(SleUuid::UUID_DEVICE_INFORMATION_SERVICE_PEN);
    pimpl->systemSupportProfileServices.insert(SleUuid::UUID_PORT_PROFILE_SERVICE);
    if (OHOS::system::GetBoolParameter("const.nearlink.support.icce", false)) {
        pimpl->systemSupportProfileServices.insert(SleUuid::SLE_STANDARD_SERVICE_ICCE_UUID);
    }
    /* 仅支持音频手机支持 */
    if (NearlinkSystemConfig::IsAudioSupported()) {
        pimpl->systemSupportProfileServices.insert(SleUuid::SLE_STANDARD_ASC_MGMT_UUID);
        pimpl->systemSupportProfileServices.insert(SleUuid::SLE_STANDARD_ASC_ABLTY_UUID);
        /* 合作设备集服务 */
        pimpl->systemSupportProfileServices.insert(SleUuid::SLE_STANDARD_SERVICE_CDSM_UUID);
        pimpl->systemSupportProfileServices.insert(SleUuid::SLE_STANDARD_SERVICE_VCP_UUID);
        pimpl->systemSupportProfileServices.insert(SleUuid::SLE_STANDARD_SERVICE_MIC_UUID);
    }
}

std::unordered_set<SleUuid> ProfileServiceManager::GetSystemSupportProfileServices() const
{
    return pimpl->systemSupportProfileServices;
}

SleConnectState ProfileServiceManager::GetProfileServiceConnectState(const uint32_t profileID) const
{
    std::string profileName = SupportProfilesInfo::IdToName(profileID);
    SleInterfaceProfile *profile = nullptr;
    if (!pimpl->startedProfiles_.Find(profileName, profile)) {
        return SleConnectState::DISCONNECTED;
    }

    unsigned int profileStateMask = static_cast<unsigned int>(profile->GetConnectState());
    LOG_INFO("profileStateMask is %{public}u", profileStateMask);
    if (profileStateMask & PROFILE_STATE_CONNECTED) {
        return SleConnectState::CONNECTED;
    } else if (profileStateMask & PROFILE_STATE_CONNECTING) {
        return SleConnectState::CONNECTING;
    } else if (profileStateMask & PROFILE_STATE_DISCONNECTING) {
        return SleConnectState::DISCONNECTING;
    } else {
        return SleConnectState::DISCONNECTED;
    }
}

SleConnectState ProfileServiceManager::GetProfileServicesConnectState() const
{
    unsigned int stateMask = 0;

    for (auto &sp : GET_SUPPORT_PROFILES()) {
        SleInterfaceProfile *profile = nullptr;
        if (pimpl->startedProfiles_.Find(sp.mName, profile)) {
            if (sp.mName != PROFILE_NAME_SSAP_CLIENT && sp.mName != PROFILE_NAME_SSAP_SERVER) {
                stateMask |= (unsigned int)profile->GetConnectState();
            }
        }
    }

    LOG_INFO("profileServicesStateMask is %{public}u", stateMask);
    if (stateMask & PROFILE_STATE_CONNECTED) {
        return SleConnectState::CONNECTED;
    } else if (stateMask & PROFILE_STATE_CONNECTING) {
        return SleConnectState::CONNECTING;
    } else if (stateMask & PROFILE_STATE_DISCONNECTING) {
        return SleConnectState::DISCONNECTING;
    } else {
        return SleConnectState::DISCONNECTED;
    }
}
}  // namespace Sle
}  // namespace OHOS
