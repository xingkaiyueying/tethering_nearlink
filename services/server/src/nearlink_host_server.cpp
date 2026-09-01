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

#include <algorithm>
#include <atomic>
#include <dlfcn.h>
#include "nearlink_host_server.h"
#include "nearlink_hadm_client_server.h"
#include "nearlink_sle_central_manager_server.h"
#include "nearlink_sle_advertiser_server.h"
#include "nearlink_sle_controller_server.h"
#include "nearlink_ipshare_server.h"
#include "nearlink_sle_datatransfer_server.h"
#include "nearlink_ssap_client_server.h"
#include "nearlink_asc_server.h"
#include "nearlink_ssap_server_server.h"
#include "nearlink_hid_host_server.h"
#include "nearlink_tws_client_server.h"
#include "nearlink_vcp_client_server.h"
#include "nearlink_cdsm_client_server.h"
#include "nearlink_cloud_pair_server.h"
#include "SleInterfaceProfileManager.h"
#include "SleInterfaceProfileHidHost.h"
#include "log.h"
#include "nearlink_errorcode.h"
#include "system_ability_definition.h"
#include "SleInterfaceManager.h"
#include "SleInterfaceAdapterSub.h"
#include "ipc_skeleton.h"
#include "remote_observer_list.h"
#include "nearlink_utils_server.h"
#include "nearlink_utils.h"
#include "nearlink_common_event_helper.h"
#include "nearlink_permission_manager.h"
#include "nearlink_device_manager.h"
#include "nearlink_safe_map.h"
#include "nearlink_dft_manager_c.h"
#include "nearlink_dft_ue.h"
#include "nearlink_verification_manager.h"
#include "file_ex.h"
#include "parameters.h"
#include "concurrent_task_client.h"
#include "nearlink_dft_exception.h"
#include "nearlink_remote_container.h"
#include "interface_cloud_pair_service.h"
#include "ipc_skeleton.h"
#include "SleInterfaceProfileBas.h"
#include "ProfileServiceManager.h"
#include "SleInterfaceProfileDis.h"
#include "nearlink_raw_address.h"
#include "IServiceManagerPlugin.h"
#include "ManufacturerAbilityLoader.h"
#include "parameter_manager.h"
#include "nearlink_system_config.h"
#include "UnloadSa.h"
#ifdef HICOLLIE_ENABLE
#include "xcollie/watchdog.h"
#endif


namespace OHOS {
namespace Nearlink {
std::mutex g_hostInstanceMutex;

namespace {
constexpr int32_t SVC_ARGS_ONE = 1;
constexpr int32_t SVC_RET_SUCCESS = 0;
constexpr int32_t SVC_RET_FAILED = -1;
const std::string ARGS_HELP = "help";
const std::string ARGS_ENABLE = "enable";
const std::string ARGS_DISABLE = "disable";
const std::string SVC_HELP_TEXT = "svc nearlink help:\n"
                                  "svc nearlink enable: enable nearlink device\n"
                                  "svc nearlink disable: disable nearlink device\n";

const char *NEARLINK_RELOAD_SYSTEM_PARAMETER_NAME = "persist.nearlink.reload_sa";
const char *NEARLINK_RELOAD_NEEDED_FALSE = "0";
const char *NEARLINK_RELOAD_NEEDED_TRUE = "1";

static constexpr const char *NEARLINK_PLUGIN_PATH_NAME = "/system/lib64/libnearlink_server_ext_plugin.z.so";
static constexpr const char *NEARLINK_PLUGIN_FUNC_NAME = "CreatePluginServerByDlsym";
}

struct NearlinkHostServer::impl {
    impl();
    ~impl();
    bool Init();
    void Clear();
    void CheckAndReloadSa();

    /// sys state observer
    class SystemStateObserver;
    std::unique_ptr<SystemStateObserver> systemStateObserver_ = nullptr;

    /// adapter state observer
    class AdapterStateObserver;
    std::unique_ptr<AdapterStateObserver> observerImp_ = nullptr;

    /// sle observer
    class AdapterSleObserver;
    std::unique_ptr<AdapterSleObserver> sleObserverImp_ = nullptr;

    /// sle observer
    class DeviceBatteryObserver;
    std::unique_ptr<DeviceBatteryObserver> deviceBatteryObserverImp_ = nullptr;

    class DeviceRssiObserver;
    std::unique_ptr<DeviceRssiObserver> deviceRssiObserverImp_ = nullptr;

    /// sle remote device observer
    class SlePeripheralCallback;
    std::unique_ptr<SlePeripheralCallback> sleRemoteObserverImp_ = nullptr;

    /// user regist observers
    RemoteObserverList<INearlinkHostObserver> sleObservers_;

    /// user regist remote observers
    RemoteObserverList<INearlinkSlePeripheralObserver> sleRemoteObservers_;

    /// user regist battery observers
    RemoteObserverList<INearlinkDeviceBatteryObserver> deviceBatteryObservers_;

    // user regist rssi observers
    RemoteObserverList<INearlinkDeviceRssiObserver> deviceRssiObservers_;

    struct NearlinkHostRemoteInfo;
    class NearlinkHostRemoteContainer;
    // shared_ptr for deathReipient of container
    std::shared_ptr<NearlinkHostRemoteContainer> remoteContainer_ =
        std::make_shared<NearlinkHostRemoteContainer>();

    struct NearlinkBasRemoteInfo;
    class BasRemoteContainer;
    // shared_ptr for deathReipient of container
    std::shared_ptr<BasRemoteContainer> remoteBatteryContainer_ =
    std::make_shared<BasRemoteContainer>();

    std::map<std::string, sptr<IRemoteObject>> servers_;
    std::map<std::string, sptr<IRemoteObject>> sleServers_;

    // whether nearlink SA should be reloaded when service stopped
    std::atomic_bool isReloadSaNeeded_ = false;

private:
    bool createServers();
    bool createPluginServer();
    bool createAudioServers();
};

struct NearlinkHostServer::impl::NearlinkHostRemoteInfo {
    NearlinkHostRemoteInfo() : fullToken(0), isRealMac(false), uid(0)
    {}
    NearlinkHostRemoteInfo(uint64_t fullToken, bool isRealMac, int32_t uid)
        : fullToken(fullToken), isRealMac(isRealMac), uid(uid)
    {}

    uint64_t fullToken = 0;
    bool isRealMac = false;
    int32_t uid = 0;
};

struct NearlinkHostServer::impl::NearlinkBasRemoteInfo {
    NearlinkBasRemoteInfo() : fullToken(0), isRealMac(false), isSendingReq(false), uid(0)
    {}
    NearlinkBasRemoteInfo(uint64_t fullToken, bool isRealMac, bool isSendingReq, int32_t uid)
        : fullToken(fullToken), isRealMac(isRealMac), isSendingReq(isSendingReq), uid(uid)
    {}

    uint64_t fullToken = 0;
    bool isRealMac = false;
    bool isSendingReq = false;
    int32_t uid = 0;
};

class NearlinkHostServer::impl::NearlinkHostRemoteContainer final
    : public NearlinkRemoteContainer<NearlinkHostServer::impl::NearlinkHostRemoteInfo> {
public:
    NearlinkHostRemoteContainer() {}
    ~NearlinkHostRemoteContainer() override = default;
    void OnRemoteDied(const wptr<IRemoteObject> &remote) override
    {
        HILOGI("NearlinkHostRemoteContainer OnRemoteDied");
        {
            std::lock_guard<std::mutex> lk(vecMutex_);
            auto it = std::find_if(vec_.begin(), vec_.end(), [remote](const auto &obj) { return obj.first == remote; });
            NL_CHECK_RETURN(it != vec_.end(), "remote info unexpectedly not found");
            vec_.erase(it);
        }
    }
};

class NearlinkHostServer::impl::BasRemoteContainer final
    : public NearlinkRemoteContainer<NearlinkHostServer::impl::NearlinkBasRemoteInfo> {
public:
    BasRemoteContainer() {}
    ~BasRemoteContainer() override = default;
    void OnRemoteDied(const wptr<IRemoteObject> &remote) override
    {
        HILOGI("BasRemoteContainer OnRemoteDied");
        std::lock_guard<std::mutex> lk(vecMutex_);
        auto it = std::find_if(vec_.begin(), vec_.end(), [remote](const auto &obj) { return obj.first == remote; });
        NL_CHECK_RETURN(it != vec_.end(), "remote info unexpectedly not found");
        vec_.erase(it);
    }

    void UpdateRemoteInfo(const wptr<IRemoteObject> &remote, bool isSendingReq)
    {
        HILOGI("BasRemoteContainer UpdateRemoteInfo");
        std::lock_guard<std::mutex> lk(vecMutex_);
        auto it = std::find_if(vec_.begin(), vec_.end(), [remote](const auto &obj) { return obj.first == remote; });
        NL_CHECK_RETURN(it != vec_.end(), "remote info unexpectedly not found");
        it->second.isSendingReq = isSendingReq;
    }
};

class NearlinkHostServer::impl::SystemStateObserver : public ISystemStateObserver {
public:
    SystemStateObserver(NearlinkHostServer::impl *impl) : impl_(impl) {};
    ~SystemStateObserver() override = default;

    void OnSystemStateChange(const SleSystemState state) override
    {
        if (!impl_) {
            HILOGI("failed: impl_ is null");
            return;
        }
        HILOGI("[NearlinkHostServer] system state changed state=%{public}d", state);
        SleInterfaceAdapterSub *sleService = static_cast<SleInterfaceAdapterSub *>
            (SleInterfaceManager::GetInstance()->GetAdapter(SleTransport::ADAPTER_SLE));
        switch (state) {
            case SleSystemState::ON:
                if (sleService) {
                    sleService->RegisterSleAdapterObserver(
                        *reinterpret_cast<IAdapterSleObserver *>(impl_->sleObserverImp_.get()));
                    sleService->RegisterSlePeripheralCallback(
                        *reinterpret_cast<ISlePeripheralCallback *>(impl_->sleRemoteObserverImp_.get()));
                    sleService->RegisterSleDeviceRssiCallback(
                            *reinterpret_cast<ISleDeviceRssiCallback *>(impl_->deviceRssiObserverImp_.get()));
                }
                break;

            case SleSystemState::OFF:
                if (sleService) {
                    sleService->DeregisterSleAdapterObserver(
                        *reinterpret_cast<IAdapterSleObserver *>(impl_->sleObserverImp_.get()));
                    sleService->DeregisterSlePeripheralCallback(
                        *reinterpret_cast<ISlePeripheralCallback *>(impl_->sleRemoteObserverImp_.get()));
                    sleService->DeregisterSleDeviceRssiCallback(
                            *reinterpret_cast<ISleDeviceRssiCallback *>(impl_->deviceRssiObserverImp_.get()));
                }
                break;
            default:
                break;
        }
    }

private:
    NearlinkHostServer::impl *impl_ = nullptr;
};

static bool GetSimplifiedState(SleStateID &state)
{
    switch (state) {
        case SleStateID::STATE_TURNING_ON:
            return true;
        case SleStateID::STATE_TURN_ON:
            return true;
        case SleStateID::STATE_TURNING_OFF:
            return true;
        case SleStateID::STATE_TURN_OFF:
            return true;
        case SleStateID::STATE_TURNING_HALF_TO_ON:
            state = SleStateID::STATE_TURNING_ON;
            return true;
        case SleStateID::STATE_TURNING_ON_TO_HALF:
            state = SleStateID::STATE_TURNING_OFF;
            return true;
        case SleStateID::STATE_TURN_HALF:
            state = SleStateID::STATE_TURN_OFF;
            return true;
        default:
            return false;
    }
}

class NearlinkHostServer::impl::AdapterStateObserver : public IAdapterStateObserver {
public:
    AdapterStateObserver(NearlinkHostServer::impl *impl) : impl_(impl){};
    ~AdapterStateObserver() override = default;

    void OnStateChange(const SleTransport transport, const SleStateID state) override
    {
        if (state == STATE_TURN_OFF) {
            return; // 状态机进入OFF状态时不进行状态回调，等SA卸载后再上报OFF状态
        }
        HILOGI("[NearlinkHostServer] adapter state changed state=%{public}d", state);
        SleStateID reportState = state;
        if (reportState == SleStateID::STATE_TURN_ON || reportState == SleStateID::STATE_TURN_HALF) {
            impl_->sleObservers_.ForEach([this, reportState](sptr<INearlinkHostObserver> observer) {
                observer->OnSwitchStateChanged(reportState);
            });
        }
        impl_->sleObservers_.ForEach([this, transport, reportState](sptr<INearlinkHostObserver> observer) {
            observer->OnFullStateChanged(transport, reportState);
        });
        // 三态状态转换为两态上报
        if (!GetSimplifiedState(reportState)) {
            HILOGI("ignore state change");
            return;
        }
        if (reportState == lastReportState_) {
            HILOGI("state unchanged");
            return;
        }
        lastReportState_ = reportState;
        // publish event
        NearlinkHelper::NearlinkCommonEventHelper::PublishStateChangeEvent(transport, reportState);
        HILOGI("[NearlinkHostServer] report state change (%{public}d)", reportState);
        impl_->sleObservers_.ForEach([this, transport, reportState](sptr<INearlinkHostObserver> observer) {
            observer->OnStateChanged(transport, reportState);
        });
    };

    void OnDisableResponse(bool isHalfDisable, SwitchCallerInfo callerInfo) override
    {
        HILOGI("[NearlinkHostServer] isHalfDisable = %{public}d", isHalfDisable);
        impl_->isReloadSaNeeded_.store(isHalfDisable); // 半关场景，SA卸载之后需要重新加载以切换到半关
        impl_->sleObservers_.ForEach([this, isHalfDisable, callerInfo](sptr<INearlinkHostObserver> observer) {
            NearlinkHostRemoteInfo info = impl_->remoteContainer_->RetrieveRemoteInfo(observer->AsObject());
            if (info.fullToken == callerInfo.fullTokenId && info.uid == callerInfo.callerUid) {
                observer->OnDisableResponse(isHalfDisable);
                return;
            }
        });
    }

private:
    SleStateID lastReportState_ = SleStateID::STATE_TURN_HALF;
    NearlinkHostServer::impl *impl_ = nullptr;
    NEARLINK_DISALLOW_COPY_AND_ASSIGN(AdapterStateObserver);
};

class NearlinkHostServer::impl::DeviceBatteryObserver : public IDeviceBatteryCallback {
public:
    DeviceBatteryObserver(NearlinkHostServer::impl *impl) : impl_(impl){};
    ~DeviceBatteryObserver() override = default;

    void OnGetBatteryLevelEvent(const RawAddress &device, int8_t batteryLevel) override
    {
        HILOGI("device: %{public}s, state: %{public}d", GET_ENCRYPT_ADDR(device), batteryLevel);
        NearlinkRawAddress nearlinkRawAddress(device);
        impl_->deviceBatteryObservers_.ForEach(
            [this, nearlinkRawAddress, batteryLevel](sptr<INearlinkDeviceBatteryObserver> observer) {
                NearlinkBasRemoteInfo info = impl_->remoteBatteryContainer_->RetrieveRemoteInfo(observer->AsObject());
                if (info.isSendingReq) {
                    observer->OnGetBatteryLevelEvent(nearlinkRawAddress, batteryLevel);
                    impl_->remoteBatteryContainer_->UpdateRemoteInfo(observer->AsObject(), false);
                }
            });
    };

    void OnBatteryLevelChanged(const RawAddress &device, int8_t batteryLevel) override
    {
        HILOGI("device: %{public}s, state: %{public}d", GET_ENCRYPT_ADDR(device), batteryLevel);
        NearlinkRawAddress nearlinkRawAddress(device);

        impl_->deviceBatteryObservers_.ForEach(
            [this, nearlinkRawAddress, batteryLevel](sptr<INearlinkDeviceBatteryObserver> observer) {
                observer->OnBatteryLevelChanged(nearlinkRawAddress, batteryLevel);
            });
    }

private:
    NearlinkHostServer::impl *impl_ = nullptr;
    NEARLINK_DISALLOW_COPY_AND_ASSIGN(DeviceBatteryObserver);
};

class NearlinkHostServer::impl::DeviceRssiObserver : public ISleDeviceRssiCallback {
public:
    DeviceRssiObserver(NearlinkHostServer::impl *impl) : impl_(impl){};
    ~DeviceRssiObserver() override = default;

    void OnReadRemoteRssiEvent(const RawAddress &device, int32_t rssi, int32_t status) override
    {
        HILOGI("device: %{public}s, rssi: %{public}d, status: %{public}d", GET_ENCRYPT_ADDR(device), rssi, status);
        impl_->deviceRssiObservers_.ForEach([this, device, rssi, status](INearlinkDeviceRssiObserver *observer) {
            HILOGI("server hasobserver");
            NearlinkHostRemoteInfo info = impl_->remoteContainer_->RetrieveRemoteInfo(observer->AsObject());
            NL_CHECK_RETURN(NearLinkPermissionManager::VerifyPermission(MANAGE_NEARLINK, info.fullToken) &&
                                NearLinkPermissionManager::CheckSystemPermission(info.fullToken),
                "false, check permission failed");

            NearlinkRawAddress randomAddr;
            NearlinkDeviceManager::GetInstance()->ConvertToRandomAddress(info.isRealMac, device, randomAddr, false);
            observer->OnReadRemoteRssiEvent(randomAddr, rssi, status);
        });
    };

private:
    NearlinkHostServer::impl *impl_ = nullptr;
    NEARLINK_DISALLOW_COPY_AND_ASSIGN(DeviceRssiObserver);
};

class NearlinkHostServer::impl::AdapterSleObserver : public IAdapterSleObserver {
public:
    AdapterSleObserver(NearlinkHostServer::impl *impl) : impl_(impl){};
    ~AdapterSleObserver() override = default;

    void OnPairConfirmed(const SleTransport transport, const RawAddress &device, const int32_t reqType,
        const int32_t number) override
    {
        HILOGI("device: %{public}s, reqType: %{public}d, number: %{public}d",
            GET_ENCRYPT_ADDR(device), reqType, number);
        impl_->sleObservers_.ForEach([this, transport, device, reqType, number](INearlinkHostObserver *observer) {
            NearlinkHostRemoteInfo info = impl_->remoteContainer_->RetrieveRemoteInfo(observer->AsObject());
            NL_CHECK_RETURN(NearLinkPermissionManager::VerifyPermission(ACCESS_NEARLINK, info.fullToken),
                "false, check permission failed");
            NearlinkRawAddress randomAddr;
            NearlinkDeviceManager::GetInstance()->ConvertToRandomAddress(info.isRealMac, device, randomAddr, false);
            observer->OnPairConfirmed(transport, randomAddr, reqType, number);
        });
    }

    void OnDeviceNameChanged(const std::string deviceName) override
    {
        HILOGI("deviceName length: %{public}lu", deviceName.length());
        impl_->sleObservers_.ForEach([this, deviceName](INearlinkHostObserver *observer) {
            NearlinkHostRemoteInfo info = impl_->remoteContainer_->RetrieveRemoteInfo(observer->AsObject());
            NL_CHECK_RETURN(NearLinkPermissionManager::VerifyPermission(MANAGE_NEARLINK, info.fullToken) &&
                NearLinkPermissionManager::CheckSystemPermission(info.fullToken), "false, check permission failed");
            observer->OnDeviceNameChanged(deviceName);
        });
    }

    void OnDeviceAddrChanged(const std::string address) override
    {
        HILOGI("address: %{public}s", GetEncryptAddr(address).c_str());
        impl_->sleObservers_.ForEach([this, address](INearlinkHostObserver *observer) {
            NearlinkHostRemoteInfo info = impl_->remoteContainer_->RetrieveRemoteInfo(observer->AsObject());
            NL_CHECK_RETURN(NearLinkPermissionManager::VerifyPermission(MANAGE_NEARLINK, info.fullToken) &&
                NearLinkPermissionManager::CheckSystemPermission(info.fullToken), "false, check permission failed");
            observer->OnDeviceAddrChanged(address);
        });
    }

private:
    NearlinkHostServer::impl *impl_ = nullptr;
    NEARLINK_DISALLOW_COPY_AND_ASSIGN(AdapterSleObserver);
};

class NearlinkHostServer::impl::SlePeripheralCallback : public ISlePeripheralCallback {
public:
    SlePeripheralCallback(NearlinkHostServer::impl *impl) : impl_(impl) {};
    ~SlePeripheralCallback() override = default;

    void OnPairingRequest(const RawAddress &device, const std::string &passkey, int32_t type) override
    {
        HILOGI("device: %{public}s", GET_ENCRYPT_ADDR(device));
        impl_->sleRemoteObservers_.ForEach([this, device, passkey, type](INearlinkSlePeripheralObserver *observer) {
            NearlinkHostRemoteInfo info = impl_->remoteContainer_->RetrieveRemoteInfo(observer->AsObject());
            NL_CHECK_RETURN(NearLinkPermissionManager::VerifyPermission(ACCESS_NEARLINK, info.fullToken) &&
                NearLinkPermissionManager::CheckSystemPermission(info.fullToken), "false, check permission failed");

            NearlinkRawAddress addr;
            NearlinkDeviceManager::GetInstance()->ConvertToRandomAddress(info.isRealMac, device, addr, false);
            observer->OnPairingRequest(addr, passkey, type);
        });
    }

    void OnPairStatusChanged(const RawAddress &device, int32_t preStatus, int32_t status, int32_t reason) override
    {
        HILOGI("device: %{public}s, status: %{public}d", GET_ENCRYPT_ADDR(device), status);
        NearlinkDeviceManager::GetInstance()->UpdateRandomAddressMap(device, status);

        impl_->sleRemoteObservers_.ForEach([this, device, preStatus, status, reason](
            INearlinkSlePeripheralObserver *observer) {
            NearlinkHostRemoteInfo info = impl_->remoteContainer_->RetrieveRemoteInfo(observer->AsObject());
            NL_CHECK_RETURN(NearLinkPermissionManager::VerifyPermission(ACCESS_NEARLINK, info.fullToken),
                "false, check permission failed");

            NearlinkRawAddress randomAddr;
            NearlinkDeviceManager::GetInstance()->ConvertToRandomAddress(info.isRealMac, device, randomAddr, false);
            observer->OnPairStatusChanged(randomAddr, preStatus, status, reason);
        });
    }

    void OnCsdmPairStateChanged(const RawAddress &device, int32_t state) override
    {
        HILOGI("device: %{public}s, state: %{public}d", GET_ENCRYPT_ADDR(device), state);
        NearlinkDeviceManager::GetInstance()->UpdateRandomAddressMap(device, state);
    }

    void OnAcbStateChanged(const RawAddress &device, int32_t state, int reason) override
    {
        HILOGI("device: %{public}s, state: %{public}d, reason: 0x%{public}x", GET_ENCRYPT_ADDR(device), state, reason);
        impl_->sleRemoteObservers_.ForEach([this, device, state, reason
            ](INearlinkSlePeripheralObserver *observer) {
            NearlinkHostRemoteInfo info = impl_->remoteContainer_->RetrieveRemoteInfo(observer->AsObject());
            NL_CHECK_RETURN(NearLinkPermissionManager::VerifyPermission(ACCESS_NEARLINK, info.fullToken),
                "false, check permission failed");

            NearlinkRawAddress randomAddr;
            NearlinkDeviceManager::GetInstance()->ConvertToRandomAddress(info.isRealMac, device, randomAddr, false);
            observer->OnAcbStateChanged(randomAddr, state, reason);
        });
    }

    void OnConnectionStateChanged(
        const RawAddress &device, int32_t state, int32_t preState, int reason) override
    {
        if (reason == static_cast<int>(SleConnectReason::CONNECT_NONE)) {
            // 只有断连时reason才可能为CONNECT_NONE（无效原因），统一以CONNECT_FAIL上报
            reason = static_cast<int>(SleConnectReason::CONNECT_FAIL);
        }

        HILOGD("device: %{public}s, state: %{public}d, reason: %{public}d", GET_ENCRYPT_ADDR(device), state, reason);

        impl_->sleRemoteObservers_.ForEach([this, device, preState, state, reason
            ](INearlinkSlePeripheralObserver *observer) {
            NearlinkHostRemoteInfo info = impl_->remoteContainer_->RetrieveRemoteInfo(observer->AsObject());
            NL_CHECK_RETURN(NearLinkPermissionManager::VerifyPermission(ACCESS_NEARLINK, info.fullToken),
                "false, check permission failed");

            NearlinkRawAddress randomAddr;
            NearlinkDeviceManager::GetInstance()->ConvertToRandomAddress(info.isRealMac, device, randomAddr, false);
            observer->OnConnectionStateChanged(randomAddr, preState, state, reason);
        });
    }

    void OnLinkFreqBandChanged(const RawAddress &device, const int32_t freqBand) override
    {
        HILOGI("device: %{public}s, freqBand: %{public}d", GET_ENCRYPT_ADDR(device), freqBand);
        impl_->sleRemoteObservers_.ForEach([this, device, freqBand](INearlinkSlePeripheralObserver *observer) {
            NearlinkHostRemoteInfo info = impl_->remoteContainer_->RetrieveRemoteInfo(observer->AsObject());
            NL_CHECK_RETURN(NearLinkPermissionManager::VerifyPermission(MANAGE_NEARLINK, info.fullToken) &&
                NearLinkPermissionManager::CheckSystemPermission(info.fullToken), "false, check permission failed");

            NearlinkRawAddress randomAddr;
            NearlinkDeviceManager::GetInstance()->ConvertToRandomAddress(info.isRealMac, device, randomAddr, false);
            observer->OnLinkFreqBandChanged(randomAddr, freqBand);
        });
    }

private:
    NearlinkHostServer::impl *impl_ = nullptr;
    NEARLINK_DISALLOW_COPY_AND_ASSIGN(SlePeripheralCallback);
};
sptr<NearlinkHostServer> NearlinkHostServer::instance_;

const bool REGISTER_RESULT = SystemAbility::MakeAndRegisterAbility(NearlinkHostServer::GetInstance().GetRefPtr());

NearlinkHostServer::impl::impl()
{
    HILOGI("starts");
    systemStateObserver_ = std::make_unique<SystemStateObserver>(this);
    observerImp_ = std::make_unique<AdapterStateObserver>(this);
    sleObserverImp_ = std::make_unique<AdapterSleObserver>(this);
    deviceBatteryObserverImp_ = std::make_unique<DeviceBatteryObserver>(this);
    deviceRssiObserverImp_ = std::make_unique<DeviceRssiObserver>(this);
    sleRemoteObserverImp_ = std::make_unique<SlePeripheralCallback>(this);
    remoteContainer_->Init();
    remoteBatteryContainer_->Init();
}

NearlinkHostServer::impl::~impl()
{
    HILOGW("NearlinkHostServer ~impl()");
}

bool NearlinkHostServer::impl::Init()
{
    HILOGI("start host server init");
    std::unordered_map<std::string, std::string> payload;
    payload["pid"] = std::to_string(getpid()); // 这里需要将pid的输入转化为string类型
    OHOS::ConcurrentTask::ConcurrentTaskClient::GetInstance().RequestAuth(payload); // 向concurrent_task服务申请对自己进程鉴权

    SleInterfaceAdapterSub *sleService = static_cast<SleInterfaceAdapterSub *>
        (SleInterfaceManager::GetInstance()->GetAdapter(SleTransport::ADAPTER_SLE));

    SleInterfaceManager::GetInstance()->RegisterSystemStateObserver(*systemStateObserver_);

    SleInterfaceManager::GetInstance()->Start();
    SleInterfaceManager::GetInstance()->RegisterStateObserver(*observerImp_);

    if (sleService) {
        sleService->RegisterSleAdapterObserver(*sleObserverImp_);
        sleService->RegisterSlePeripheralCallback(*sleRemoteObserverImp_);
        sleService->RegisterSleDeviceRssiCallback(*deviceRssiObserverImp_);
    }

    ProfileBas *basService = static_cast<ProfileBas *>(ProfileBas::GetInstance());
    if (basService) {
        basService->RegisterDeviceObserver(*deviceBatteryObserverImp_);
    }

    NearlinkDeviceManager::GetInstance()->RecoverRetainedDeviceInfo();
    if (!createServers()) {
        return false;
    }
    return true;
}

void NearlinkHostServer::impl::Clear()
{
    /// systerm state observer
    SleInterfaceManager::GetInstance()->DeregisterSystemStateObserver(*systemStateObserver_);

    /// adapter state observer
    SleInterfaceManager::GetInstance()->Stop();
    SleInterfaceManager::GetInstance()->DeregisterStateObserver(*observerImp_);
    SleInterfaceAdapterSub *sleService = static_cast<SleInterfaceAdapterSub *>
        (SleInterfaceManager::GetInstance()->GetAdapter(SleTransport::ADAPTER_SLE));
    if (sleService) {
        sleService->DeregisterSleAdapterObserver(*sleObserverImp_.get());
        sleService->DeregisterSlePeripheralCallback(*sleRemoteObserverImp_.get());
        sleService->DeregisterSleDeviceRssiCallback(*deviceRssiObserverImp_.get());
    }

    ProfileBas *basService = static_cast<ProfileBas *>(ProfileBas::GetInstance());
    if (basService) {
        basService->DeregisterDeviceObserver(*deviceBatteryObserverImp_.get());
    }
}

void NearlinkHostServer::impl::CheckAndReloadSa()
{
    if (!isReloadSaNeeded_.load()) {
        return;
    }
    // SAMGR已开始卸载星闪SA，此时通过系统参数触发SA加载，SA会在卸载完成后重新加载
    HILOGI("set nearlink reload_sa parameter, reload nearlink SA");
    bool ret = OHOS::system::SetParameter(NEARLINK_RELOAD_SYSTEM_PARAMETER_NAME, NEARLINK_RELOAD_NEEDED_TRUE);
    NL_CHECK_RETURN(ret, "failed to set nearlink reload_sa parameter.");
}

bool NearlinkHostServer::impl::createServers()
{
    HILOGI("starts");

    sptr<NearlinkSleAdvertiserServer> sleAdvertiser = new (std::nothrow) NearlinkSleAdvertiserServer();
    NL_CHECK_RETURN_RET(sleAdvertiser, false, "sleAdvertiser is nullptr");
    sleServers_[SLE_ADVERTISER_SERVER] = sleAdvertiser->AsObject();

    sptr<NearlinkSleCentralManagerServer> sleCentralManger = new (std::nothrow) NearlinkSleCentralManagerServer();
    NL_CHECK_RETURN_RET(sleCentralManger, false, "sleCentralManger is nullptr");
    sleServers_[SLE_CENTRAL_MANAGER_SERVER] = sleCentralManger->AsObject();

    sptr<NearlinkCloudPairServer> nearlinkCloudPair = new (std::nothrow) NearlinkCloudPairServer();
    NL_CHECK_RETURN_RET(nearlinkCloudPair, false, "nearlinkCloudPair is nullptr");
    sleServers_[CLOUD_PAIR_SERVER] = nearlinkCloudPair->AsObject();

    sptr<NearlinkSsapClientServer> sleSsapClient = new (std::nothrow) NearlinkSsapClientServer();
    NL_CHECK_RETURN_RET(sleSsapClient, false, "sleSsapClient is nullptr");
    sleServers_[PROFILE_SSAP_CLIENT] = sleSsapClient->AsObject();

    sptr<NearlinkSsapServerServer> ssapServer = new (std::nothrow) NearlinkSsapServerServer();
    NL_CHECK_RETURN_RET(ssapServer, false, "ssapServer is nullptr");
    sleServers_[PROFILE_SSAP_SERVER] = ssapServer->AsObject();

    sptr<NearlinkHadmClientServer> hadmClientServer = new (std::nothrow) NearlinkHadmClientServer();
    NL_CHECK_RETURN_RET(hadmClientServer, false, "hadmClientServer is nullptr");
    sleServers_[NEARLINK_HADM_CLIENT_SERVER] = hadmClientServer->AsObject();

    sptr<NearlinkHidHostServer> hidHostServer = new (std::nothrow) NearlinkHidHostServer();
    NL_CHECK_RETURN_RET(hidHostServer, false, "hidHostServer is nullptr");
    sleServers_[PROFILE_HID_HOST_SERVER] = hidHostServer->AsObject();

    sptr<NearlinkSleDataTransferServer> sleDatatransfer = new (std::nothrow) NearlinkSleDataTransferServer();
    NL_CHECK_RETURN_RET(sleDatatransfer, false, "sleDatatransfer is nullptr");
    sleServers_[SLE_DATATRANSFER_SERVER] = sleDatatransfer->AsObject();

    sptr<NearlinkSleControllerServer> sleControllerServer = new (std::nothrow) NearlinkSleControllerServer();
    NL_CHECK_RETURN_RET(sleControllerServer, false, "sleControllerServer is nullptr");
    sleServers_[NEARLINK_SLE_CONTROLLER_SERVER] = sleControllerServer->AsObject();

    sptr<NearlinkIpShareServer> ipShareServer = new (std::nothrow) NearlinkIpShareServer();
    NL_CHECK_RETURN_RET(ipShareServer, false, "ipShareServer is nullptr");
    sleServers_[PROFILE_IPSHARE_SERVER] = ipShareServer->AsObject();
    HILOGI("[IpShare][Host] IP share IPC server registered profile=%{public}d", PROFILE_IPSHARE_SERVER);

    if (!createPluginServer()) {
        return false;
    }

    if (NearlinkSystemConfig::IsAudioSupported() && !createAudioServers()) {
        return false;
    }

    HILOGI("sleServers_ constructed, size is %{public}zu;", sleServers_.size());
    return true;
}

bool NearlinkHostServer::impl::createPluginServer()
{
    HILOGI("starts");

    void *handle = dlopen(NEARLINK_PLUGIN_PATH_NAME, RTLD_NOW);
    if (handle == nullptr) {
        HILOGW("could not find ext file, dlopen failed: %{public}s", dlerror());
        return true;
    }

    typedef bool (*CreatePluginServerFunc)(std::map<std::string, sptr<IRemoteObject>>&);
    CreatePluginServerFunc createPluginServerByDlsym = (CreatePluginServerFunc)dlsym(handle, NEARLINK_PLUGIN_FUNC_NAME);
    if (createPluginServerByDlsym == nullptr) {
        HILOGE("dlsym failed: %{public}s", dlerror());
        dlclose(handle);
        return false;
    }

    bool ret = createPluginServerByDlsym(sleServers_);

    ManufacturerAbilityLoader::GetInstance().Load();
    ParameterManager::LoadProvider();

    return ret;
}

bool NearlinkHostServer::impl::createAudioServers()
{
    HILOGI("starts");

    sptr<NearlinkASCServer> audioServer = new (std::nothrow) NearlinkASCServer();
    NL_CHECK_RETURN_RET(audioServer, false, "audioServer is nullptr");
    sleServers_[PROFILE_ASC] = audioServer->AsObject();

    sptr<NearlinkVcpClientServer> vcpClientServer = new (std::nothrow) NearlinkVcpClientServer();
    NL_CHECK_RETURN_RET(vcpClientServer, false, "vcpClientServer is nullptr");
    sleServers_[PROFILE_VCP_SERVER] = vcpClientServer->AsObject();

    sptr<NearlinkTwsClientServer> twsClientServer = new (std::nothrow) NearlinkTwsClientServer();
    NL_CHECK_RETURN_RET(twsClientServer, false, "twsClientServer is nullptr");
    sleServers_[NEARLINK_TWS_CLIENT_SERVER] = twsClientServer->AsObject();

    sptr<NearlinkCdsmClientServer> cdsmClientServer = new (std::nothrow) NearlinkCdsmClientServer();
    NL_CHECK_RETURN_RET(cdsmClientServer, false, "cdsmClientServer is nullptr");
    sleServers_[PROFILE_CDSM_CLIENT_SERVER] = cdsmClientServer->AsObject();

    return true;
}

NearlinkHostServer::NearlinkHostServer() : SystemAbility(NEARLINK_HOST_SYS_ABILITY_ID, true)
{
    HILOGI("NearlinkHostServer called.");
    pimpl = std::make_unique<impl>();
}

NearlinkHostServer::~NearlinkHostServer()
{
    HILOGW("~NearlinkHostServer called.");
}

sptr<NearlinkHostServer> NearlinkHostServer::GetInstance()
{
    std::lock_guard<std::mutex> lock(g_hostInstanceMutex);
    if (instance_ == nullptr) {
        sptr<NearlinkHostServer> temp = new (std::nothrow) NearlinkHostServer();
        if (temp != nullptr) {
            instance_ = temp;
        }
    }
    return instance_;
}

void NearlinkHostServer::OnStart()
{
    HILOGI("starting service.");
    bool parameterRet = OHOS::system::SetParameter(NEARLINK_RELOAD_SYSTEM_PARAMETER_NAME, NEARLINK_RELOAD_NEEDED_FALSE);
    if (!parameterRet) {
        HILOGE("failed to reset nearlink reload_sa parameter, errCode(%{public}d)", parameterRet);
    }

    DftManagerStart();
    if (state_ == ServiceRunningState::STATE_RUNNING) {
        HILOGI("service is already started.");
        return;
    }

    if (!Init()) {
        HILOGE("init fail");
        OnStop();
        return;
    }

    state_ = ServiceRunningState::STATE_RUNNING;

    HILOGI("Service has been started successfully");
    return;
}

bool NearlinkHostServer::Init()
{
#ifdef HICOLLIE_ENABLE
    HiviewDFX::Watchdog::GetInstance().InitFfrtWatchdog();
#endif
    if (!pimpl->Init()) {
        HILOGE("pimpl init failed!");
        return false;
    }
    if (!PublishHostServer()) {
        HILOGE("init publish failed!");
        return false;
    }
    HILOGI("init success");
    return true;
}

bool __attribute__((weak)) NearlinkHostServer::PublishHostServer()
{
    HILOGI("Publish nearlink service started.");
    return Publish(NearlinkHostServer::GetInstance());
}

int32_t NearlinkHostServer::OnIdle(const SystemAbilityOnDemandReason &idleReason)
{
    HILOGI("OnIdle service.");
    UnloadSa::GetInstance().NearlinkHostExtOnIdle();
    return 0;
}

void NearlinkHostServer::OnStop()
{
    HILOGI("stopping service.");
    pimpl->CheckAndReloadSa();
    DftManagerStop();

    pimpl->Clear();
    state_ = ServiceRunningState::STATE_IDLE;
    return;
}

NlErrCode NearlinkHostServer::GetProfile(const std::string &name, sptr<IRemoteObject> &remoteProfile)
{
    HILOGD("seraching %{public}s ", name.c_str());
    auto it = pimpl->sleServers_.find(name);
    if (it != pimpl->sleServers_.end()) {
        HILOGD("server serached %{public}s ", name.c_str());
        remoteProfile = pimpl->sleServers_[name];
    }
    return NL_NO_ERROR;
}

NlErrCode NearlinkHostServer::GetLocalName(std::string &name)
{
    HILOGI("Enter!");
    NL_CHECK_RETURN_RET(IsSleAvailableToCallerInner(), NL_ERR_SLE_OFF, "nearlink is off.");
    SleInterfaceAdapterSub *sleService = static_cast<SleInterfaceAdapterSub *>
        (SleInterfaceManager::GetInstance()->GetAdapter(SleTransport::ADAPTER_SLE));
    NL_CHECK_RETURN_RET(sleService, NL_ERR_INTERNAL_ERROR, "sleService invalid.");
    name = sleService->GetLocalName();
    return NL_NO_ERROR;
}

NlErrCode NearlinkHostServer::SetLocalName(const std::string &name)
{
    NL_CHECK_RETURN_RET(IsSleAvailableToCallerInner(), NL_ERR_SLE_OFF, "nearlink is off.");
    SleInterfaceAdapterSub *sleService = static_cast<SleInterfaceAdapterSub *>
        (SleInterfaceManager::GetInstance()->GetAdapter(SleTransport::ADAPTER_SLE));
    NL_CHECK_RETURN_RET(sleService, NL_ERR_INTERNAL_ERROR, "sleService invalid.");
    if (sleService->SetLocalName(name)) {
        return NL_NO_ERROR;
    }
    return NL_ERR_INTERNAL_ERROR;
}

NlErrCode NearlinkHostServer::GetLocalAddress(std::string &addr)
{
    HILOGD("Enter!");
    NL_CHECK_RETURN_RET(IsSleAvailableToCallerInner(), NL_ERR_SLE_OFF, "nearlink is off.");
    SleInterfaceAdapterSub *sleService = static_cast<SleInterfaceAdapterSub *>
        (SleInterfaceManager::GetInstance()->GetAdapter(SleTransport::ADAPTER_SLE));
    NL_CHECK_RETURN_RET(sleService, NL_ERR_INTERNAL_ERROR, "sleService invalid.");
    addr = sleService->GetLocalAddress();
    return NL_NO_ERROR;
}

inline bool IsNearlinkEdmDisallowed()
{
    const std::string nlEdmKey = "persist.edm.near_link_disallowed";
    //  Returns the number of bytes of the system parameter if the operation is successful.
    bool ret = OHOS::system::GetBoolParameter(nlEdmKey, false);
    if (ret) {
        HILOGW("nearlink is prohibited by EDM. You won't be able to turn on nearlink!");
    }
    return ret;
}

NlErrCode NearlinkHostServer::EnableSle(const SleAutoConnectPolicy autoConnPolicy)
{
    HILOGD("Enter!");
    // if nearlink is disallowed by pc edm, don't enable.
    NL_CHECK_RETURN_RET(!IsNearlinkEdmDisallowed(), NL_ERR_PROHIBITED_BY_EDM, "nearlink pc edm disallowed");

    NL_CHECK_RETURN_RET(SleInterfaceManager::GetInstance() != nullptr, NL_ERR_INTERNAL_ERROR,
        "GetInstance is nullptr!");
    SwitchCallerInfo callerInfo = SleInterfaceManager::GetCallerInfo();
    int32_t ret = SleInterfaceManager::GetInstance()->Enable(SleTransport::ADAPTER_SLE,
        SleEventType::INTERFACE_TRIGGERED, callerInfo, autoConnPolicy);
    return static_cast<NlErrCode>(ret);
}

NlErrCode NearlinkHostServer::DisableSle()
{
    HILOGI("Enter!");
    SwitchCallerInfo callerInfo = SleInterfaceManager::GetCallerInfo();
    int32_t ret = SleInterfaceManager::GetInstance()->Disable(
        SleTransport::ADAPTER_SLE, SleEventType::INTERFACE_TRIGGERED, callerInfo);
    return static_cast<NlErrCode>(ret);
}

NlErrCode NearlinkHostServer::DisableSleToOff()
{
    HILOGI("Enter!");
    SwitchCallerInfo callerInfo = SleInterfaceManager::GetCallerInfo();
    int32_t ret = SleInterfaceManager::GetInstance()->DisableToOff(
        SleTransport::ADAPTER_SLE, SleEventType::INTERFACE_TRIGGERED, callerInfo);
    return static_cast<NlErrCode>(ret);
}

NlErrCode NearlinkHostServer::EnableSleToHalf()
{
    HILOGI("Enter!");
    SwitchCallerInfo callerInfo = SleInterfaceManager::GetCallerInfo();
    int32_t ret = SleInterfaceManager::GetInstance()->EnableToHalf(
        SleTransport::ADAPTER_SLE, SleEventType::INTERFACE_TRIGGERED, callerInfo);
    return static_cast<NlErrCode>(ret);
}

NlErrCode NearlinkHostServer::GetSleFullState(int &sleCurrentState)
{
    SleStateID sleState = SleInterfaceManager::GetInstance()->GetState(SleTransport::ADAPTER_SLE);
    sleCurrentState = static_cast<int>(sleState);
    return NL_NO_ERROR;
}

NlErrCode NearlinkHostServer::DisableSleForFactoryReset()
{
    HILOGI("Enter!");
    SwitchCallerInfo callerInfo = SleInterfaceManager::GetCallerInfo();
    NlErrCode ret = static_cast<NlErrCode>(SleInterfaceManager::GetInstance()->Disable(
        SleTransport::ADAPTER_SLE, SleEventType::SYS_FACTORY_RESET_TRIGGERED, callerInfo));
    if (ret == NL_NO_ERROR || ret == NL_ERR_INVALID_SWITCH_OPERATION) {
        return NL_NO_ERROR;
    }
    return NL_ERR_INTERNAL_ERROR;
}

NlErrCode __attribute__((weak)) NearlinkHostServer::IsSleEnabled(bool &isSleEnabled)
{
    if (SleInterfaceManager::GetInstance()->GetState(SleTransport::ADAPTER_SLE) == SleStateID::STATE_TURN_ON) {
        isSleEnabled = true;
    } else {
        isSleEnabled = false;
    }
    HILOGD("isSleEnabled(%{public}d)", isSleEnabled);
    return NL_NO_ERROR;
}

bool NearlinkHostServer::IsSleEnabledInner()
{
    bool isSleEnabled = false;
    NlErrCode status = IsSleEnabled(isSleEnabled);
    return status == NL_NO_ERROR && isSleEnabled;
}

NlErrCode NearlinkHostServer::IsSleHalfDisabled(bool &isSleHalfDisabled)
{
    if (SleInterfaceManager::GetInstance()->GetState(SleTransport::ADAPTER_SLE) == SleStateID::STATE_TURN_HALF) {
        isSleHalfDisabled = true;
    } else {
        isSleHalfDisabled = false;
    }
    HILOGI("isSleHalfDisabled(%{public}d)", isSleHalfDisabled);
    return NL_NO_ERROR;
}

bool NearlinkHostServer::IsSleHalfDisabledInner()
{
    bool isSleHalfDisabled = false;
    NlErrCode status = IsSleHalfDisabled(isSleHalfDisabled);
    return status == NL_NO_ERROR && isSleHalfDisabled;
}

NlErrCode NearlinkHostServer::IsSleDisabled(bool &isSleDisabled)
{
    if (SleInterfaceManager::GetInstance()->GetState(SleTransport::ADAPTER_SLE) == SleStateID::STATE_TURN_OFF) {
        isSleDisabled = true;
    } else {
        isSleDisabled = false;
    }
    HILOGI("isSleDisabled(%{public}d)", isSleDisabled);
    return NL_NO_ERROR;
}

NlErrCode __attribute__((weak)) NearlinkHostServer::IsSleAvailableToCaller(bool &isSleAvailable)
{
    SleStateID sleState = SleInterfaceManager::GetInstance()->GetState(SleTransport::ADAPTER_SLE);
    if (sleState == SleStateID::STATE_TURN_ON) {
        isSleAvailable = true;
    } else if (NearLinkPermissionManager::IsNativeCaller() && sleState == SleStateID::STATE_TURN_HALF) {
        HILOGD("native caller");
        isSleAvailable = true;
    } else {
        isSleAvailable = false;
    }
    HILOGD("isSleAvailable(%{public}d)", isSleAvailable);
    return NL_NO_ERROR;
}

bool NearlinkHostServer::IsSleAvailableToCallerInner()
{
    bool isSleAvailable = false;
    NlErrCode status = IsSleAvailableToCaller(isSleAvailable);
    return status == NL_NO_ERROR && isSleAvailable;
}

NlErrCode NearlinkHostServer::GetAcbState(const std::string &address, int32_t &acbState)
{
    std::string realAddr = "";
    NearlinkDeviceManager::GetInstance()->GetDeviceRealAddr(address, realAddr);
    HILOGI("address: %{public}s", GetEncryptAddr(realAddr).c_str());
    NL_CHECK_RETURN_RET(IsSleAvailableToCallerInner(), NL_ERR_SLE_OFF, "nearlink is off.");
    RawAddress addr(realAddr);
    SleInterfaceAdapterSub *sleService = static_cast<SleInterfaceAdapterSub *>
        (SleInterfaceManager::GetInstance()->GetAdapter(SleTransport::ADAPTER_SLE));
    NL_CHECK_RETURN_RET(sleService, NL_ERR_INTERNAL_ERROR, "sleService invalid.");
    acbState = sleService->GetAcbState(addr);
    return NL_NO_ERROR;
}

NlErrCode NearlinkHostServer::GetAdapterConnectState(int32_t &state)
{
    NL_CHECK_RETURN_RET(IsSleAvailableToCallerInner(), NL_ERR_SLE_OFF, "nearlink is off.");
    state = static_cast<int32_t>(SleInterfaceManager::GetInstance()->GetAdapterConnectState());
    HILOGI("state(%{public}d)", state);
    return NL_NO_ERROR;
}

NlErrCode NearlinkHostServer::GetProfileConnState(const std::string &remoteAddr, int32_t &state)
{
    NL_CHECK_RETURN_RET(IsSleAvailableToCallerInner(), NL_ERR_SLE_OFF, "nearlink is off.");
    SleInterfaceAdapterSub *sleService = static_cast<SleInterfaceAdapterSub *>
        (SleInterfaceManager::GetInstance()->GetAdapter(SleTransport::ADAPTER_SLE));
    NL_CHECK_RETURN_RET(sleService, NL_ERR_INTERNAL_ERROR, "sleService invalid.");
    std::string realAddr = "";
    NearlinkDeviceManager::GetInstance()->GetDeviceRealAddr(remoteAddr, realAddr);
    RawAddress addr(realAddr);
    state = sleService->GetProfileConnState(addr);
    HILOGD("state(%{public}d)", state);
    return NL_NO_ERROR;
}

NlErrCode NearlinkHostServer::GetPairedDevices(std::vector<NearlinkRawAddress> &pairedAddr)
{
    NL_CHECK_RETURN_RET(IsSleEnabledInner(), NL_ERR_SLE_OFF, "nearlink is off.");
    SleInterfaceAdapterSub *sleService = static_cast<SleInterfaceAdapterSub *>
        (SleInterfaceManager::GetInstance()->GetAdapter(SleTransport::ADAPTER_SLE));
    NL_CHECK_RETURN_RET(sleService, NL_ERR_INTERNAL_ERROR, "sleService invalid.");
    std::vector<RawAddress> rawAddrVec = sleService->GetPairedDevices();
    for (auto it = rawAddrVec.begin(); it != rawAddrVec.end(); ++it) {
        NearlinkRawAddress destAddr;
        /*
         * note: according to current interface design,
         * callers of this function must have been granted GET_NEARLINK_PEER_MAC.
         * however, ConvertToRandomAddress would verify again.
         */
        NearlinkDeviceManager::GetInstance()->ConvertToRandomAddress(*it, destAddr, false);
        pairedAddr.emplace_back(destAddr);
    }
    HILOGD("pairedAddr size(%{public}lu)", pairedAddr.size());
    return NL_NO_ERROR;
}

NlErrCode NearlinkHostServer::SetConnectionMode(int32_t connectionMode, int32_t duration)
{
    HILOGI("connectionMode: %{public}d, duration: %{public}d", connectionMode, duration);
    NL_CHECK_RETURN_RET(IsSleEnabledInner(), NL_ERR_SLE_OFF, "nearlink is off.");
    // for sle connectable
    SleInterfaceAdapterSub *sleService = static_cast<SleInterfaceAdapterSub *>
        (SleInterfaceManager::GetInstance()->GetAdapter(SleTransport::ADAPTER_SLE));
    NL_CHECK_RETURN_RET(sleService, NL_ERR_INTERNAL_ERROR, "sleService invalid.");
    sleService->SetSleConnectionMode(connectionMode, duration);
    return NL_NO_ERROR;
}

NlErrCode NearlinkHostServer::RemovePair(const sptr<NearlinkRawAddress> &device)
{
    if (device == nullptr) {
        HILOGE("device is nullptr.");
        return NL_ERR_INTERNAL_ERROR;
    }
    std::string realAddr = "";
    NearlinkDeviceManager::GetInstance()->GetDeviceRealAddr(device->GetAddress(), realAddr);
    NL_CHECK_RETURN_RET(IsSleEnabledInner(), NL_ERR_SLE_OFF, "nearlink is off.");
    HILOGI("addr:%{public}s", GetEncryptAddr(realAddr).c_str());
    SleInterfaceAdapterSub *sleService = static_cast<SleInterfaceAdapterSub *>
        (SleInterfaceManager::GetInstance()->GetAdapter(SleTransport::ADAPTER_SLE));
    NL_CHECK_RETURN_RET(sleService, NL_ERR_INTERNAL_ERROR, "sleService invalid.");
    RawAddress addr(realAddr);

    std::string callingName = NearLinkPermissionManager::GetCallingName();
    NearlinkDftUe::GetInstance().WriteCommonUe(DFT_PAIRING_CANCEL_UE, addr, REMOVEPAIR,
        CANCELPAIRTYPEINVALID, callingName);
    if (sleService->RemovePair(addr)) {
        return NL_NO_ERROR;
    }
    return NL_ERR_INTERNAL_ERROR;
}

NlErrCode NearlinkHostServer::RemoveAllPairs()
{
    HILOGI("enter");
    NL_CHECK_RETURN_RET(IsSleEnabledInner(), NL_ERR_SLE_OFF, "nearlink is off.");
    SleInterfaceAdapterSub *sleService = static_cast<SleInterfaceAdapterSub *>
        (SleInterfaceManager::GetInstance()->GetAdapter(SleTransport::ADAPTER_SLE));
    NL_CHECK_RETURN_RET(sleService, NL_ERR_INTERNAL_ERROR, "sleService invalid.");
    if (!sleService->RemoveAllPairs()) {
        HILOGE("BREDR RemoveAllPairs failed");
        return NL_ERR_INTERNAL_ERROR;
    }
    return NL_NO_ERROR;
}

NlErrCode NearlinkHostServer::RegisterSlePeripheralCallback(const sptr<INearlinkSlePeripheralObserver> &observer)
{
    HILOGD("enter");
    if (observer == nullptr) {
        HILOGE("observer is nullptr!");
        return NL_ERR_INVALID_PARAM;
    }
    if (pimpl->sleRemoteObservers_.Size() >= MAX_OBSERVER_SIZE) {
        HILOGE("Register SlePeripheral failed, the number of observers is the maximum");
        return NL_ERR_INTERNAL_ERROR;
    }
    // allow register or deregister even if nearlink is off.
    uint64_t fullTokenId = IPCSkeleton::GetCallingFullTokenID();
    bool isUseRealAddrFlag = NearLinkPermissionManager::IsUseRealAddr();
    int32_t callingUid = IPCSkeleton::GetCallingUid();
    pimpl->sleRemoteObservers_.Register(observer);
    impl::NearlinkHostRemoteInfo info {fullTokenId, isUseRealAddrFlag, callingUid};
    pimpl->remoteContainer_->AddRemoteInfo(observer->AsObject(), info);
    return NL_NO_ERROR;
}

NlErrCode NearlinkHostServer::DeregisterSlePeripheralCallback(const sptr<INearlinkSlePeripheralObserver> &observer)
{
    HILOGI("enter");
    if (observer == nullptr) {
        HILOGE("observer is nullptr!");
        return NL_ERR_INVALID_PARAM;
    }
    // allow register or deregister even if nearlink is off.
    pimpl->sleRemoteObservers_.Deregister(observer);
    pimpl->remoteContainer_->DeleteRemoteInfo(observer->AsObject());
    return NL_NO_ERROR;
}

NlErrCode NearlinkHostServer::RegisterSleAdapterObserver(const sptr<INearlinkHostObserver> &observer)
{
    HILOGD("enter");
    if (observer == nullptr) {
        HILOGE("observer is nullptr!");
        return NL_ERR_INVALID_PARAM;
    }
    if (pimpl->sleObservers_.Size() >= MAX_OBSERVER_SIZE) {
        HILOGE("Register SleAdapter failed, the number of observers is the maximum");
        return NL_ERR_INTERNAL_ERROR;
    }
    // allow register or deregister even if nearlink is off.
    pimpl->sleObservers_.Register(observer);
    uint64_t fullTokenId = IPCSkeleton::GetCallingFullTokenID();
    bool isUseRealAddrFlag = NearLinkPermissionManager::IsUseRealAddr();
    int32_t callingUid = IPCSkeleton::GetCallingUid();
    impl::NearlinkHostRemoteInfo info {fullTokenId, isUseRealAddrFlag, callingUid};
    pimpl->remoteContainer_->AddRemoteInfo(observer->AsObject(), info);
    return NL_NO_ERROR;
}

NlErrCode NearlinkHostServer::DeregisterSleAdapterObserver(const sptr<INearlinkHostObserver> &observer)
{
    HILOGI("enter");
    if (observer == nullptr) {
        HILOGE("observer is nullptr!");
        return NL_ERR_INVALID_PARAM;
    }
    // allow register or deregister even if nearlink is off.
    pimpl->sleObservers_.Deregister(observer);
    pimpl->remoteContainer_->DeleteRemoteInfo(observer->AsObject());
    return NL_NO_ERROR;
}

NlErrCode NearlinkHostServer::RegisterDeviceBatteryObserver(const sptr<INearlinkDeviceBatteryObserver> &observer)
{
    HILOGD("enter");
    if (observer == nullptr) {
        HILOGE("observer is nullptr!");
        return NL_ERR_INVALID_PARAM;
    }
    if (pimpl->deviceBatteryObservers_.Size() >= MAX_OBSERVER_SIZE) {
        HILOGE("Register basService failed, the number of observers is the maximum");
        return NL_ERR_INTERNAL_ERROR;
    }
    // allow register or deregister even if nearlink is off.
    pimpl->deviceBatteryObservers_.Register(observer);
    uint64_t fullTokenId = IPCSkeleton::GetCallingFullTokenID();
    bool isUseRealAddrFlag = NearLinkPermissionManager::IsUseRealAddr();
    int32_t callingUid = IPCSkeleton::GetCallingUid();
    impl::NearlinkBasRemoteInfo info {fullTokenId, isUseRealAddrFlag, false, callingUid};
    pimpl->remoteBatteryContainer_->AddRemoteInfo(observer->AsObject(), info);
    return NL_NO_ERROR;
}

NlErrCode NearlinkHostServer::DeregisterDeviceBatteryObserver(const sptr<INearlinkDeviceBatteryObserver> &observer)
{
    HILOGI("enter");
    if (observer == nullptr) {
        HILOGE("observer is nullptr!");
        return NL_ERR_INVALID_PARAM;
    }
    // allow register or deregister even if nearlink is off.
    pimpl->deviceBatteryObservers_.Deregister(observer);
    pimpl->remoteBatteryContainer_->DeleteRemoteInfo(observer->AsObject());
    return NL_NO_ERROR;
}

NlErrCode NearlinkHostServer::RegisterDeviceRssiObserver(const sptr<INearlinkDeviceRssiObserver> &observer)
{
    HILOGD("enter");
    NL_CHECK_RETURN_RET(observer != nullptr, NL_ERR_INVALID_PARAM, "observer is null");
    NL_CHECK_RETURN_RET(pimpl->deviceRssiObservers_.Size() < MAX_OBSERVER_SIZE, NL_ERR_INTERNAL_ERROR, "observer max");

    pimpl->deviceRssiObservers_.Register(observer);
    uint64_t fullTokenId = IPCSkeleton::GetCallingFullTokenID();
    bool isUseRealAddrFlag = NearLinkPermissionManager::IsUseRealAddr();
    int32_t callingUid = IPCSkeleton::GetCallingUid();
    impl::NearlinkHostRemoteInfo info {fullTokenId, isUseRealAddrFlag, callingUid};
    pimpl->remoteContainer_->AddRemoteInfo(observer->AsObject(), info);
    return NL_NO_ERROR;
}

NlErrCode NearlinkHostServer::DeregisterDeviceRssiObserver(const sptr<INearlinkDeviceRssiObserver> &observer)
{
    HILOGI("enter");
    NL_CHECK_RETURN_RET(observer != nullptr, NL_ERR_INVALID_PARAM, "observer is null");
    pimpl->deviceRssiObservers_.Deregister(observer);
    pimpl->remoteContainer_->DeleteRemoteInfo(observer->AsObject());
    return NL_NO_ERROR;
}

NlErrCode NearlinkHostServer::GetSleMaxAdvertisingDataLength(uint32_t &maxAdvDataLen)
{
    HILOGI("enter");
    NL_CHECK_RETURN_RET(IsSleAvailableToCallerInner(), NL_ERR_SLE_OFF, "nearlink is off.");
    SleInterfaceAdapterSub *sleService = static_cast<SleInterfaceAdapterSub *>
        (SleInterfaceManager::GetInstance()->GetAdapter(SleTransport::ADAPTER_SLE));
    NL_CHECK_RETURN_RET(sleService, NL_ERR_INTERNAL_ERROR, "sleService invalid.");
    maxAdvDataLen = sleService->GetSleMaxAdvertisingDataLength();
    return NL_NO_ERROR;
}

NlErrCode NearlinkHostServer::GetDeviceName(int32_t transport, const std::string &address, std::string &name)
{
    if (transport != static_cast<int>(NlTransportType::NL_TRANSPORT_SLE)) {
        HILOGE("transport(%{public}d) is invalid.", transport);
        return NL_ERR_INVALID_PARAM;
    }
    // allow to get the device name even if the nearlink is off.
    std::string realAddr = "";
    NearlinkDeviceManager::GetInstance()->GetDeviceRealAddr(address, realAddr);
    RawAddress addr(realAddr);
    SleInterfaceAdapterSub *sleService = static_cast<SleInterfaceAdapterSub *>
        (SleInterfaceManager::GetInstance()->GetAdapter(SleTransport::ADAPTER_SLE));
    NL_CHECK_RETURN_RET(sleService, NL_ERR_INTERNAL_ERROR, "sleService invalid.");
    name = sleService->GetDeviceName(addr);
    HILOGD("addr(%{public}s)", GetEncryptAddr(realAddr).c_str());
    return NL_NO_ERROR;
}

NlErrCode NearlinkHostServer::GetDeviceAlias(int32_t transport, const std::string &address, std::string &alias)
{
    if (transport != static_cast<int>(NlTransportType::NL_TRANSPORT_SLE)) {
        HILOGW("transport is not NL_TRANSPORT_SLE");
        return NL_ERR_INVALID_STATE;
    }
    // allow to get the device alias even if the nearlink is off.
    std::string realAddr = "";
    NearlinkDeviceManager::GetInstance()->GetDeviceRealAddr(address, realAddr);
    RawAddress addr(realAddr);
    SleInterfaceAdapterSub *sleService = static_cast<SleInterfaceAdapterSub *>
        (SleInterfaceManager::GetInstance()->GetAdapter(SleTransport::ADAPTER_SLE));
    NL_CHECK_RETURN_RET(sleService, NL_ERR_INTERNAL_ERROR, "sleService invalid.");
    alias = sleService->GetAliasName(addr);
    return NL_NO_ERROR;
}

NlErrCode NearlinkHostServer::SetDeviceAlias(int32_t transport, const std::string &address, const std::string &alias)
{
    HILOGI("alias: %{public}s", alias.c_str());
    if (transport != static_cast<int>(NlTransportType::NL_TRANSPORT_SLE)) {
        HILOGW("transport is not NL_TRANSPORT_SLE");
        return NL_ERR_INVALID_STATE;
    }
    NL_CHECK_RETURN_RET(IsSleEnabledInner(), NL_ERR_SLE_OFF, "nearlink is off.");
    std::string realAddr = "";
    NearlinkDeviceManager::GetInstance()->GetDeviceRealAddr(address, realAddr);
    RawAddress addr(realAddr);
    SleInterfaceAdapterSub *sleService = static_cast<SleInterfaceAdapterSub *>
        (SleInterfaceManager::GetInstance()->GetAdapter(SleTransport::ADAPTER_SLE));
    NL_CHECK_RETURN_RET(sleService, NL_ERR_INTERNAL_ERROR, "sleService invalid.");
    if (sleService->SetAliasName(addr, alias)) {
        return NL_NO_ERROR;
    }
    return NL_ERR_INTERNAL_ERROR;
}

NlErrCode NearlinkHostServer::SetPairingConfirmation(int32_t transport, const std::string &address, bool cfm)
{
    if (transport != static_cast<int>(NlTransportType::NL_TRANSPORT_SLE)) {
        HILOGW("transport is not NL_TRANSPORT_SLE");
        return NL_ERR_INVALID_STATE;
    }
    NL_CHECK_RETURN_RET(IsSleEnabledInner(), NL_ERR_SLE_OFF, "nearlink is off.");
    std::string realAddr = "";
    NearlinkDeviceManager::GetInstance()->GetDeviceRealAddr(address, realAddr);
    RawAddress addr(realAddr);
    NearlinkDftUe::GetInstance().WriteCommonUe(DFT_PAIRING_CFM_UE, addr, SETPAIRCONFIRM, PAIRCFMINVALID,
                                               NearLinkPermissionManager::GetCallingName());
    SleInterfaceAdapterSub *sleService = static_cast<SleInterfaceAdapterSub *>
        (SleInterfaceManager::GetInstance()->GetAdapter(SleTransport::ADAPTER_SLE));
    NL_CHECK_RETURN_RET(sleService, NL_ERR_INTERNAL_ERROR, "sleService invalid.");
    if (cfm == false) {
        HILOGI("cancel pairing");
        if (sleService->CancelPairing(addr)) {
            return NL_NO_ERROR;
        }
        return NL_ERR_INTERNAL_ERROR;
    }
    HILOGI("set pairing confirmation");
    if (sleService->SetPairingConfirmation(addr)) {
        return NL_NO_ERROR;
    }
    return NL_ERR_INTERNAL_ERROR;
}

NlErrCode NearlinkHostServer::GetPairState(int32_t transport, const std::string &address, int &pairState)
{
    std::string realAddr = "";
    NearlinkDeviceManager::GetInstance()->GetDeviceRealAddr(address, realAddr);
    HILOGI("transport: %{public}d, address: %{public}s", transport, GetEncryptAddr(realAddr).c_str());
    if (transport != static_cast<int>(NlTransportType::NL_TRANSPORT_SLE)) {
        return NL_ERR_INVALID_PARAM;
    }
    NL_CHECK_RETURN_RET(IsSleEnabledInner(), NL_ERR_SLE_OFF, "nearlink is off.");
    RawAddress addr(realAddr);
    SleInterfaceAdapterSub *sleService = static_cast<SleInterfaceAdapterSub *>
        (SleInterfaceManager::GetInstance()->GetAdapter(SleTransport::ADAPTER_SLE));
    NL_CHECK_RETURN_RET(sleService, NL_ERR_INTERNAL_ERROR, "sleService invalid.");
    pairState = sleService->GetPairState(addr);

    return NL_NO_ERROR;
}

NlErrCode NearlinkHostServer::StartPair(int32_t transport, const std::string &address)
{
    std::string realAddr = "";
    NearlinkDeviceManager::GetInstance()->GetDeviceRealAddr(address, realAddr);
    HILOG_COMM_INFO("[%{public}s:%{public}d]transport: %{public}d, address: %{public}s",
        __FUNCTION__, __LINE__, transport, GetEncryptAddr(realAddr).c_str());
    if (transport != static_cast<int>(NlTransportType::NL_TRANSPORT_SLE)) {
        return NL_ERR_INVALID_PARAM;
    }
    NL_CHECK_RETURN_RET(IsSleEnabledInner(), NL_ERR_SLE_OFF, "nearlink is off.");
    RawAddress addr(realAddr);
    std::string callingName = NearLinkPermissionManager::GetCallingName();
    NearlinkDftUe::GetInstance().WriteCommonUe(DFT_PAIRING_START_UE, addr, NOCREDIBLEPAIR,
        PAIRTYPEINVALID, callingName);
    DftCacheCallingName(addr.GetAddress(), callingName);
    HILOGI("callingName: %{public}s", callingName.c_str());
    SleInterfaceAdapterSub *sleService = static_cast<SleInterfaceAdapterSub *>
        (SleInterfaceManager::GetInstance()->GetAdapter(SleTransport::ADAPTER_SLE));
    NL_CHECK_RETURN_RET(sleService, NL_ERR_INTERNAL_ERROR, "sleService invalid.");
    if (sleService->StartPair(addr)) {
        return NL_NO_ERROR;
    }
    return NL_ERR_INTERNAL_ERROR;
}

NlErrCode NearlinkHostServer::StartCrediblePair(int32_t transport, const std::string &address)
{
    std::string realAddr = "";
    NearlinkDeviceManager::GetInstance()->GetDeviceRealAddr(address, realAddr);
    HILOG_COMM_INFO("[%{public}s:%{public}d]transport: %{public}d, address: %{public}s",
        __FUNCTION__, __LINE__, transport, GetEncryptAddr(realAddr).c_str());
    if (transport != static_cast<int>(NlTransportType::NL_TRANSPORT_SLE)) {
        return NL_ERR_INVALID_PARAM;
    }
    NL_CHECK_RETURN_RET(IsSleEnabledInner(), NL_ERR_SLE_OFF, "nearlink is off.");
    RawAddress addr(realAddr);
    std::string callingName = NearLinkPermissionManager::GetCallingName();
    NearlinkDftUe::GetInstance().WriteCommonUe(DFT_PAIRING_START_UE, addr, CREDIBLEPAIR, PAIRTYPEINVALID,
        callingName);
    DftCacheCallingName(addr.GetAddress(), callingName);
    SleInterfaceAdapterSub *sleService = static_cast<SleInterfaceAdapterSub *>
        (SleInterfaceManager::GetInstance()->GetAdapter(SleTransport::ADAPTER_SLE));
    NL_CHECK_RETURN_RET(sleService, NL_ERR_INTERNAL_ERROR, "sleService invalid.");
    if (sleService->StartCrediblePair(addr)) {
        return NL_NO_ERROR;
    }
    HILOGE("StartCrediblePair fail!");
    return NL_ERR_INTERNAL_ERROR;
}

NlErrCode NearlinkHostServer::CancelPairing(int32_t transport, const std::string &address)
{
    std::string realAddr = "";
    NearlinkDeviceManager::GetInstance()->GetDeviceRealAddr(address, realAddr);
    HILOG_COMM_INFO("[%{public}s:%{public}d]transport: %{public}d, address: %{public}s",
        __FUNCTION__, __LINE__, transport, GetEncryptAddr(realAddr).c_str());
    if (transport != static_cast<int>(NlTransportType::NL_TRANSPORT_SLE)) {
        return NL_ERR_INVALID_PARAM;
    }
    NL_CHECK_RETURN_RET(IsSleEnabledInner(), NL_ERR_SLE_OFF, "nearlink is off.");
    RawAddress addr(realAddr);
    SleInterfaceAdapterSub *sleService = static_cast<SleInterfaceAdapterSub *>
        (SleInterfaceManager::GetInstance()->GetAdapter(SleTransport::ADAPTER_SLE));
    NL_CHECK_RETURN_RET(sleService, NL_ERR_INTERNAL_ERROR, "sleService invalid.");
    std::string callingName = NearLinkPermissionManager::GetCallingName();
    NearlinkDftUe::GetInstance().WriteCommonUe(DFT_PAIRING_CANCEL_UE, addr, CANCELPAIR,
        CANCELPAIRTYPEINVALID, callingName);
    if (sleService->CancelPairing(addr)) {
        return NL_NO_ERROR;
    }
    return NL_ERR_INTERNAL_ERROR;
}

NlErrCode NearlinkHostServer::SetPairingPassCode(int32_t transport, const std::string &address,
                                                 const std::string &passCode)
{
    std::string realAddr = "";
    NearlinkDeviceManager::GetInstance()->GetDeviceRealAddr(address, realAddr);
    if (transport != static_cast<int>(NlTransportType::NL_TRANSPORT_SLE)) {
        HILOGW("transport is not NL_TRANSPORT_SLE");
        return NL_ERR_INVALID_STATE;
    }
    NL_CHECK_RETURN_RET(IsSleEnabledInner(), NL_ERR_SLE_OFF, "nearlink is off.");

    RawAddress addr(realAddr);
    NearlinkDftUe::GetInstance().WriteCommonUe(DFT_PAIRING_CFM_UE, addr, SETPAIRPASSCODE, PAIRCFMINVALID,
                                               NearLinkPermissionManager::GetCallingName());
    SleInterfaceAdapterSub *sleService = static_cast<SleInterfaceAdapterSub *>
        (SleInterfaceManager::GetInstance()->GetAdapter(SleTransport::ADAPTER_SLE));
    NL_CHECK_RETURN_RET(sleService, NL_ERR_INTERNAL_ERROR, "sleService invalid.");
    if (sleService->SetPairingPassCode(addr, passCode)) {
        return NL_NO_ERROR;
    }
    return NL_ERR_INTERNAL_ERROR;
}

NlErrCode NearlinkHostServer::IsBondedFromLocal(int32_t transport, const std::string &address, bool &isBondedFromLocal)
{
    HILOGI("transport: %{public}d, address: %{public}s", transport, GetEncryptAddr(address).c_str());
    if (transport != static_cast<int>(NlTransportType::NL_TRANSPORT_SLE)) {
        return NL_ERR_INVALID_PARAM;
    }
    NL_CHECK_RETURN_RET(IsSleEnabledInner(), NL_ERR_SLE_OFF, "nearlink is off.");
    RawAddress addr(address);
    SleInterfaceAdapterSub *sleService = static_cast<SleInterfaceAdapterSub *>
        (SleInterfaceManager::GetInstance()->GetAdapter(SleTransport::ADAPTER_SLE));
    NL_CHECK_RETURN_RET(sleService, NL_ERR_INTERNAL_ERROR, "sleService invalid.");
    isBondedFromLocal = sleService->IsBondedFromLocal(addr);
    return NL_NO_ERROR;
}

NlErrCode NearlinkHostServer::IsAcbConnected(int32_t transport, const std::string &address, bool &isAcbConnected)
{
    std::string realAddr = "";
    NearlinkDeviceManager::GetInstance()->GetDeviceRealAddr(address, realAddr);
    HILOGD("transport: %{public}d, address: %{public}s", transport, GetEncryptAddr(address).c_str());
    if (transport != static_cast<int>(NlTransportType::NL_TRANSPORT_SLE)) {
        return NL_ERR_INVALID_PARAM;
    }
    NL_CHECK_RETURN_RET(IsSleAvailableToCallerInner(), NL_ERR_SLE_OFF, "nearlink is off.");
    RawAddress addr(realAddr);
    SleInterfaceAdapterSub *sleService = static_cast<SleInterfaceAdapterSub *>
        (SleInterfaceManager::GetInstance()->GetAdapter(SleTransport::ADAPTER_SLE));
    NL_CHECK_RETURN_RET(sleService, NL_ERR_INTERNAL_ERROR, "sleService invalid.");
    isAcbConnected = sleService->IsAcbConnected(addr);
    return NL_NO_ERROR;
}

NlErrCode NearlinkHostServer::IsAcbEncrypted(int32_t transport, const std::string &address, bool &isAcbEncrypted)
{
    std::string realAddr = "";
    NearlinkDeviceManager::GetInstance()->GetDeviceRealAddr(address, realAddr);
    HILOGI("transport: %{public}d, address: %{public}s", transport, GetEncryptAddr(address).c_str());
    if (transport != static_cast<int>(NlTransportType::NL_TRANSPORT_SLE)) {
        return NL_ERR_INVALID_PARAM;
    }
    NL_CHECK_RETURN_RET(IsSleAvailableToCallerInner(), NL_ERR_SLE_OFF, "nearlink is off.");
    RawAddress addr(realAddr);
    SleInterfaceAdapterSub *sleService = static_cast<SleInterfaceAdapterSub *>
        (SleInterfaceManager::GetInstance()->GetAdapter(SleTransport::ADAPTER_SLE));
    NL_CHECK_RETURN_RET(sleService, NL_ERR_INTERNAL_ERROR, "sleService invalid.");
    isAcbEncrypted = sleService->IsAcbEncrypted(addr);
    return NL_NO_ERROR;
}

NlErrCode NearlinkHostServer::GetLinkRole(int32_t transport, const std::string &address, uint8_t &role)
{
    HILOGI("transport: %{public}d, address: %{public}s", transport, GetEncryptAddr(address).c_str());
    NL_CHECK_RETURN_RET(transport == static_cast<int>(NlTransportType::NL_TRANSPORT_SLE),
        NL_ERR_INVALID_PARAM, "transport is not sle");
    NL_CHECK_RETURN_RET(IsSleAvailableToCallerInner(), NL_ERR_SLE_OFF, "nearlink is off.");
    RawAddress addr(address);
    SleInterfaceAdapterSub *sleService = static_cast<SleInterfaceAdapterSub *>
        (SleInterfaceManager::GetInstance()->GetAdapter(SleTransport::ADAPTER_SLE));
    NL_CHECK_RETURN_RET(sleService, NL_ERR_INTERNAL_ERROR, "sleService invalid.");
    NL_CHECK_RETURN_RET(sleService->IsAcbConnected(addr), NL_ERR_DEVICE_DISCONNECTED, "device is not connected.");
    role = sleService->GetLinkRole(addr);
    return NL_NO_ERROR;
}

NlErrCode NearlinkHostServer::GetDeviceUuids(int32_t transport, const std::string &address,
    std::vector<std::string> &uuids)
{
    HILOGI("transport: %{public}d, address: %{public}s", transport, GetEncryptAddr(address).c_str());
    if (transport != static_cast<int>(NlTransportType::NL_TRANSPORT_SLE)) {
        return NL_ERR_INVALID_PARAM;
    }
    NL_CHECK_RETURN_RET(IsSleAvailableToCallerInner(), NL_ERR_SLE_OFF, "nearlink is off.");
    std::vector<Uuid> parcelUuids;
    RawAddress addr(address);
    SleInterfaceAdapterSub *sleService = static_cast<SleInterfaceAdapterSub *>
        (SleInterfaceManager::GetInstance()->GetAdapter(SleTransport::ADAPTER_SLE));
    NL_CHECK_RETURN_RET(sleService, NL_ERR_INTERNAL_ERROR, "sleService invalid.");
    parcelUuids = sleService->GetDeviceUuids(addr);
    for (auto uuid : parcelUuids) {
        uuids.push_back(uuid.ToString());
    }
    return NL_NO_ERROR;
}

NlErrCode NearlinkHostServer::PairRequestReply(int32_t transport, const std::string &address, bool accept)
{
    HILOGI("transport: %{public}d, address: %{public}s, accept: %{public}d",
        transport, GetEncryptAddr(address).c_str(), accept);
    if (transport != static_cast<int>(NlTransportType::NL_TRANSPORT_SLE)) {
        return NL_ERR_INVALID_PARAM;
    }
    NL_CHECK_RETURN_RET(IsSleEnabledInner(), NL_ERR_SLE_OFF, "nearlink is off.");
    RawAddress addr(address);
    SleInterfaceAdapterSub *sleService = static_cast<SleInterfaceAdapterSub *>
        (SleInterfaceManager::GetInstance()->GetAdapter(SleTransport::ADAPTER_SLE));
    NL_CHECK_RETURN_RET(sleService, NL_ERR_INTERNAL_ERROR, "sleService invalid.");
    if (sleService->PairRequestReply(addr, accept)) {
        return NL_NO_ERROR;
    }
    return NL_ERR_INTERNAL_ERROR;
}

NlErrCode NearlinkHostServer::ReadRemoteRssiValue(const std::string &address)
{
    HILOGI("address: %{public}s", GetEncryptAddr(address).c_str());
    std::string realAddr = "";
    NearlinkDeviceManager::GetInstance()->GetDeviceRealAddr(address, realAddr);
    NL_CHECK_RETURN_RET(IsSleAvailableToCallerInner(), NL_ERR_SLE_OFF, "nearlink is off.");

    SleInterfaceAdapterSub *sleService = static_cast<SleInterfaceAdapterSub *>
        (SleInterfaceManager::GetInstance()->GetAdapter(SleTransport::ADAPTER_SLE));
    NL_CHECK_RETURN_RET(sleService, NL_ERR_INTERNAL_ERROR, "sleService invalid.");
    RawAddress addr(realAddr);
    int acbState = sleService->GetAcbState(addr);
    NL_CHECK_RETURN_RET(acbState >= static_cast<int>(SleConnState::SLE_CONNECTION_STATE_CONNECTED),
        NL_ERR_INTERNAL_ERROR,
        "need connected.");
    sleService->ReadRemoteRssiValue(addr);
    return NL_NO_ERROR;
}

NlErrCode NearlinkHostServer::GetDeviceAppearance(const std::string &address, int &appearance)
{
    std::string realAddr = "";
    NearlinkDeviceManager::GetInstance()->GetDeviceRealAddr(address, realAddr);
    HILOGD("address: %{public}s", GetEncryptAddr(realAddr).c_str());
    NL_CHECK_RETURN_RET(IsSleAvailableToCallerInner(), NL_ERR_SLE_OFF, "nearlink is off.");
    RawAddress addr(realAddr);
    SleInterfaceAdapterSub *sleService = static_cast<SleInterfaceAdapterSub *>
        (SleInterfaceManager::GetInstance()->GetAdapter(SleTransport::ADAPTER_SLE));
    NL_CHECK_RETURN_RET(sleService, NL_ERR_INTERNAL_ERROR, "sleService invalid.");
    appearance = sleService->GetDeviceAppearance(addr);
    return NL_NO_ERROR;
}

NlErrCode NearlinkHostServer::ConnectAllowedProfiles(const std::string &address)
{
    std::string realAddr = "";
    NearlinkDeviceManager::GetInstance()->GetDeviceRealAddr(address, realAddr);
    HILOG_COMM_INFO("[%{public}s:%{public}d]address: %{public}s",
        __FUNCTION__, __LINE__, GetEncryptAddr(realAddr).c_str());
    NL_CHECK_RETURN_RET(IsSleEnabledInner(), NL_ERR_SLE_OFF, "nearlink is off.");
    RawAddress addr(realAddr);
    SleInterfaceAdapterSub *sleService = static_cast<SleInterfaceAdapterSub *>
        (SleInterfaceManager::GetInstance()->GetAdapter(SleTransport::ADAPTER_SLE));
    NL_CHECK_RETURN_RET(sleService, NL_ERR_INTERNAL_ERROR, "sleService invalid.");
    sleService->ConnectAllProfile(addr);
    return NL_NO_ERROR;
}

NlErrCode NearlinkHostServer::DisconnectAllowedProfiles(const std::string &address)
{
    std::string realAddr = "";
    NearlinkDeviceManager::GetInstance()->GetDeviceRealAddr(address, realAddr);
    HILOG_COMM_INFO("[%{public}s:%{public}d]address: %{public}s",
        __FUNCTION__, __LINE__, GetEncryptAddr(realAddr).c_str());
    NL_CHECK_RETURN_RET(IsSleEnabledInner(), NL_ERR_SLE_OFF, "nearlink is off.");
    RawAddress addr(realAddr);
    SleInterfaceAdapterSub *sleService = static_cast<SleInterfaceAdapterSub *>
        (SleInterfaceManager::GetInstance()->GetAdapter(SleTransport::ADAPTER_SLE));
    NL_CHECK_RETURN_RET(sleService, NL_ERR_INTERNAL_ERROR, "sleService invalid.");
    sleService->DisconnectAllProfile(addr);
    return NL_NO_ERROR;
}

NlErrCode NearlinkHostServer::GetDeviceProductId(const std::string &address, uint16_t &productId)
{
    HILOGI("address: %{public}s", GetEncryptAddr(address).c_str());
    NL_CHECK_RETURN_RET(IsSleAvailableToCallerInner(), NL_ERR_SLE_OFF, "nearlink is off.");
    RawAddress addr(address);
    SleInterfaceAdapterSub *sleService = static_cast<SleInterfaceAdapterSub *>
        (SleInterfaceManager::GetInstance()->GetAdapter(SleTransport::ADAPTER_SLE));
    NL_CHECK_RETURN_RET(sleService, NL_ERR_INTERNAL_ERROR, "sleService invalid.");
    productId = sleService->GetDeviceProductId(addr);
    return NL_NO_ERROR;
}

NlErrCode NearlinkHostServer::GetDeviceVendorId(const std::string &address, uint16_t &vendorId)
{
    HILOGI("address: %{public}s", GetEncryptAddr(address).c_str());
    NL_CHECK_RETURN_RET(IsSleAvailableToCallerInner(), NL_ERR_SLE_OFF, "nearlink is off.");
    RawAddress addr(address);
    SleInterfaceAdapterSub *sleService = static_cast<SleInterfaceAdapterSub *>
        (SleInterfaceManager::GetInstance()->GetAdapter(SleTransport::ADAPTER_SLE));
    NL_CHECK_RETURN_RET(sleService, NL_ERR_INTERNAL_ERROR, "sleService invalid.");
    vendorId = sleService->GetDeviceVendorId(addr);
    return NL_NO_ERROR;
}

NlErrCode NearlinkHostServer::GetDeviceModel(const std::string &address, NearlinkDeviceModel &model)
{
    HILOGD("address: %{public}s", GetEncryptAddr(address).c_str());
    NL_CHECK_RETURN_RET(IsSleAvailableToCallerInner(), NL_ERR_SLE_OFF, "nearlink is off.");
    RawAddress addr(address);
    SleInterfaceAdapterSub *sleService = static_cast<SleInterfaceAdapterSub *>
        (SleInterfaceManager::GetInstance()->GetAdapter(SleTransport::ADAPTER_SLE));
    NL_CHECK_RETURN_RET(sleService, NL_ERR_INTERNAL_ERROR, "sleService invalid.");
    model = sleService->GetDeviceModel(addr);
    return NL_NO_ERROR;
}

NlErrCode NearlinkHostServer::GetDeviceInformation(const std::string &address, NearlinkDeviceInformation &information)
{
    HILOGD("address: %{public}s", GetEncryptAddr(address).c_str());
    NL_CHECK_RETURN_RET(IsSleAvailableToCallerInner(), NL_ERR_SLE_OFF, "nearlink is off.");
    std::string realAddr = "";
    NearlinkDeviceManager::GetInstance()->GetDeviceRealAddr(address, realAddr);
    RawAddress addr(realAddr);
    ProfileDis *disService = static_cast<ProfileDis *>(
        SleInterfaceProfileManager::GetInstance().GetProfileService(PROFILE_NAME_DIS));
    NL_CHECK_RETURN_RET(disService, NL_ERR_INTERNAL_ERROR, "disService invalid.");
    information = disService->GetDeviceInformation(addr);
    return NL_NO_ERROR;
}

NlErrCode NearlinkHostServer::FactoryReset()
{
    HILOG_COMM_INFO("[%{public}s:%{public}d] enter", __FUNCTION__, __LINE__);
    NL_CHECK_RETURN_RET(
        SleInterfaceManager::GetInstance() != nullptr, NL_ERR_INTERNAL_ERROR, "GetInstance is nullptr!");
    SleInterfaceAdapterSub *sleService = static_cast<SleInterfaceAdapterSub *>
        (SleInterfaceManager::GetInstance()->GetAdapter(SleTransport::ADAPTER_SLE));
    NL_CHECK_RETURN_RET(sleService, NL_ERR_INTERNAL_ERROR, "sleService invalid.");
    NL_CHECK_RETURN_RET(sleService->FactoryReset(), NL_ERR_INTERNAL_ERROR, "FactoryReset failed");
    /* use disable to reset param and unload sa */
    return DisableSleForFactoryReset();
}

NlErrCode NearlinkHostServer::SetFreqHopping(const std::vector<uint8_t> &freq)
{
    HILOGI("enter");
    if (freq.size() != SLE_FREQ_HOPPING_LEN) {
        HILOGE("Param_Freq len is invalid!");
        return NL_ERR_INVALID_PARAM;
    }
    NL_CHECK_RETURN_RET(IsSleAvailableToCallerInner(), NL_ERR_SLE_OFF, "nearlink is off.");
    SleInterfaceAdapterSub *sleService = static_cast<SleInterfaceAdapterSub *>
        (SleInterfaceManager::GetInstance()->GetAdapter(SleTransport::ADAPTER_SLE));
    NL_CHECK_RETURN_RET(sleService, NL_ERR_INTERNAL_ERROR, "sleService invalid.");
    if (sleService->SleFreqHopping(freq)) {
        return NL_NO_ERROR;
    }
    return NL_ERR_INTERNAL_ERROR;
}

NlErrCode NearlinkHostServer::UpdateSleVirtualDevice(int32_t cmd, const std::string &address)
{
    std::string realAddr = "";
    NearlinkDeviceManager::GetInstance()->GetDeviceRealAddr(address, realAddr);
    HILOGI("cmd: %{public}d, realAddr: %{public}s", cmd, GetEncryptAddr(realAddr).c_str());
    NL_CHECK_RETURN_RET(IsSleAvailableToCallerInner(), NL_ERR_SLE_OFF, "nearlink is off.");
    RawAddress addr(realAddr);
    SleInterfaceAdapterSub *sleService = static_cast<SleInterfaceAdapterSub *>
        (SleInterfaceManager::GetInstance()->GetAdapter(SleTransport::ADAPTER_SLE));
    NL_CHECK_RETURN_RET(sleService, NL_ERR_INTERNAL_ERROR, "sleService invalid.");
    if (sleService->UpdateSleVirtualDevice(cmd, addr)) {
        return NL_NO_ERROR;
    }
    return NL_ERR_INTERNAL_ERROR;
}

NlErrCode NearlinkHostServer::UpdateRefusePolicy(int32_t protocolType, int32_t pid, int64_t refuseTime)
{
    HILOGI("enter");
    NL_CHECK_RETURN_RET(refuseTime > 0, NL_ERR_INVALID_PARAM, "invalid time");
    NL_CHECK_RETURN_RET(IsSleEnabledInner(), NL_ERR_SLE_OFF, "nearlink is off.");
    SleInterfaceAdapterSub *sleService = static_cast<SleInterfaceAdapterSub *>
        (SleInterfaceManager::GetInstance()->GetAdapter(SleTransport::ADAPTER_SLE));
    NL_CHECK_RETURN_RET(sleService, NL_ERR_INTERNAL_ERROR, "sleService invalid.");
    if (sleService->UpdateRefusePolicy(protocolType, pid, refuseTime)) {
        return NL_NO_ERROR;
    }
    return NL_ERR_PROFILE_PROHIBITED_BY_EDM;
}

NlErrCode NearlinkHostServer::CheckPermissionForNapi(const std::string &permission, bool &isGranted)
{
    HILOGI("enter");
    isGranted = NearLinkPermissionManager::VerifyPermission(permission);
    return NL_NO_ERROR;
}

int32_t NearlinkHostServer::OnSvcCmd(int32_t fd, const std::vector<std::u16string>& args)
{
    if (args.empty()) {
        std::string info = "wrong parameter size\n";
        SaveStringToFd(fd, info.append(SVC_HELP_TEXT));
        return SVC_RET_FAILED;
    }
    int32_t svcResult = SVC_RET_FAILED;
    std::string info = "";
    std::string cmd = Str16ToStr8(args[0]);
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);
    HILOGI("svc cmd is %{public}s", cmd.c_str());
    if (cmd == ARGS_HELP) {
        info.append(SVC_HELP_TEXT);
        svcResult = SVC_RET_SUCCESS;
    } else if (cmd == ARGS_ENABLE) {
        if (args.size() != SVC_ARGS_ONE) {
            info = "wrong parameter size\n";
        } else {
            NlErrCode ret = EnableSle();
            info = (ret == NL_NO_ERROR) ? "nearlink enable success\n" : "nearlink enable failed\n";
            svcResult = (ret == NL_NO_ERROR) ? SVC_RET_SUCCESS : SVC_RET_FAILED;
        }
    } else if (cmd == ARGS_DISABLE) {
        if (args.size() != SVC_ARGS_ONE) {
            info = "wrong parameter size\n";
        } else {
            NlErrCode ret = DisableSleBySvc();
            info = (ret == NL_NO_ERROR) ? "nearlink disable success\n" : "nearlink disable failed\n";
            svcResult = (ret == NL_NO_ERROR) ? SVC_RET_SUCCESS : SVC_RET_FAILED;
        }
    } else if (ServiceManagerPluginInterface::GetInstance()->IsLibraryLoaded()) {
        ServiceManagerPluginInterface::GetInstance()->SvcCmdProc(cmd, fd, args, svcResult, info);
    } else {
        info.append("wrong parameter name\n").append(SVC_HELP_TEXT);
    }
    if (!info.empty() && SaveStringToFd(fd, info)) {
        HILOGE("Nearlink device save string to fd failed.");
    }
    return svcResult;
}

NlErrCode NearlinkHostServer::DisableSleBySvc()
{
    HILOGI("Enter!");
    SwitchCallerInfo callerInfo = SleInterfaceManager::GetCallerInfo();
    NlErrCode ret = static_cast<NlErrCode>(SleInterfaceManager::GetInstance()->DisableToOff(
        SleTransport::ADAPTER_SLE, SleEventType::SVC_TRIGGERED, callerInfo));
    if (ret == NL_NO_ERROR || ret == NL_ERR_INVALID_SWITCH_OPERATION) {
        return NL_NO_ERROR;
    }
    return NL_ERR_INTERNAL_ERROR;
}

static bool IsSetBtAddrAllowed()
{
    uint32_t tokenId = IPCSkeleton::GetCallingTokenID();
    int32_t uid = IPCSkeleton::GetCallingUid();
    VerificationContext ctx = { .tokenId = tokenId, .uid = uid };
    bool result = NearlinkVerificationManager::GetInstance().CheckVerification(
        VerificationType::CONTROLLER_BT_ADDR, ctx);
    return result;
}

NlErrCode NearlinkHostServer::GetSleAddrByBtAddr(const std::string &btAddr, std::string &sleAddr)
{
    SleInterfaceAdapterSub *sleService = static_cast<SleInterfaceAdapterSub *>(
        SleInterfaceManager::GetInstance()->GetAdapter(SleTransport::ADAPTER_SLE));
    NL_CHECK_RETURN_RET(sleService, NL_ERR_INTERNAL_ERROR, "sleService invalid.");
    if (sleService->GetSleAddrByBtAddr(btAddr, sleAddr)) {
        HILOGI("sleAddr: %{public}s, btAddr: %{public}s",
            GetEncryptAddr(sleAddr).c_str(), GetEncryptAddr(btAddr).c_str());
        return NL_NO_ERROR;
    }
    return NL_ERR_INTERNAL_ERROR;
}

NlErrCode NearlinkHostServer::GetBtAddrBySleAddr(const std::string &sleAddr, std::string &btAddr)
{
    SleInterfaceAdapterSub *sleService = static_cast<SleInterfaceAdapterSub *>(
        SleInterfaceManager::GetInstance()->GetAdapter(SleTransport::ADAPTER_SLE));
    NL_CHECK_RETURN_RET(sleService, NL_ERR_INTERNAL_ERROR, "sleService invalid.");

    if (sleService->GetBtAddrBySleAddr(sleAddr, btAddr)) {
        HILOGD("sleAddr: %{public}s, btAddr: %{public}s",
            GetEncryptAddr(sleAddr).c_str(), GetEncryptAddr(btAddr).c_str());
        return NL_NO_ERROR;
    }
    return NL_ERR_INTERNAL_ERROR;
}

NlErrCode NearlinkHostServer::SetBtAddrBySleAddr(const std::string &sleAddr, const std::string &btAddr)
{
    NL_CHECK_RETURN_RET(IsSleAvailableToCallerInner(), NL_ERR_SLE_OFF, "nearlink is off.");
    SleInterfaceAdapterSub *sleService = static_cast<SleInterfaceAdapterSub *>(
        SleInterfaceManager::GetInstance()->GetAdapter(SleTransport::ADAPTER_SLE));
    NL_CHECK_RETURN_RET(sleService, NL_ERR_INTERNAL_ERROR, "sleService invalid.");
    NL_CHECK_RETURN_RET(IsSetBtAddrAllowed(), NL_ERR_INTERNAL_ERROR,
                        "no allow to set bt address");
    HILOGD("sleAddr: %{public}s, btAddr: %{public}s", GetEncryptAddr(sleAddr).c_str(), GetEncryptAddr(btAddr).c_str());
    if (sleService->SetBtAddrBySleAddr(sleAddr, btAddr)) {
        return NL_NO_ERROR;
    }
    return NL_ERR_INTERNAL_ERROR;
}

NlErrCode NearlinkHostServer::IsFeatureSupported(int32_t feature, bool &isSupported)
{
    NL_CHECK_RETURN_RET(IsSleAvailableToCallerInner(), NL_ERR_SLE_OFF, "nearlink is off.");
    SleInterfaceAdapterSub *sleService = static_cast<SleInterfaceAdapterSub *>(
        SleInterfaceManager::GetInstance()->GetAdapter(SleTransport::ADAPTER_SLE));
    NL_CHECK_RETURN_RET(sleService, NL_ERR_INTERNAL_ERROR, "sleService invalid.");
    if (sleService->IsFeatureSupported(feature)) {
        isSupported = true;
    }
    HILOGI("isSupported(%{public}d)", isSupported);
    return NL_NO_ERROR;
}

NlErrCode NearlinkHostServer::IsConnectionExist(bool &isConnectionExist)
{
    isConnectionExist = true;
    NL_CHECK_RETURN_RET(IsSleAvailableToCallerInner(), NL_ERR_SLE_OFF, "nearlink is off.");
    SleInterfaceAdapterSub *sleService = static_cast<SleInterfaceAdapterSub *>(
        SleInterfaceManager::GetInstance()->GetAdapter(SleTransport::ADAPTER_SLE));
    NL_CHECK_RETURN_RET(sleService, NL_ERR_INTERNAL_ERROR, "sleService invalid.");

    if (!sleService->HasConnectedDevice()) {
        isConnectionExist = false;
    }
    HILOGI("isConnectionExist(%{public}d)", isConnectionExist);
    return NL_NO_ERROR;
}

NlErrCode NearlinkHostServer::GetBatteryLevel(const std::string &address)
{
    std::string realAddr = "";
    NearlinkDeviceManager::GetInstance()->GetDeviceRealAddr(address, realAddr);
    HILOGI("address: %{public}s", GetEncryptAddr(realAddr).c_str());
    RawAddress addr(realAddr);
    int32_t connState = static_cast<int32_t>(SleConnectState::INVALID_STATE);
    NlErrCode result = GetProfileConnState(address, connState);
    NL_CHECK_RETURN_RET(result == NL_NO_ERROR, result, "get connection state error.");
    NL_CHECK_RETURN_RET(connState == static_cast<int32_t>(SleConnectState::CONNECTED),
                        NL_ERR_CONNECTION_NOT_ESTABLISHED, "No connection established with the peer device.");
    ProfileBas *basService = static_cast<ProfileBas *>
        (SleInterfaceProfileManager::GetInstance().GetProfileService(PROFILE_NAME_BAS));
    NL_CHECK_RETURN_RET(basService, NL_ERR_INTERNAL_ERROR, "basService is nullptr.");
    SwitchCallerInfo caller = SleInterfaceManager::GetCallerInfo();
    bool isReqSent = false;
    pimpl->deviceBatteryObservers_.ForEach([this, &caller, &isReqSent](sptr<INearlinkDeviceBatteryObserver> observer) {
        impl::NearlinkBasRemoteInfo info = pimpl->remoteBatteryContainer_->RetrieveRemoteInfo(observer->AsObject());
        if (info.isSendingReq == true) {
            isReqSent = true;
        }
        if (info.fullToken == caller.fullTokenId && info.uid == caller.callerUid) {
            pimpl->remoteBatteryContainer_->UpdateRemoteInfo(observer->AsObject(), true);
            return;
        }
    });
    if (!isReqSent) {
        basService->GetDeviceBatteryLevel(addr);
    }
    return NL_NO_ERROR;
}
}  // namespace Nearlink
}  // namespace OHOS
