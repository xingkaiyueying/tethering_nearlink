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

#include <condition_variable>
#include <memory>
#include <set>
#include "nearlink_host.h"
#include "nearlink_sa_manager.h"
#include "log_util.h"
#include "nearlink_host_proxy.h"
#include "nearlink_ssap_server_proxy.h"
#include "nearlink_ssap_server_callback_stub.h"
#include "nearlink_safe_map.h"
#include "nearlink_safe_list.h"
#include "ssap_data.h"
#include "iservice_registry.h"
#include "raw_address.h"
#include "system_ability_definition.h"
#include "nearlink_ssap_server.h"

namespace OHOS {
namespace Nearlink {

struct RequestInformation {
    uint8_t type;
    SsapDevice device;
    union {
        SsapProperty *property_;
        SsapMethod *method_;
        SsapEvent *event_;
        SsapDescriptor *descriptor_;
    } context_;

    RequestInformation(uint8_t type, const SsapDevice &device, SsapProperty *property)
        : type(type), device(device), context_ {
            .property_ = property
        }
    {}

    RequestInformation(uint8_t type, const SsapDevice &device, SsapMethod *method)
        : type(type), device(device), context_ {
            .method_ = method
        }
    {}

    RequestInformation(uint8_t type, const SsapDevice &device, SsapEvent *event)
        : type(type), device(device), context_ {
            .event_ = event
        }
    {}

    RequestInformation(uint8_t type, const SsapDevice &device, SsapDescriptor *descriptor)
        : type(type), device(device), context_ {
            .descriptor_ = descriptor
        }
    {}

    RequestInformation(uint8_t type, const SsapDevice &device) : type(type), device(device)
    {}

    bool operator==(const RequestInformation &rhs) const
    {
        return (device == rhs.device && type == rhs.type);
    };

    bool operator<(const RequestInformation &rhs) const
    {
        if (device < rhs.device) {
            return true;
        }
        if (rhs.device < device) {
            return false;
        }
        return type < rhs.type;
    };
};

struct SsapServer::impl : public std::enable_shared_from_this<impl> {
    class NearlinkSsapServerCallbackStubImpl;
    bool isRegisterSucceeded_;
    std::mutex requestListMutex_;
    NearlinkSafeMap<uint16_t, std::shared_ptr<SsapService>> ssapServices;
    sptr<NearlinkSsapServerCallbackStubImpl> serviceCallback_;
    std::set<RequestInformation> requests_;
    NearlinkSafeList<std::shared_ptr<SsapDevice>> connectedDevices;
    std::shared_ptr<SsapServerCallback> callback_;
    int applicationId_ = 0;
    std::shared_ptr<SsapService> GetIncludeService(uint16_t handle);
    std::shared_ptr<SsapDevice> FindConnectedDevice(const NearlinkRemoteDevice &device);
    std::shared_ptr<SsapService> BuildService(const NearlinkSsapServiceParcel &service);
    void BuildIncludeService(SsapService &svc, const std::vector<Service> &iSvcs);
    impl(std::shared_ptr<SsapServerCallback> callback);
    ~impl();
    void Init(std::weak_ptr<SsapServer>);
    int32_t profileRegisterId_{0};
};

class SsapServer::impl::NearlinkSsapServerCallbackStubImpl : public NearlinkSsapServerCallbackStub {
public:
    explicit NearlinkSsapServerCallbackStubImpl(std::weak_ptr<SsapServer> server) : server_(server)
    {
        HILOGD("enter");
    }

    ~NearlinkSsapServerCallbackStubImpl() override
    {
        HILOGD("enter");
    }

    inline std::shared_ptr<SsapServer> GetServerSptr(void)
    {
        auto serverSptr = server_.lock();
        if (!serverSptr) {
            HILOGE("server_ is nullptr");
            return nullptr;
        }
        if (!serverSptr->pimpl) {
            HILOGE("impl is nullptr");
            return nullptr;
        }
        return serverSptr;
    }

    void OnConnectionStateChanged(const NearlinkSsapDevice &device, uint8_t state, int reason) override
    {
        HILOGD("ssapServer conn state updated, remote device: %{public}s, state: %{public}s reason: 0x%{public}x",
            GET_ENCRYPT_SSAP_ADDR(device), GetConnStateString(state).c_str(), reason);
        auto serverSptr = GetServerSptr();
        NL_CHECK_RETURN(serverSptr && serverSptr->pimpl && serverSptr->pimpl->callback_,
            "serverSptr pimpl or callback_ is nullptr");
        std::shared_ptr<SsapDevice> dev = std::make_shared<SsapDevice>(device.addr_, device.transport_);
        if (state == static_cast<int>(SleConnectState::CONNECTED)) {
            serverSptr->pimpl->connectedDevices.Insert(dev);
        } else if (state == static_cast<int>(SleConnectState::DISCONNECTED)) {
            serverSptr->pimpl->connectedDevices.Erase(dev);
        }
        serverSptr->pimpl->callback_->OnConnectionStateUpdate(
            NearlinkRemoteDevice(device.addr_.GetAddress(), device.transport_),
            state, reason);
    }

    void OnAddService(const NearlinkSsapServiceParcel &service, int32_t ret) override
    {
        HILOGI("enter, ret: %{public}d", ret);
        auto serverSptr = GetServerSptr();
        NL_CHECK_RETURN(serverSptr && serverSptr->pimpl && serverSptr->pimpl->callback_,
            "serverSptr pimpl or callback_ is nullptr");

        std::shared_ptr<SsapService> ssapSvc = serverSptr->pimpl->BuildService(service);
        NL_CHECK_RETURN(ssapSvc, "ssapSvc is nullptr.");
        serverSptr->pimpl->ssapServices.EnsureInsert(ssapSvc->GetHandle(), ssapSvc);
        serverSptr->pimpl->callback_->OnServiceAdded(ssapSvc, ret);
    }

    void OnPropertyReadRequest(
        const NearlinkSsapDevice &device, const NearlinkSsapPropertyParcel &property, int ret) override
    {
        HILOGI("device: %{public}s, handle: 0x%{public}04X, ret: %{public}d, uuid: %{public}s",
            GET_ENCRYPT_SSAP_ADDR(device), property.handle_, ret, property.uuid_.GetEncryptUuid().c_str());
        auto serverSptr = GetServerSptr();
        NL_CHECK_RETURN(serverSptr && serverSptr->pimpl && serverSptr->pimpl->callback_,
            "serverSptr pimpl or callback_ is nullptr");
        SsapProperty proper(property.handle_, UUID::ConvertFrom128Bits(property.uuid_.ConvertTo128Bits()),
                property.opInd_, property.permission_);
        bool isFindService = serverSptr->pimpl->ssapServices.Find([&proper](const uint16_t handle,
            std::shared_ptr<SsapService> &service) -> bool {
                for (auto &sp : service->GetProperty()) {
                    if (sp.GetUuid().Equals(proper.GetUuid())) {
                        proper.SetServiceUuid(service->GetUuid());
                        return true;
                    }
                }
                return false;
        });
        if (!isFindService) {
            HILOGE("not find service!");
            return;
        }
        serverSptr->pimpl->callback_->OnPropertyReadRequest(
            NearlinkRemoteDevice(device.addr_.GetAddress(), device.transport_), proper, ret);
    }

    void OnPropertyWriteRequest(const NearlinkSsapDevice &device,
        const NearlinkSsapPropertyParcel &property, int ret) override
    {
        HILOGI("device: %{public}s, handle: 0x%{public}04X, ret: %{public}d, data len:%{public}lu, uuid: %{public}s",
            GET_ENCRYPT_SSAP_ADDR(device), property.handle_, ret, property.value_.size(),
            property.uuid_.GetEncryptUuid().c_str());
        auto serverSptr = GetServerSptr();
        NL_CHECK_RETURN(serverSptr && serverSptr->pimpl && serverSptr->pimpl->callback_,
            "serverSptr pimpl or callback_ is nullptr");
        SsapProperty reportProperty(property.handle_, UUID::ConvertFrom128Bits(property.uuid_.ConvertTo128Bits()),
            property.opInd_, property.permission_);
        reportProperty.SetValue(property.value_.data(), property.value_.size());
        bool isFindService = serverSptr->pimpl->ssapServices.Find([&reportProperty](const uint16_t handle,
            std::shared_ptr<SsapService> &service) -> bool {
                for (auto &sp : service->GetProperty()) {
                    if (sp.GetUuid().Equals(reportProperty.GetUuid())) {
                        reportProperty.SetServiceUuid(service->GetUuid());
                        return true;
                    }
                }
                return false;
        });
        if (!isFindService) {
            HILOGE("not find service!");
            return;
        }
        serverSptr->pimpl->callback_->OnPropertyWriteRequest(
            NearlinkRemoteDevice(device.addr_.GetAddress(), device.transport_), reportProperty, ret);
    }

    void OnDescriptorReadRequest(
        const NearlinkSsapDevice &device, const NearlinkSsapDescriptorParcel &descriptor, int ret) override
    {
        HILOGI("remote device: %{public}s, handle: 0x%{public}04X, ret: %{public}d",
            GET_ENCRYPT_SSAP_ADDR(device), descriptor.handle_, ret);
        auto serverSptr = GetServerSptr();
        NL_CHECK_RETURN(serverSptr && serverSptr->pimpl && serverSptr->pimpl->callback_,
            "serverSptr pimpl or callback_ is nullptr");
        serverSptr->pimpl->callback_->OnDescriptorReadRequest(
            NearlinkRemoteDevice(device.addr_.GetAddress(), device.transport_),
            SsapDescriptor(descriptor.handle_, descriptor.type_, descriptor.permission_), ret);
    }

    void OnDescriptorWriteRequest(
        const NearlinkSsapDevice &device, const NearlinkSsapDescriptorParcel &descriptor, int ret) override
    {
        HILOGI("remote device: %{public}s, handle: 0x%{public}04X, ret: %{public}d",
            GET_ENCRYPT_SSAP_ADDR(device), descriptor.handle_, ret);
        auto serverSptr = GetServerSptr();
        NL_CHECK_RETURN(serverSptr && serverSptr->pimpl && serverSptr->pimpl->callback_,
            "serverSptr pimpl or callback_ is nullptr");
        serverSptr->pimpl->callback_->OnDescriptorWriteRequest(
            NearlinkRemoteDevice(device.addr_.GetAddress(), device.transport_),
            SsapDescriptor(descriptor.handle_, descriptor.type_, descriptor.permission_), ret);
    }

    void OnMtuChanged(const NearlinkSsapDevice &device, uint16_t mtu) override
    {
        HILOGI("remote device: %{public}s, mtu: %{public}d", GET_ENCRYPT_SSAP_ADDR(device), mtu);
        auto serverSptr = GetServerSptr();
        NL_CHECK_RETURN(serverSptr && serverSptr->pimpl && serverSptr->pimpl->callback_,
            "serverSptr pimpl or callback_ is nullptr");
        serverSptr->pimpl->callback_->OnMtuUpdate(
            NearlinkRemoteDevice(device.addr_.GetAddress(), device.transport_), mtu);
        return;
    }

    void OnNotifyPropertyChanged(
        const NearlinkSsapDevice &device, const Uuid &uuid, uint16_t handle, int result) override
    {
        HILOGI("device: %{public}s, result: %{public}d, uuid: %{public}s",
            GET_ENCRYPT_SSAP_ADDR(device), result, uuid.GetEncryptUuid().c_str());
        auto serverSptr = GetServerSptr();
        NL_CHECK_RETURN(serverSptr && serverSptr->pimpl && serverSptr->pimpl->callback_,
            "serverSptr pimpl or callback_ is nullptr");
        serverSptr->pimpl->callback_->OnNotifyPropertyChanged(
            NearlinkRemoteDevice(device.addr_.GetAddress(), device.transport_),
            UUID::ConvertFrom128Bits(uuid.ConvertTo128Bits()), handle, result);

        return;
    }

    void OnNotifyEventChanged(
        const NearlinkSsapDevice &device, const Uuid &uuid, uint16_t handle, int result) override
    {
        HILOGI("device: %{public}s, result: %{public}d, uuid: %{public}s",
            GET_ENCRYPT_SSAP_ADDR(device), result, uuid.GetEncryptUuid().c_str());
        auto serverSptr = GetServerSptr();
        NL_CHECK_RETURN(serverSptr && serverSptr->pimpl && serverSptr->pimpl->callback_,
            "serverSptr pimpl or callback_ is nullptr");
        serverSptr->pimpl->callback_->OnNotifyEventChanged(
            NearlinkRemoteDevice(device.addr_.GetAddress(), device.transport_),
            UUID::ConvertFrom128Bits(uuid.ConvertTo128Bits()), handle, result);

        return;
    }
private:
    std::weak_ptr<SsapServer> server_;
};

std::shared_ptr<SsapService> SsapServer::impl::BuildService(const NearlinkSsapServiceParcel &service)
{
    std::shared_ptr<SsapService> ssapService = std::make_shared<SsapService>(
        UUID::ConvertFrom128Bits(service.uuid_.ConvertTo128Bits()),
        service.handle_,
        service.endHandle_,
        (service.isPrimary_ ? SsapServiceType::VENDOR_PROMARY : SsapServiceType::VENDOR_SECONDARY));

    for (auto &proper : service.properties_) {
        SsapProperty ssapProperty(proper.handle_,
            (proper.uuid_.GetUuidType() == Uuid::UUID16_BYTES_TYPE ? SsapProperty::PropertyType::ENTRY_TYPE_PROPERTY :
                SsapProperty::PropertyType::ENTRY_TYPE_VENDOR_PROPERTY),
            UUID::ConvertFrom128Bits(proper.uuid_.ConvertTo128Bits()),
            proper.opInd_,
            proper.permission_);

        ssapProperty.SetValue(proper.value_.data(), proper.value_.size());

        for (auto &desc : proper.descriptors_) {
            SsapDescriptor ssapDesc(desc.handle_, desc.type_, desc.permission_);
            ssapDesc.SetServiceUuid(ssapService->GetUuid());
            ssapDesc.SetValue(desc.value_.data(), desc.value_.size());
            ssapProperty.AddDescriptor(std::move(ssapDesc));
        }

        ssapService->AddProperty(std::move(ssapProperty));
    }

    return ssapService;
}

void SsapServer::impl::BuildIncludeService(SsapService &svc, const std::vector<Service> &iSvcs)
{
    for (auto &iSvc : iSvcs) {
        std::shared_ptr<SsapService> pSvc = GetIncludeService(iSvc.startHandle_);
        if (!pSvc) {
            HILOGE("Can not find include service entity in service ");
            continue;
        }
        svc.AddService(pSvc);
    }
}

std::shared_ptr<SsapService> SsapServer::impl::GetIncludeService(uint16_t handle)
{
    std::shared_ptr<SsapService> svc = nullptr;
    if (!ssapServices.GetValue(handle, svc)) {
        HILOGE("Can not find the service handle(0x%{public}04X)", handle);
        return nullptr;
    }
    return svc;
}

SsapServer::impl::impl(std::shared_ptr<SsapServerCallback> callback)
    : isRegisterSucceeded_(false), callback_(callback), applicationId_(0)
{
    HILOGD("enter");
}

SsapServer::impl::~impl()
{
    HILOGD("enter");
    NearlinkSaManager::GetInstance().DeregisterFunc(profileRegisterId_);

    sptr<INearlinkSsapServer> proxy = GetProxy<INearlinkSsapServer>(PROFILE_SSAP_SERVER);
    NL_CHECK_RETURN(proxy, "proxy is nullptr.");

    if (isRegisterSucceeded_) {
        proxy->DeregisterApplication(applicationId_);
    }
}

SsapServer::SsapServer(std::shared_ptr<SsapServerCallback> callback)
{
    HILOGI("create SsapServer start.");
    pimpl = std::make_shared<SsapServer::impl>(callback);
    if (!pimpl) {
        HILOGE("create SsapServer failed.");
    }
}

SsapServer::~SsapServer()
{}

void SsapServer::impl::Init(std::weak_ptr<SsapServer> server)
{
    if (profileRegisterId_ != 0) {
        HILOGI("profile has already registered.");
        return;
    }

    serviceCallback_ = new (std::nothrow) NearlinkSsapServerCallbackStubImpl(server);
    std::shared_ptr<NearlinkRegisterInfo> info = std::make_shared<NearlinkRegisterInfo>(PROFILE_SSAP_SERVER);
    std::weak_ptr<impl> wp = shared_from_this();
    info->serviceStartedFunc_ = [wp](sptr<IRemoteObject> remote) -> void {
        auto implSptr = wp.lock();
        NL_CHECK_RETURN(implSptr, "implSptr is nullptr.");
        sptr<INearlinkSsapServer> proxy = iface_cast<INearlinkSsapServer>(remote);
        NL_CHECK_RETURN(proxy, "proxy is nullptr.");
        int32_t appId = 0;
        NL_CHECK_RETURN(implSptr->serviceCallback_, "serviceCallback_ is nullptr.");
        NlErrCode status = proxy->RegisterApplication(implSptr->serviceCallback_, appId);
        if (status == NL_NO_ERROR && appId >= 0) {
            implSptr->applicationId_ = appId;
            implSptr->isRegisterSucceeded_ = true;
        } else {
            HILOGE("Can not Register to ssap server service! result = %{public}d", status);
        }
    };
    profileRegisterId_ = NearlinkSaManager::GetInstance().RegisterFunc(info);
    if (profileRegisterId_ == INVALID_PROFILE_ID) {
        HILOGE("profileRegisterId_ is invalid");
    }
}

std::shared_ptr<SsapServer> SsapServer::CreateSsapServer(std::shared_ptr<SsapServerCallback> callback)
{
    auto instance = std::make_shared<SsapServer>(Pattern(), callback);
    if (!instance->pimpl) {
        HILOGE("pimpl is nullptr.");
        return nullptr;
    }

    instance->pimpl->Init(instance);
    return instance;
}

std::shared_ptr<SsapDevice> SsapServer::impl::FindConnectedDevice(const NearlinkRemoteDevice &device)
{
    std::shared_ptr<SsapDevice> dev = nullptr;
    bool ret = connectedDevices.Find([&dev, &device](const std::shared_ptr<SsapDevice> ssapDevice) -> bool {
        if (device.GetDeviceAddr().compare(ssapDevice->addr_.GetAddress()) == 0 &&
            (device.GetTransportType() == ssapDevice->transport_)) {
            dev = ssapDevice;
            return true;
        }
        return false;
    });
    if (!ret) {
        HILOGE("device is not exist.");
    }
    return dev;
}

NlErrCode SsapServer::AddService(SsapService &service)
{
    HILOGD("enter");
    NL_CHECK_RETURN_RET(NearlinkHost::GetInstance().IsNearlinkSupport(), NL_ERR_API_NOT_SUPPORT,
        "nearlink is not support.");
    NL_CHECK_RETURN_RET(NearlinkHost::GetInstance().IsSleAvailableToCaller(), NL_ERR_SLE_OFF, "nearlink is off.");

    NearlinkSsapServiceParcel svc;
    svc.isPrimary_ = service.IsPrimary();
    svc.uuid_ = Uuid::ConvertFrom128Bits(service.GetUuid().ConvertTo128Bits());

    for (auto &isvc : service.GetIncludedServices()) {
        svc.includeServices_.push_back(Service(isvc->GetHandle()));
    }

    for (auto &proper : service.GetProperty()) {
        size_t length = 0;
        uint8_t *value = proper.GetValue(&length).get();
        if (value == nullptr || length == 0) {
            HILOGW("property handle=%{public}d value is empty, skip.", proper.GetHandle());
            continue;
        }
        std::vector<uint8_t> vecValue(value, value + length);
        Property p(proper.GetHandle(),
            Uuid::ConvertFrom128Bits(proper.GetUuid().ConvertTo128Bits()),
            vecValue,
            proper.GetOperationIndication(),
            proper.GetValuePermissions());

        for (auto &desc : proper.GetDescriptors()) {
            value = desc.GetValue(&length).get();
            if (value == nullptr || length == 0) {
                HILOGW("descriptor handle=%{public}d value is empty, skip.", desc.GetHandle());
                continue;
            }
            std::vector<uint8_t> temp(value, value + length);
            vecValue = std::move(temp);
            Descriptor d(desc.GetHandle(),
                desc.GetDescriptorType(),
                std::move(vecValue),
                desc.GetDescriptorPermission());

            p.descriptors_.push_back(std::move(d));
        }

        svc.properties_.push_back(std::move(p));
    }

    for (auto &method : service.GetMethod()) {
        size_t paramLen = 0;
        const std::unique_ptr<uint8_t[]> &param = method.GetParameter(&paramLen);
        std::vector<uint8_t> paramVec;
        if (param && paramLen > 0) {
            paramVec.assign(param.get(), param.get() + paramLen);
        }
        size_t resultLen = 0;
        const std::unique_ptr<uint8_t[]> &result = method.GetResult(&resultLen);
        std::vector<uint8_t> resultVec;
        if (result && resultLen > 0) {
            resultVec.assign(result.get(), result.get() + resultLen);
        }
        Method m(method.GetHandle(),
            Uuid::ConvertFrom128Bits(method.GetUuid().ConvertTo128Bits()),
            paramVec,
            resultVec,
            static_cast<uint32_t>(method.GetPermissions()));
        svc.methods_.push_back(std::move(m));
    }
    int appId = pimpl->applicationId_;

    sptr<INearlinkSsapServer> proxy = GetProxy<INearlinkSsapServer>(PROFILE_SSAP_SERVER);
    NL_CHECK_RETURN_RET(proxy, NL_ERR_UNAVAILABLE_PROXY, "proxy is nullptr.");
    return proxy->AddService(appId, &svc);
}

NlErrCode SsapServer::RemoveSsapService(const SsapService &service)
{
    HILOGD("enter");
    NL_CHECK_RETURN_RET(NearlinkHost::GetInstance().IsNearlinkSupport(), NL_ERR_API_NOT_SUPPORT,
        "nearlink is not support.");
    NL_CHECK_RETURN_RET(NearlinkHost::GetInstance().IsSleAvailableToCaller(), NL_ERR_SLE_OFF, "nearlink is off.");

    sptr<INearlinkSsapServer> proxy = GetProxy<INearlinkSsapServer>(PROFILE_SSAP_SERVER);
    NL_CHECK_RETURN_RET(proxy, NL_ERR_UNAVAILABLE_PROXY, "proxy is nullptr.");

    NlErrCode ret = NL_NO_ERROR;

    std::shared_ptr<SsapService> svc = nullptr;
    bool findRet = pimpl->ssapServices.Find([&svc, &service](int32_t key, std::shared_ptr<SsapService> val) -> bool {
        if (val->GetUuid().ToString() == service.GetUuid().ToString()) {
            svc = val;
            return true;
        }
        return false;
    });
    if (!findRet) {
        HILOGE("not find remove service");
        return NL_ERR_INVALID_PARAM;
    }
    ret = proxy->RemoveService(pimpl->applicationId_, (NearlinkSsapServiceParcel)Service(svc->GetHandle()));
        pimpl->ssapServices.Erase(svc->GetHandle());
    HILOGI("result = %{public}d.", ret);
    return ret;
}

NlErrCode SsapServer::ClearServices()
{
    HILOGD("enter");
    NL_CHECK_RETURN_RET(NearlinkHost::GetInstance().IsNearlinkSupport(), NL_ERR_API_NOT_SUPPORT,
        "nearlink is not support.");
    NL_CHECK_RETURN_RET(NearlinkHost::GetInstance().IsSleAvailableToCaller(), NL_ERR_SLE_OFF, "nearlink is off.");

    sptr<INearlinkSsapServer> proxy = GetProxy<INearlinkSsapServer>(PROFILE_SSAP_SERVER);
    NL_CHECK_RETURN_RET(proxy, NL_ERR_UNAVAILABLE_PROXY, "proxy is nullptr.");

    NlErrCode ret = proxy->ClearServices(pimpl->applicationId_);
    pimpl->ssapServices.Clear();
    return ret;
}

NlErrCode SsapServer::Close()
{
    HILOGD("enter");
    NL_CHECK_RETURN_RET(NearlinkHost::GetInstance().IsNearlinkSupport(), NL_ERR_API_NOT_SUPPORT,
        "nearlink is not support.");
    NL_CHECK_RETURN_RET(NearlinkHost::GetInstance().IsSleAvailableToCaller(), NL_ERR_SLE_OFF, "nearlink is off.");

    sptr<INearlinkSsapServer> proxy = GetProxy<INearlinkSsapServer>(PROFILE_SSAP_SERVER);
    NL_CHECK_RETURN_RET(proxy, NL_ERR_UNAVAILABLE_PROXY, "proxy is nullptr.");
    NlErrCode ret = proxy->DeregisterApplication(pimpl->applicationId_);
    HILOGI("ret: %{public}d", ret);
    if (ret == NL_NO_ERROR) {
        pimpl->isRegisterSucceeded_ = false;
    }
    return ret;
}

NlErrCode SsapServer::CancelConnection(const NearlinkRemoteDevice &device)
{
    HILOGI("remote device: %{public}s", GET_ENCRYPT_DEVICE_ADDR(device));
    NL_CHECK_RETURN_RET(NearlinkHost::GetInstance().IsNearlinkSupport(), NL_ERR_API_NOT_SUPPORT,
        "nearlink is not support.");
    NL_CHECK_RETURN_RET(NearlinkHost::GetInstance().IsSleAvailableToCaller(), NL_ERR_SLE_OFF, "nearlink is off.");
    NL_CHECK_RETURN_RET(device.IsValidNearlinkRemoteDevice(), NL_ERR_INTERNAL_ERROR, "Invalid remote device.");

    auto ssapDevice = pimpl->FindConnectedDevice(device);
    NL_CHECK_RETURN_RET(ssapDevice, NL_ERR_INTERNAL_ERROR, "ssapDevice is nullptr.");

    sptr<INearlinkSsapServer> proxy = GetProxy<INearlinkSsapServer>(PROFILE_SSAP_SERVER);
    NL_CHECK_RETURN_RET(proxy, NL_ERR_UNAVAILABLE_PROXY, "proxy is nullptr.");
    return proxy->CancelConnection(pimpl->applicationId_, static_cast<NearlinkSsapDevice>(*ssapDevice));
}

std::shared_ptr<SsapService> SsapServer::GetService(const UUID &uuid, bool isPrimary)
{
    HILOGD("enter");
    NL_CHECK_RETURN_RET(NearlinkHost::GetInstance().IsSleAvailableToCaller(), nullptr, "nearlink is off.");

    std::shared_ptr<SsapService> srv = nullptr;
    auto func = [&srv, &uuid, &isPrimary](const uint16_t handle, std::shared_ptr<SsapService> service) -> bool {
        if (service->GetUuid().Equals(uuid) && service->IsPrimary() == isPrimary) {
            HILOGI("Find service, handle: 0x%{public}04X", handle);
            srv = service;
            return true;
        }
        return false;
    };
    pimpl->ssapServices.Find(func);
    return srv;
}

bool SsapServer::FindPropertyByUuid(const SsapProperty &property, uint16_t &propertyHandle)
{
    return pimpl->ssapServices.Find([&propertyHandle, &property](const uint16_t handle,
        std::shared_ptr<SsapService> &service) -> bool {
        for (auto &sp : service->GetProperty()) {
            if (sp.GetUuid().Equals(property.GetUuid())) {
                propertyHandle = sp.GetHandle();
                return true;
            }
        }
        return false;
    });
}

NlErrCode SsapServer::NotifyPropertyChanged(
    const NearlinkRemoteDevice &device, const SsapProperty &property, bool confirm)
{
    NL_CHECK_RETURN_RET(NearlinkHost::GetInstance().IsNearlinkSupport(), NL_ERR_API_NOT_SUPPORT,
        "nearlink is not support.");
    uint16_t handle = 0;
    FindPropertyByUuid(property, handle);
    HILOGI("remote device: %{public}s, handle: 0x%{public}04X, confirm: %{public}d",
        GET_ENCRYPT_DEVICE_ADDR(device), handle, confirm);
    NL_CHECK_RETURN_RET(NearlinkHost::GetInstance().IsSleAvailableToCaller(), NL_ERR_SLE_OFF, "nearlink is off.");
    NL_CHECK_RETURN_RET(device.IsValidNearlinkRemoteDevice(), NL_ERR_INTERNAL_ERROR, "Invalid remote device.");
    NL_CHECK_RETURN_RET(pimpl->FindConnectedDevice(device), NL_ERR_INTERNAL_ERROR,
        "can not find device: %{public}s.", GET_ENCRYPT_DEVICE_ADDR(device));

    size_t length = 0;
    auto &propertyValue = property.GetValue(&length);
    NL_CHECK_RETURN_RET(propertyValue.get() != nullptr, NL_ERR_INTERNAL_ERROR, "propertyValue is nullptr.");
    std::vector<uint8_t> vecValue(propertyValue.get(), propertyValue.get() + length);

    NearlinkSsapPropertyParcel proper(Property(handle, vecValue));
    std::string address = device.GetDeviceAddr();

    sptr<INearlinkSsapServer> proxy = GetProxy<INearlinkSsapServer>(PROFILE_SSAP_SERVER);
    NL_CHECK_RETURN_RET(proxy, NL_ERR_UNAVAILABLE_PROXY, "proxy is nullptr.");
    return proxy->NotifyClient(pimpl->applicationId_, &proper,
        static_cast<NearlinkSsapDevice>(SsapDevice(RawAddress(address), device.GetTransportType())), confirm);
}

NlErrCode SsapServer::NotifyEvent(const NearlinkRemoteDevice &device, const SsapEvent &event,
    std::vector<uint8_t> &value, bool confirm)
{
    HILOGI("remote device: %{public}s, handle: 0x%{public}04X, confirm: %{public}d",
        GET_ENCRYPT_DEVICE_ADDR(device), event.GetHandle(), confirm);
    NL_CHECK_RETURN_RET(NearlinkHost::GetInstance().IsNearlinkSupport(), NL_ERR_API_NOT_SUPPORT,
        "nearlink is not support.");
    NL_CHECK_RETURN_RET(NearlinkHost::GetInstance().IsSleAvailableToCaller(), NL_ERR_SLE_OFF, "nearlink is off.");
    NL_CHECK_RETURN_RET(device.IsValidNearlinkRemoteDevice(), NL_ERR_INTERNAL_ERROR, "Invalid remote device.");
    NL_CHECK_RETURN_RET(pimpl->FindConnectedDevice(device), NL_ERR_INTERNAL_ERROR,
        "can not find device: %{public}s.", GET_ENCRYPT_DEVICE_ADDR(device));
    NearlinkSsapEventParcel eve(Event(event.GetHandle(),
        Uuid::ConvertFrom128Bits(event.GetUuid().ConvertTo128Bits())));
    std::string address = device.GetDeviceAddr();

    sptr<INearlinkSsapServer> proxy = GetProxy<INearlinkSsapServer>(PROFILE_SSAP_SERVER);
    NL_CHECK_RETURN_RET(proxy, NL_ERR_UNAVAILABLE_PROXY, "proxy is nullptr.");
    return proxy->NotifyEvent(pimpl->applicationId_, &eve, value,
        static_cast<NearlinkSsapDevice>(SsapDevice(RawAddress(address), device.GetTransportType())), confirm);
}

NlErrCode SsapServer::SetPropertyValue(SsapProperty &property)
{
    HILOGI("Enter");
    NL_CHECK_RETURN_RET(NearlinkHost::GetInstance().IsNearlinkSupport(), NL_ERR_API_NOT_SUPPORT,
        "nearlink is not support.");
    NL_CHECK_RETURN_RET(NearlinkHost::GetInstance().IsSleAvailableToCaller(), NL_ERR_SLE_OFF, "nearlink is off.");

    size_t length = 0;
    auto &propertyValue = property.GetValue(&length);
    NL_CHECK_RETURN_RET(propertyValue.get() != nullptr, NL_ERR_INTERNAL_ERROR, "propertyValue is nullptr.");
    std::vector<uint8_t> vecValue(propertyValue.get(), propertyValue.get() + length);
    NearlinkSsapPropertyParcel proper(Property(property.GetHandle(), vecValue));

    sptr<INearlinkSsapServer> proxy = GetProxy<INearlinkSsapServer>(PROFILE_SSAP_SERVER);
    NL_CHECK_RETURN_RET(proxy, NL_ERR_UNAVAILABLE_PROXY, "proxy is nullptr.");
    return proxy->SetPropertyValue(pimpl->applicationId_, &proper);
}

NlErrCode SsapServer::SetDescriptorValue(SsapDescriptor &descriptor)
{
    HILOGI("Enter");
    NL_CHECK_RETURN_RET(NearlinkHost::GetInstance().IsNearlinkSupport(), NL_ERR_API_NOT_SUPPORT,
        "nearlink is not support.");
    NL_CHECK_RETURN_RET(NearlinkHost::GetInstance().IsSleAvailableToCaller(), NL_ERR_SLE_OFF, "nearlink is off.");

    size_t length = 0;
    auto &descriptorValue = descriptor.GetValue(&length);
    NL_CHECK_RETURN_RET(descriptorValue.get() != nullptr, NL_ERR_INTERNAL_ERROR, "descriptorValue is nullptr.");
    std::vector<uint8_t> vecValue(descriptorValue.get(), descriptorValue.get() + length);
    NearlinkSsapDescriptorParcel descript(
        Descriptor(descriptor.GetHandle(), descriptor.GetDescriptorType(), std::move(vecValue)));

    sptr<INearlinkSsapServer> proxy = GetProxy<INearlinkSsapServer>(PROFILE_SSAP_SERVER);
    NL_CHECK_RETURN_RET(proxy, NL_ERR_UNAVAILABLE_PROXY, "proxy is nullptr.");
    return proxy->SetDescriptorValue(pimpl->applicationId_, &descript);
}

NlErrCode SsapServer::AuthorizeResponse(uint16_t requestId, bool allow)
{
    NL_CHECK_RETURN_RET(NearlinkHost::GetInstance().IsNearlinkSupport(), NL_ERR_API_NOT_SUPPORT,
        "nearlink is not support.");
    NL_CHECK_RETURN_RET(NearlinkHost::GetInstance().IsSleAvailableToCaller(), NL_ERR_SLE_OFF, "nearlink is off.");
    sptr<INearlinkSsapServer> proxy = GetProxy<INearlinkSsapServer>(PROFILE_SSAP_SERVER);
    NL_CHECK_RETURN_RET(proxy, NL_ERR_UNAVAILABLE_PROXY, "proxy is nullptr.");
    return proxy->AuthorizeResponse(pimpl->applicationId_, requestId, allow);
}

NlErrCode SsapServer::Connect(const NearlinkRemoteDevice &device, uint8_t secureReq, bool autoConnect)
{
    HILOGI("remote device: %{public}s, secureReq: %{public}d, autoConnect: %{public}d",
        GET_ENCRYPT_DEVICE_ADDR(device), secureReq, autoConnect);
    NL_CHECK_RETURN_RET(NearlinkHost::GetInstance().IsNearlinkSupport(), NL_ERR_API_NOT_SUPPORT,
        "nearlink is not support.");
    NL_CHECK_RETURN_RET(NearlinkHost::GetInstance().IsSleAvailableToCaller(), NL_ERR_SLE_OFF, "nearlink is off.");
    NL_CHECK_RETURN_RET(device.IsValidNearlinkRemoteDevice(), NL_ERR_INTERNAL_ERROR, "Invalid remote device.");

    std::string address = device.GetDeviceAddr();
    sptr<INearlinkSsapServer> proxy = GetProxy<INearlinkSsapServer>(PROFILE_SSAP_SERVER);
    NL_CHECK_RETURN_RET(proxy, NL_ERR_UNAVAILABLE_PROXY, "proxy is nullptr.");
    return proxy->Connect(pimpl->applicationId_,
        static_cast<NearlinkSsapDevice>(SsapDevice(RawAddress(address), device.GetTransportType())),
        secureReq, autoConnect);
}
}  // namespace Nearlink
}  // namespace OHOS