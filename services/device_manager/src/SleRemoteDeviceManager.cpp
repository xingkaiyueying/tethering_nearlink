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

#include "SleRemoteDeviceManager.h"
#include "log_util.h"
#include "nearlink_def.h"
#include "SleConfig.h"
#include "nearlink_common_event_helper.h"
#include "SleHuksTool.h"

namespace OHOS {
namespace Nearlink {
namespace {
constexpr const char* INVALID_NAME = "";
}

SleRemoteDeviceManager::SleRemoteDeviceManager()
{}

SleRemoteDeviceManager::~SleRemoteDeviceManager()
{}

SleRemoteDeviceManager* SleRemoteDeviceManager::GetInstance(void)
{
    static SleRemoteDeviceManager instance;
    return &instance;
}

std::vector<RawAddress> SleRemoteDeviceManager::GetPairedDevices()
{
    std::vector<RawAddress> pairedList;
    auto getPairedList = [&pairedList](std::string key,
        std::shared_ptr<SlePeripheralDevice> value)-> void {
        if (value->GetPairedStatus() == static_cast<int>(SlePairState::SLE_PAIR_PAIRED)) {
            RawAddress rawAddr(value->GetRawAddress());
            pairedList.push_back(rawAddr);
        }
    };
    peerConnDeviceSafeList_.Iterate(getPairedList);
    return pairedList;
}

std::vector<RawAddress> SleRemoteDeviceManager::GetConnectedDevices()
{
    std::vector<RawAddress> connectedList;
    auto getConnectedList = [&connectedList](std::string key,
        std::shared_ptr<SlePeripheralDevice> value)-> void {
        RawAddress rawAddr(value->GetRawAddress());
        connectedList.push_back(rawAddr);
    };
    peerConnDeviceSafeList_.Iterate(getConnectedList);
    return connectedList;
}

std::vector<RawAddress> SleRemoteDeviceManager::GetConnectingDevices()
{
    std::vector<RawAddress> connectingList;
    auto getConnectingList = [&connectingList](std::string key,
        std::shared_ptr<SlePeripheralDevice> value)-> void {
        if (value->GetAcbConnectState() == static_cast<int>(SleConnState::SLE_CONNECTION_STATE_CONNECTING)) {
            RawAddress rawAddr(value->GetRawAddress());
            connectingList.push_back(rawAddr);
        }
    };
    peerConnDeviceSafeList_.Iterate(getConnectingList);
    return connectingList;
}

std::vector<RawAddress> SleRemoteDeviceManager::GetAcbConnectedDevices()
{
    std::vector<RawAddress> connectingList;
    auto getConnectingList = [&connectingList](std::string key, std::shared_ptr<SlePeripheralDevice> value) -> void {
        if ((value->GetAcbConnectState() == static_cast<int>(SleConnState::SLE_CONNECTION_STATE_CONNECTED)) ||
            (value->GetAcbConnectState() == static_cast<int>(SleConnState::SLE_CONNECTION_STATE_ENCRYPTED))) {
            RawAddress rawAddr(value->GetRawAddress());
            connectingList.push_back(rawAddr);
        }
    };
    peerConnDeviceSafeList_.Iterate(getConnectingList);
    return connectingList;
}

std::string SleRemoteDeviceManager::GetDeviceName(const RawAddress &device)
{
    std::string remoteName = INVALID_NAME;
    auto func = [&remoteName](std::string key, std::shared_ptr<SlePeripheralDevice> value) -> void {
        remoteName = value->GetName();
    };
    peerConnDeviceSafeList_.GetValueAndOpt(device.GetAddress(), func);
    return remoteName;
}

std::string SleRemoteDeviceManager::GetAliasName(const RawAddress &device)
{
    std::string name = INVALID_NAME;
    auto func = [&name](std::string key, std::shared_ptr<SlePeripheralDevice> value) -> void {
        name = value->GetAliasName();
        name = (name == INVALID_NAME) ? value->GetName() : name;
    };
    peerConnDeviceSafeList_.GetValueAndOpt(device.GetAddress(), func);
    return name;
}

bool SleRemoteDeviceManager::SetAliasName(const RawAddress &device, const std::string &name)
{
    auto setAlias = [&name](const std::string &key, std::shared_ptr<SlePeripheralDevice> &value) -> void {
        if (name.compare(value->GetAliasName()) != 0) {
            value->SetAliasName(name);
            SleConfig::GetInstance().SetPeerAlias(key, name);
            SleConfig::GetInstance().Save();
            NearlinkHelper::NearlinkCommonEventHelper::PublishRemoteNameChangedEvent(key, name);
        } else {
            HILOGI("name is same");
        }
    };
    return peerConnDeviceSafeList_.GetValueAndOpt(device.GetAddress(), setAlias);
}

bool SleRemoteDeviceManager::SetName(const RawAddress &device, const std::string &name)
{
    auto setName = [&name](const std::string &key, std::shared_ptr<SlePeripheralDevice> &value) -> void {
        if (name != value->GetName()) {
            value->SetName(name);
            SleConfig::GetInstance().SetPeerName(key, name);
            SleConfig::GetInstance().Save();
        } else {
            HILOGI("name is same");
        }
    };
    return peerConnDeviceSafeList_.GetValueAndOpt(device.GetAddress(), setName);
}

bool SleRemoteDeviceManager::SetAppearance(const RawAddress &device, const int appearance)
{
    auto setAppearance =[&appearance](
        const std::string &key, std::shared_ptr<SlePeripheralDevice> &value) -> void {
        if (appearance != value->GetAppearance()) {
            value->SetAppearance(appearance);
            SleConfig::GetInstance().SetPeerAppearance(key, appearance);
            SleConfig::GetInstance().Save();
        }
    };
    return peerConnDeviceSafeList_.GetValueAndOpt(device.GetAddress(), setAppearance);
}

int SleRemoteDeviceManager::GetDeviceAppearance(const RawAddress &device)
{
    int appearance = INVALID_APPEARANCE;
    auto func = [&appearance](std::string key, std::shared_ptr<SlePeripheralDevice> value) -> void {
        appearance = value->GetAppearance();
    };
    peerConnDeviceSafeList_.GetValueAndOpt(device.GetAddress(), func);
    return appearance;
}

std::vector<Uuid> SleRemoteDeviceManager::GetDeviceUuids(const RawAddress &device)
{
    std::vector<Uuid> uuids;
    peerConnDeviceSafeList_.GetValueAndOpt(device.GetAddress(),
        [&uuids](std::string key, std::shared_ptr<SlePeripheralDevice> value) -> void {
            uuids = value->GetServiceUUID();
    });
    return uuids;
}

bool SleRemoteDeviceManager::IsBondedFromLocal(const RawAddress &device)
{
    bool isBondedFromLocal = false;
    peerConnDeviceSafeList_.GetValueAndOpt(device.GetAddress(), [&isBondedFromLocal](
        std::string key, std::shared_ptr<SlePeripheralDevice> value) {
            isBondedFromLocal = value->IsAcbEncrypted();
        });
    return isBondedFromLocal;
}

bool SleRemoteDeviceManager::IsAcbConnected(const RawAddress &device)
{
    bool isAcbConnected = false;
    peerConnDeviceSafeList_.GetValueAndOpt(device.GetAddress(), [&isAcbConnected](
        std::string key, std::shared_ptr<SlePeripheralDevice> value) {
        isAcbConnected = value->IsAcbConnected();
    });
    LOG_INFO("addr: %{public}s, connected:%{public}d", GetEncryptAddr(device.GetAddress()).c_str(), isAcbConnected);
    return isAcbConnected;
}

bool SleRemoteDeviceManager::IsAcbEncrypted(const RawAddress &device)
{
    bool isAcbEncrypted = false;
    peerConnDeviceSafeList_.GetValueAndOpt(device.GetAddress(), [&isAcbEncrypted](
        std::string key, std::shared_ptr<SlePeripheralDevice> value) {
        isAcbEncrypted = value->IsAcbEncrypted();
    });
    return isAcbEncrypted;
}

uint8_t SleRemoteDeviceManager::GetLinkRole(const RawAddress &device)
{
    uint8_t role = SLE_INVALID_LINK_ROLE;
    peerConnDeviceSafeList_.GetValueAndOpt(device.GetAddress(), [&role](
        std::string key, std::shared_ptr<SlePeripheralDevice> value) {
        role = value->GetLinkRole();
    });
    return role;
}

uint16_t SleRemoteDeviceManager::GetLcidByAddress(const RawAddress &device)
{
    uint16_t lcid = INVALID_LCID;
    peerConnDeviceSafeList_.GetValueAndOpt(device.GetAddress(), [&lcid](
        std::string key, std::shared_ptr<SlePeripheralDevice> value) {
        lcid = value->GetLcid();
    });
    return lcid;
}

std::string SleRemoteDeviceManager::GetAddressByLcid(uint16_t lcid)
{
    std::string addr = INVALID_MAC_ADDRESS;
    peerConnDeviceSafeList_.Find(
        [&addr, lcid](std::string key, std::shared_ptr<SlePeripheralDevice> value)->bool {
        if (value->GetLcid() == lcid) {
            addr = key;
            return true;
        }
        return false;
    });
    return addr;
}

int SleRemoteDeviceManager::GetAcbState(const std::string &address)
{
    int acbState = static_cast<int>(SleConnState::SLE_CONNECTION_STATE_DISCONNECTED);

    auto func = [&acbState](std::string key, std::shared_ptr<SlePeripheralDevice> value) -> void {
        acbState = value->GetAcbConnectState();
    };
    if (!peerConnDeviceSafeList_.GetValueAndOpt(address, func)) {
        HILOGI("device doesn't exist");
    }
    LOG_INFO("addr: %{public}s, state:%{public}d", GetEncryptAddr(address).c_str(), acbState);
    return acbState;
}

bool SleRemoteDeviceManager::SetAcbState(const std::string &address, int connectState)
{
    return peerConnDeviceSafeList_.GetValueAndOpt(address, [connectState](
        std::string key, std::shared_ptr<SlePeripheralDevice> value) {
        value->SetAcbConnectState(connectState);
    });
}

bool SleRemoteDeviceManager::SetAcbDisConnReason(const std::string &address, int reason)
{
    return peerConnDeviceSafeList_.GetValueAndOpt(address, [reason](
        std::string key, std::shared_ptr<SlePeripheralDevice> value) {
        value->SetAcbDisConnReason(reason);
    });
}

int SleRemoteDeviceManager::GetAcbDisConnReason(const std::string &address)
{
    int reason = 0;
    auto func = [&reason](std::string key, std::shared_ptr<SlePeripheralDevice> value) -> void {
        reason = value->GetAcbDisConnReason();
    };
    if (!peerConnDeviceSafeList_.GetValueAndOpt(address, func)) {
        HILOGI("device doesn't exist");
    }
    return reason;
}

bool SleRemoteDeviceManager::SetLcid(const std::string &address, uint16_t lcid)
{
    return peerConnDeviceSafeList_.GetValueAndOpt(address, [lcid](
        std::string key, std::shared_ptr<SlePeripheralDevice> value) {
        value->SetLcid(lcid);
    });
}

bool SleRemoteDeviceManager::GetPairAlgoInfo(
    const RawAddress &addr, uint8_t &cryptoAlgo, uint8_t &keyDerivAlgo, uint8_t &integrChkInd)
{
    return peerConnDeviceSafeList_.GetValueAndOpt(addr.GetAddress(), [&cryptoAlgo, &keyDerivAlgo, &integrChkInd]
        (const std::string key, std::shared_ptr<SlePeripheralDevice> &value) {
            cryptoAlgo = value->GetCryptoAlgo();
            keyDerivAlgo = value->GetKeyDerivAlgo();
            integrChkInd = value->GetIntegrChkInd();
        });
}

bool SleRemoteDeviceManager::SetPairAlgoInfo(
    const RawAddress &addr, uint8_t cryptoAlgo, uint8_t keyDerivAlgo, uint8_t integrChkInd)
{
    return peerConnDeviceSafeList_.GetValueAndOpt(addr.GetAddress(),
        [cryptoAlgo, keyDerivAlgo, integrChkInd](const std::string key, std::shared_ptr<SlePeripheralDevice> &value) {
            value->SetCryptoAlgo(cryptoAlgo);
            value->SetKeyDerivAlgo(keyDerivAlgo);
            value->SetIntegrChkInd(integrChkInd);
        });
}

bool SleRemoteDeviceManager::GetGroupAndGiv(const RawAddress &addr, std::string &encryptGroupKeyStr, uint64_t &giv)
{
    return peerConnDeviceSafeList_.GetValueAndOpt(addr.GetAddress(),
        [&encryptGroupKeyStr, &giv](const std::string key, std::shared_ptr<SlePeripheralDevice> &value) {
            encryptGroupKeyStr = value->GetEncryptGroupKeyStr();
            giv = value->GetGiv();
        });
}

bool SleRemoteDeviceManager::SetGroupAndGiv(const RawAddress &addr, std::string encryptGroupKeyStr, uint64_t giv)
{
    return peerConnDeviceSafeList_.GetValueAndOpt(addr.GetAddress(),
        [encryptGroupKeyStr, giv](const std::string key, std::shared_ptr<SlePeripheralDevice> &value) {
            value->SetEncryptGroupKeyStr(encryptGroupKeyStr);
            value->SetGiv(giv);
        });
}

bool SleRemoteDeviceManager::HasConnectedDevice()
{
    return peerConnDeviceSafeList_.Find(
        [](std::string key, std::shared_ptr<SlePeripheralDevice> value)-> bool {
        if (value->GetAcbConnectState() == static_cast<int>(SleConnState::SLE_CONNECTION_STATE_CONNECTED) ||
            value->GetAcbConnectState() == static_cast<int>(SleConnState::SLE_CONNECTION_STATE_ENCRYPTED)) {
            LOG_INFO("[SleRemoteDeviceManager]%{public}s is connected",
                GetEncryptAddr(value->GetRawAddress().GetAddress()).c_str());
            return true;
        }
        return false;
    });
}

int SleRemoteDeviceManager::GetConnDirect(const RawAddress &device)
{
    int connDirect = static_cast<int>(SleConnDirect::SLE_CONNECTION_PASSIVE);
    peerConnDeviceSafeList_.GetValueAndOpt(device.GetAddress(), [&connDirect](
        std::string key, std::shared_ptr<SlePeripheralDevice> value) {
        connDirect = value->GetConnDirect();
    });
    return connDirect;
}

/* 获取广播中的业务类型 */
int SleRemoteDeviceManager::GetManufacturerBusinessType(const RawAddress &device)
{
    int businessType = 0;
    auto getType = [&businessType](std::string key, std::shared_ptr<SlePeripheralDevice> value) -> void {
        businessType = value->GetManufacturerBusiness();
    };
    bool res = peerConnDeviceSafeList_.GetValueAndOpt(device.GetAddress(), getType);
    HILOGD("[SleRemoteDeviceManager]:peer addr:%{public}s,bussiness type:%{public}d, res:%{public}d",
                GET_ENCRYPT_ADDR(device), businessType, res);
    return businessType;
}

void SleRemoteDeviceManager::SetConnDirect(const RawAddress &device, int connDirect)
{
    peerConnDeviceSafeList_.GetValueAndOpt(device.GetAddress(), [&connDirect](
        std::string key, std::shared_ptr<SlePeripheralDevice> value) {
        value->SetConnDirect(connDirect);
    });
}

void SleRemoteDeviceManager::SetConnDirectActive(const RawAddress &device)
{
    peerConnDeviceSafeList_.GetValueAndOpt(device.GetAddress(), [](
        std::string key, std::shared_ptr<SlePeripheralDevice> value) {
        int acbConnState = value->GetAcbConnectState();
        if (acbConnState == static_cast<int>(SleConnState::SLE_CONNECTION_STATE_DISCONNECTED)) {
            // 如果当前ACB未建立，需要增加记录active
            value->SetConnDirect(static_cast<int>(SleConnDirect::SLE_CONNECTION_ACTIVE));
        }
    });
}

int SleRemoteDeviceManager::GetPairState(const RawAddress &device)
{
    int pairState = static_cast<int>(SlePairState::SLE_PAIR_NONE);
    auto getPair = [&pairState](std::string key, std::shared_ptr<SlePeripheralDevice> value) {
        pairState = value->GetPairedStatus();
    };
    peerConnDeviceSafeList_.GetValueAndOpt(device.GetAddress(), getPair);
    return pairState;
}

bool SleRemoteDeviceManager::SetPairStatus(const RawAddress &device, int pairStatus)
{
    return peerConnDeviceSafeList_.GetValueAndOpt(device.GetAddress(), [pairStatus](
        std::string key, std::shared_ptr<SlePeripheralDevice> value) {
        value->SetPairedStatus(pairStatus);
    });
}

bool SleRemoteDeviceManager::SetPrePairStatus(const RawAddress &device, int pairStatus)
{
    return peerConnDeviceSafeList_.GetValueAndOpt(device.GetAddress(), [pairStatus](
        std::string key, std::shared_ptr<SlePeripheralDevice> value) {
        value->SetPrePairedStatus(pairStatus);
    });
}

void SleRemoteDeviceManager::SetDeviceIsAvailable(const RawAddress &device, bool isAvailable)
{
    bool ret = peerConnDeviceSafeList_.GetValueAndOpt(device.GetAddress(),
        [device, isAvailable](std::string key, std::shared_ptr<SlePeripheralDevice> value) {
        value->SetIsDeviceAvailable(isAvailable);
    });
    if (!ret) {
        HILOGI("Get SlePeripheralDevice failed!");
    }
    SleConfig::GetInstance().SetAvailableControl(device.GetAddress(), isAvailable);
    SleConfig::GetInstance().Save();
}

int SleRemoteDeviceManager::GetConnectedCnt()
{
    int count = 0;
    peerConnDeviceSafeList_.Iterate([&count](
        const std::string first, std::shared_ptr<SlePeripheralDevice> &second) {
        if (second->IsAcbConnected()) {
            count++;
        }
    });
    return count;
}

void SleRemoteDeviceManager::AddPeripheralDevice(const std::string &address,
    std::shared_ptr<SlePeripheralDevice> &peerDevice)
{
    peerConnDeviceSafeList_.EnsureInsert(address, peerDevice);
}

void SleRemoteDeviceManager::RemovePeripheralDevice(const std::string &address)
{
    peerConnDeviceSafeList_.Erase(address);
}

void SleRemoteDeviceManager::RemoveAllPeripheralDevices()
{
    peerConnDeviceSafeList_.Clear();
}

std::shared_ptr<SlePeripheralDevice> SleRemoteDeviceManager::GetRemoteDevice(const RawAddress &device)
{
    std::shared_ptr<SlePeripheralDevice> remoteDevice = nullptr;
    peerConnDeviceSafeList_.GetValue(device.GetAddress(), remoteDevice);
    // 返回新对象的 shared_ptr（深拷贝）
    if (remoteDevice) {
        return std::make_shared<SlePeripheralDevice>(*remoteDevice);
    }
    return nullptr;
}

uint8_t SleRemoteDeviceManager::GetPeerDeviceAddrType(const RawAddress &device)
{
    uint8_t type = static_cast<uint8_t>(SLE_ADDR_TYPE::SLE_PUBLIC_ADDRESS_TYPE);
    peerConnDeviceSafeList_.GetValueAndOpt(device.GetAddress(),
        [&type](std::string key, std::shared_ptr<SlePeripheralDevice> value) -> void {
            type = value->GetAddressType();
    });
    return type;
}

bool SleRemoteDeviceManager::SetBtAddrBySleAddr(const std::string &sleAddr, const std::string &btAddr)
{
    bool ret = peerConnDeviceSafeList_.GetValueAndOpt(
        sleAddr, [sleAddr, btAddr](const std::string &key,
            std::shared_ptr<SlePeripheralDevice> &value) -> void {
            if (value == nullptr) {
                return;
            }
            if (btAddr.compare(value->GetBtAddr()) != 0) {
                value->SetBtAddr(btAddr);
            }
        });
    if (ret) {
        SleConfig::GetInstance().SetBtAddrBySleAddr(sleAddr, btAddr);
        SleConfig::GetInstance().Save();
    }
    return ret;
}

bool SleRemoteDeviceManager::GetBtAddrBySleAddr(const std::string &sleAddr, std::string &btAddr)
{
    return peerConnDeviceSafeList_.GetValueAndOpt(sleAddr,
        [&btAddr](const std::string &key, std::shared_ptr<SlePeripheralDevice> &value) -> void {
            btAddr = value->GetBtAddr();
        });
}

bool SleRemoteDeviceManager::GetSleAddrByBtAddr(const std::string &btAddr, std::string &sleAddr)
{
    return peerConnDeviceSafeList_.Find(
        [&btAddr, &sleAddr](const std::string &key, std::shared_ptr<SlePeripheralDevice> &value) -> bool {
            if (value != nullptr && value->GetBtAddr() == btAddr) {
                sleAddr = key;
                return true;
            }
            return false;
        });
}

bool SleRemoteDeviceManager::SetCdsmAddrType(const RawAddress &device, int addrType)
{
    auto func = [addrType](std::string key, std::shared_ptr<SlePeripheralDevice> value) -> void {
        value->SetCdsmAddrType(addrType);
    };
    return peerConnDeviceSafeList_.GetValueAndOpt(device.GetAddress(), func);
}

bool SleRemoteDeviceManager::IsAudioDevice(const std::string &address)
{
    bool isAudioDevice = false;
    peerConnDeviceSafeList_.GetValueAndOpt(address,
        [&isAudioDevice](std::string key, std::shared_ptr<SlePeripheralDevice> value) {
            isAudioDevice = value->GetIsAudioDeviceFlag();
    });
    return isAudioDevice;
}

bool SleRemoteDeviceManager::IsServiceSupportedConn(const RawAddress &device)
{
    bool ret = true;
    peerConnDeviceSafeList_.GetValueAndOpt(device.GetAddress(),
        [&ret, device](std::string key, std::shared_ptr<SlePeripheralDevice> value) {
            HILOGI("No avaiable service, connection not allowed :  %{public}s", GET_ENCRYPT_ADDR(device));
            if (!value->GetIsDeviceAvailable()) {
                ret = false;
            }
    });
    return ret;
}

void SleRemoteDeviceManager::SaveDeviceModelInfo(const std::string &address, const DeviceModel &model,
    const std::string &newModelId)
{
    HILOGI("[SleRemoteDeviceManager]SaveDeviceModelInfo: device:%{public}s, modelId:%{public}s, "
        "newModelId:%{public}s, iconId:%{public}s, devType:%{public}s", GetEncryptAddr(address).c_str(),
        model.GetModelId().c_str(), newModelId.c_str(), model.GetIconId().c_str(), model.GetDevType().c_str());
    auto func = [&address, &model, &newModelId]
        (std::string, std::shared_ptr<SlePeripheralDevice> value) -> void {
        if (!model.GetModelId().empty()) {
            value->SetModelId(model.GetModelId());
            SleConfig::GetInstance().SetDeviceModelId(address, value->GetModelId());
        }
        if (!newModelId.empty()) {
            value->SetNewModelId(newModelId);
            SleConfig::GetInstance().SetDeviceNewModelId(address, value->GetNewModelId());
        }
        if (!model.GetIconId().empty()) {
            value->SetIconId(model.GetIconId());
            SleConfig::GetInstance().SetDeviceIconId(address, value->GetIconId());
        }
        if (!model.GetDevType().empty()) {
            value->SetDevType(model.GetDevType());
            SleConfig::GetInstance().SetDeviceDevType(address, value->GetDevType());
        }
        SleConfig::GetInstance().Save();
    };
    bool res = peerConnDeviceSafeList_.GetValueAndOpt(address, func);
    HILOGI("update device(%{public}s) model info ret:%{public}d", GetEncryptAddr(address).c_str(), res);
}

bool SleRemoteDeviceManager::GetDeviceModelInfo(const RawAddress &device, DeviceModel &model,
    std::string &newModelId)
{
    auto dev = GetRemoteDevice(device);
    if (dev != nullptr) {
        model.SetModelId(dev->GetModelId());
        model.SetSubModelId(dev->GetSubModelId());
        model.SetIconId(dev->GetIconId());
        model.SetDevType(dev->GetDevType());
        newModelId = dev->GetNewModelId();
        return true;
    }
    return false;
}

void SleRemoteDeviceManager::SetManufacturerAbility(const RawAddress &device,
    const std::array<uint8_t, SLE_MANU_ABILITY_LEN> &manufacturerAbility)
{
    peerConnDeviceSafeList_.GetValueAndOpt(device.GetAddress(),
        [manufacturerAbility](const std::string key, std::shared_ptr<SlePeripheralDevice> value) -> void {
            value->SetManufacturerAbility(manufacturerAbility);
    });
}

std::array<uint8_t, SLE_MANU_ABILITY_LEN> SleRemoteDeviceManager::GetManufacturerAbility(const RawAddress &device)
{
    std::array<uint8_t, SLE_MANU_ABILITY_LEN> manufacturerAbility = {};
    peerConnDeviceSafeList_.GetValueAndOpt(device.GetAddress(),
        [&manufacturerAbility](const std::string key, std::shared_ptr<SlePeripheralDevice> value) -> void {
            manufacturerAbility = value->GetManufacturerAbility();
    });
    return manufacturerAbility;
}

void SleRemoteDeviceManager::SetPairDirection(const RawAddress &device, int pairDirection)
{
    peerConnDeviceSafeList_.GetValueAndOpt(device.GetAddress(),
        [pairDirection](std::string key, std::shared_ptr<SlePeripheralDevice> value) {
            value->SetPairDirection(pairDirection);
    });
}

int SleRemoteDeviceManager::GetPairDirection(const RawAddress &device)
{
    int pairDirection = static_cast<int>(SlePairDirect::SLE_PAIR_PASSIVE);
    peerConnDeviceSafeList_.GetValueAndOpt(device.GetAddress(),
        [&pairDirection](std::string key, std::shared_ptr<SlePeripheralDevice> value) {
            pairDirection = value->GetPairDirection();
    });
    return pairDirection;
}

bool SleRemoteDeviceManager::GetIsDeviceAvailable(const RawAddress &device)
{
    bool isAvailable = true;
    peerConnDeviceSafeList_.GetValueAndOpt(device.GetAddress(),
        [&isAvailable](std::string key, std::shared_ptr<SlePeripheralDevice> value) {
            isAvailable = value->GetIsDeviceAvailable();
    });
    return isAvailable;
}

bool SleRemoteDeviceManager::GetIsAudioDeviceFlag(const RawAddress &device)
{
    bool isAudioDevice = false;
    peerConnDeviceSafeList_.GetValueAndOpt(device.GetAddress(),
        [&isAudioDevice](std::string key, std::shared_ptr<SlePeripheralDevice> value) {
            isAudioDevice = value->GetIsAudioDeviceFlag();
    });
    return isAudioDevice;
}

std::vector<RawAddress> SleRemoteDeviceManager::GetDirectConnDevices()
{
    std::vector<RawAddress> directConnList;
    auto getDirectConn = [&directConnList](std::string key,
        std::shared_ptr<SlePeripheralDevice> value)-> void {
        if (value->GetPairedStatus() != static_cast<int>(SlePairState::SLE_PAIR_PAIRED)) {
            return;
        }
        // 被动配对不回连，车钥匙场景不回连，路由器不回连，不可用设备不回连
        if (value->GetPairDirection() == static_cast<int>(SlePairDirect::SLE_PAIR_PASSIVE) ||
            value->GetAppearance() == static_cast<int>(DeviceClassForService::DEVICE_CLASS_VEHICLE_LOCK) ||
            value->GetAppearance() == static_cast<int>(DeviceClassForService::DEVICE_NETWORKING) ||
            !value->GetIsDeviceAvailable()) {
            return;
        }
#ifndef NON_AUDIO_DEVICE_DIRECT_CONN
        // 非音频设备不回连（走背景回连）
        if (!value->GetIsAudioDeviceFlag()) {
            return;
        }
#endif
        RawAddress rawAddr(value->GetRawAddress());
        directConnList.push_back(rawAddr);
    };
    peerConnDeviceSafeList_.Iterate(getDirectConn);
    return directConnList;
}

void SleRemoteDeviceManager::SetIsAudioDeviceFlag(const RawAddress &device)
{
    peerConnDeviceSafeList_.GetValueAndOpt(device.GetAddress(),
        [](std::string key, std::shared_ptr<SlePeripheralDevice> value) {
            value->SetIsAudioDeviceFlag(true);
            SleConfig::GetInstance().SetIsAudioDeviceFlag(key, true);
            SleConfig::GetInstance().Save();
    });
}

bool SleRemoteDeviceManager::GetIsUserDisconnected(const RawAddress &device)
{
    bool isUserDisconnected = false;
    peerConnDeviceSafeList_.GetValueAndOpt(device.GetAddress(),
        [&isUserDisconnected](std::string key, std::shared_ptr<SlePeripheralDevice> value) {
            isUserDisconnected = value->GetIsUserDisconnected();
    });
    return isUserDisconnected;
}

int SleRemoteDeviceManager::GetEncryptedDevicesCount(const std::vector<RawAddress> &devices)
{
    int encryptedCnt = 0;
    for (const auto &device : devices) {
        int acbState = GetAcbState(device.GetAddress());
        if (acbState == static_cast<int>(SleConnState::SLE_CONNECTION_STATE_ENCRYPTED)) {
            encryptedCnt++;
        }
    }
    return encryptedCnt;
}

bool SleRemoteDeviceManager::SetConnectionInfo(const RawAddress &device, uint16_t lcid, uint8_t role, uint8_t addrType)
{
    return peerConnDeviceSafeList_.GetValueAndOpt(device.GetAddress(),
        [lcid, role, addrType](std::string key, std::shared_ptr<SlePeripheralDevice> value) {
            value->SetLcid(lcid);
            value->SetRoles(role);
            value->SetAddressType(addrType);
            value->SetAcbConnectState(static_cast<int>(SleConnState::SLE_CONNECTION_STATE_CONNECTED));
    });
}

void SleRemoteDeviceManager::SaveCdsmInfo(const RawAddress &reportAddr, bool isPrivate,
    const std::vector<std::string> &cdsmDevList)
{
    SleConfig::GetInstance().SetCdsmIsPrivateDevice(reportAddr.GetAddress(), static_cast<int>(isPrivate));
    SleConfig::GetInstance().SetCdsmMemberList(reportAddr.GetAddress(), cdsmDevList);
    SleConfig::GetInstance().Save();
}

int SleRemoteDeviceManager::GetCdsmAddrType(const RawAddress &device)
{
    int cdsmAddrType = static_cast<int>(SleCdsmAddrType::CDSM_TYPE_NONE);
    peerConnDeviceSafeList_.GetValueAndOpt(device.GetAddress(),
        [&cdsmAddrType](std::string key, std::shared_ptr<SlePeripheralDevice> value) {
            cdsmAddrType = value->GetCdsmAddrType();
    });
    return cdsmAddrType;
}

int SleRemoteDeviceManager::GetManufacturerBusinessTypeExt(const RawAddress &device)
{
    int manuBusiness = 0;
    peerConnDeviceSafeList_.GetValueAndOpt(device.GetAddress(),
        [&manuBusiness](std::string key, std::shared_ptr<SlePeripheralDevice> value) {
            manuBusiness = value->GetManufacturerBusiness();
    });
    return manuBusiness;
}

bool SleRemoteDeviceManager::SaveDeviceModelInfoToConf(const RawAddress &device,
    const std::shared_ptr<SlePeripheralDevice> &value)
{
    bool ret = false;
    /* HiLink相关设备信息 */
    std::string modelId = value->GetModelId();
    if (!modelId.empty()) {
        ret &= SleConfig::GetInstance().SetDeviceModelId(device.GetAddress(), modelId);
    }
    std::string newModelId = value->GetNewModelId();
    if (!newModelId.empty()) {
        ret &= SleConfig::GetInstance().SetDeviceNewModelId(device.GetAddress(), newModelId);
    }
    std::string subModelId = value->GetSubModelId();
    if (!subModelId.empty()) {
        ret &= SleConfig::GetInstance().SetDeviceSubModelId(device.GetAddress(), subModelId);
    }
    std::string iconId = value->GetIconId();
    if (!iconId.empty()) {
        ret &= SleConfig::GetInstance().SetDeviceIconId(device.GetAddress(), iconId);
    }
    return ret;
}

bool SleRemoteDeviceManager::SavePeerDeviceInfoToConf()
{
    LOG_INFO("[SleRemoteDeviceManager] enter");
    bool ret = false;
    auto saveToConf = [this, &ret](std::string key, std::shared_ptr<SlePeripheralDevice> value) -> void {
        if (value->GetPairedStatus() != static_cast<int>(SlePairState::SLE_PAIR_PAIRED)) {
            return;
        }
        RawAddress rawAddr(value->GetRawAddress());

        uint8_t addrType = value->GetAddressType();
        ret = SleConfig::GetInstance().SetPeerAddressType(rawAddr.GetAddress(), addrType);

        ret &= SleConfig::GetInstance().SetPeerName(rawAddr.GetAddress(), value->GetName());

        ret &= SleConfig::GetInstance().SetPeerAlias(rawAddr.GetAddress(), value->GetAliasName());

        ret &= SleConfig::GetInstance().SetPeerAppearance(rawAddr.GetAddress(), value->GetAppearance());
        ret &= SaveDeviceModelInfoToConf(rawAddr, value);
        /* 配对信息xml存储合作集成员标识 */
        ret &= SleConfig::GetInstance().SetCdsmAddrType(rawAddr.GetAddress(), value->GetCdsmAddrType());
        ret &= SleConfig::GetInstance().SetIsAudioDeviceFlag(rawAddr.GetAddress(), value->GetIsAudioDeviceFlag());
        ret &= SleConfig::GetInstance().SetSleBusiness(rawAddr.GetAddress(), value->GetManufacturerBusiness());

        std::array<uint8_t, SLE_MANU_ABILITY_LEN> item = value->GetManufacturerAbility();
        std::string manuAbility = SleUtils::ConvertIntToHexString(item.data(), SLE_MANU_ABILITY_LEN);
        ret &= SleConfig::GetInstance().SetManufacturerAbility(rawAddr.GetAddress(), manuAbility);
    };
    peerConnDeviceSafeList_.Iterate(saveToConf);

    SleConfig::GetInstance().Save();
    return ret;
}

} // namespace Nearlink
} // namespace OHOS