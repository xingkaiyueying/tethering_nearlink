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
#include "ssap_client_stack_adapter.h"

#include "ssap_log.h"
#include "ssap_utils.h"
#include "SleInterfaceAdapterSub.h"
#include "SleInterfaceManager.h"
#include "ThreadUtil.h"

namespace OHOS {
namespace Nearlink {
static SsapClientStackAdapter *g_ssapClientStackAdapter = nullptr;
struct SsapClientStackAdapter::impl {
    impl(SsapClientStackCallback &callback) : ssapClientStackCbk_(callback) {}

    SsapClientStackCallback &ssapClientStackCbk_;
};

SsapClientStackAdapter::SsapClientStackAdapter(SsapClientStackCallback &callback)
    : pimpl(std::make_unique<impl>(callback))
{
    g_ssapClientStackAdapter = this;
}

SsapClientStackAdapter::~SsapClientStackAdapter()
{
    g_ssapClientStackAdapter = nullptr;
}

void SsapClientStackAdapter::OnConnectionStateChangedTask(int appId, int state, int ret, int reason)
{
    pimpl->ssapClientStackCbk_.OnConnectionStateChanged(appId, state, ret, reason);
}

void SsapClientStackAdapter::OnConnectionStateChanged(int appId, uint8_t state, NLSTK_Errcode_E ret, int reason)
{
    SSAP_LOGI("appId=%{public}d, state=%{public}d, ret=%{public}d, reason=%{public}d", appId, state, ret, reason);
    DoInSsapThread([clientStackAdapter = g_ssapClientStackAdapter,
        appId, s = ConvertStateFromStackSsapState(state), r = ConvertFromPDUError(ret), reason]() -> void {
        NL_CHECK_RETURN(clientStackAdapter != nullptr, "clientStackAdapter is nullptr");
        clientStackAdapter->OnConnectionStateChangedTask(appId, s, r, reason);
    });
}

void SsapClientStackAdapter::OnMtuChangedTask(int appId, uint16_t mtu, int ret)
{
    pimpl->ssapClientStackCbk_.OnMtuChanged(appId, mtu, ret);
}

void SsapClientStackAdapter::OnMtuChanged(int appId, uint16_t mtu, NLSTK_Errcode_E ret)
{
    SSAP_LOGI("appId=%{public}d", appId);
    DoInSsapThread([clientStackAdapter = g_ssapClientStackAdapter,
        appId, mtu, r = ConvertFromPDUError(ret)]() -> void {
        NL_CHECK_RETURN(clientStackAdapter != nullptr, "clientStackAdapter is nullptr");
        clientStackAdapter->OnMtuChangedTask(appId, mtu, r);
    });
}

void SsapClientStackAdapter::OnDiscoverCompleteTask(int appId, int ret)
{
    pimpl->ssapClientStackCbk_.OnDiscoverComplete(appId, ret);
}

void SsapClientStackAdapter::OnDiscoverComplete(int appId, NLSTK_Errcode_E ret)
{
    SSAP_LOGD("appId=%{public}d", appId);
    DoInSsapThread([clientStackAdapter = g_ssapClientStackAdapter,
        appId, r = ConvertFromPDUError(ret)]() -> void {
        NL_CHECK_RETURN(clientStackAdapter != nullptr, "clientStackAdapter is nullptr");
        clientStackAdapter->OnDiscoverCompleteTask(appId, r);
    });
}

void SsapClientStackAdapter::OnDiscoverByUuidCompleteTask(int appId, const Uuid &uuid, int ret)
{
    pimpl->ssapClientStackCbk_.OnDiscoverByUuidComplete(appId, uuid, ret);
}

void SsapClientStackAdapter::OnDiscoverByUuidComplete(int appId, NLSTK_SsapUuid_S *stackUuid, NLSTK_Errcode_E ret)
{
    SSAP_LOGI("appId=%{public}d", appId);
    NL_CHECK_RETURN(stackUuid != nullptr, "stackUuid is nullptr");
    Uuid uuid = Uuid::ConvertFromBytesSle(stackUuid->uuid);

    DoInSsapThread([clientStackAdapter = g_ssapClientStackAdapter, appId, uuid,
        r = ConvertFromPDUError(ret)]() -> void {
        NL_CHECK_RETURN(clientStackAdapter != nullptr, "clientStackAdapter is nullptr");
        clientStackAdapter->OnDiscoverByUuidCompleteTask(appId, uuid, r);
    });
}

void SsapClientStackAdapter::OnReadPropertyTask(int appId, Property &property, int ret)
{
    pimpl->ssapClientStackCbk_.OnReadProperty(appId, property, ret);
}

void SsapClientStackAdapter::OnReadProperty(int appId, NLSTK_SsapClientReadPropertyInfo_S *property,
    NLSTK_Errcode_E ret)
{
    SSAP_LOGI("appId=%{public}d", appId);
    NL_CHECK_RETURN(property != nullptr, "property is nullptr");
    NL_CHECK_RETURN(property->value.data != nullptr, "property->value.data is nullptr");

    std::vector<uint8_t> value(property->value.data, property->value.data + property->value.len);
    Uuid uuid = Uuid::ConvertFromBytesSle(property->uuid.uuid);

    DoInSsapThread([clientStackAdapter = g_ssapClientStackAdapter, appId,
        ssapProperty = Property(property->handle, uuid, std::move(value)), r = property->errorCode]() mutable {
        NL_CHECK_RETURN(clientStackAdapter != nullptr, "clientStackAdapter is nullptr");
        clientStackAdapter->OnReadPropertyTask(appId, ssapProperty, r);
    });
}

void SsapClientStackAdapter::OnCallMethodTask(int appId, Method &method, int ret)
{
    pimpl->ssapClientStackCbk_.OnCallMethod(appId, method, ret);
}

void SsapClientStackAdapter::OnCallMethod(int appId, NLSTK_SsapClientCallMethodResult_S *method,
    NLSTK_Errcode_E ret)
{
    SSAP_LOGI("appId=%{public}d", appId);
    NL_CHECK_RETURN(method != nullptr, "method is nullptr");
    NL_CHECK_RETURN(method->value.data != nullptr, "method->value.data is nullptr");

    std::vector<uint8_t> value(method->value.data, method->value.data + method->value.len);
    Uuid uuid = Uuid::ConvertFromBytesSle(method->uuid.uuid);

    DoInSsapThread([clientStackAdapter = g_ssapClientStackAdapter, appId,
        ssapMethod = Method(method->handle, uuid, std::move(value)), r = method->errorCode]() mutable {
        NL_CHECK_RETURN(clientStackAdapter != nullptr, "clientStackAdapter is nullptr");
        clientStackAdapter->OnCallMethodTask(appId, ssapMethod, r);
    });
}

void SsapClientStackAdapter::OnReadDescriptorTask(int appId, Descriptor &descriptor, int ret)
{
    pimpl->ssapClientStackCbk_.OnReadDescriptor(appId, descriptor, ret);
}

void SsapClientStackAdapter::OnReadDescriptor(int appId, NLSTK_SsapClientReadDescriptorInfo_S *descriptor,
    NLSTK_Errcode_E ret)
{
    SSAP_LOGI("appId=%{public}d", appId);
    NL_CHECK_RETURN(descriptor != nullptr, "descriptor is nullptr");
    NL_CHECK_RETURN(descriptor->value.data != nullptr, "descriptor->value.data is nullptr");
    std::vector<uint8_t> value(descriptor->value.data, descriptor->value.data + descriptor->value.len);

    DoInSsapThread([clientStackAdapter = g_ssapClientStackAdapter, appId,
        ssapDescriptor = Descriptor(descriptor->handle, descriptor->type, std::move(value)),
        r = descriptor->errorCode]() mutable {
        NL_CHECK_RETURN(clientStackAdapter != nullptr, "clientStackAdapter is nullptr");
        clientStackAdapter->OnReadDescriptorTask(appId, ssapDescriptor, r);
    });
}

void SsapClientStackAdapter::OnReadPropertiesByUuidTask(int appId, std::list<Property> &properties, int ret)
{
    pimpl->ssapClientStackCbk_.OnReadPropertiesByUuid(appId, properties, ret);
}

Property SsapClientStackAdapter::BuildPropertyByStackProperty(NLSTK_SsapClientReadPropertyInfo_S *stackProperty)
{
    NL_CHECK_RETURN_RET(stackProperty != nullptr, Property(), "stackProperty is nullptr");
    Uuid uuid = Uuid::ConvertFromBytesSle(stackProperty->uuid.uuid);
    NL_CHECK_RETURN_RET(stackProperty->value.data != nullptr, Property(stackProperty->handle, uuid),
        "stackProperty->value.data is nullptr");
    std::vector<uint8_t> value(stackProperty->value.data, stackProperty->value.data + stackProperty->value.len);
    return Property(stackProperty->handle, uuid, std::move(value));
}

void SsapClientStackAdapter::OnReadPropertiesByUuid(int appId, NLSTK_SsapClientReadPropertyInfo_S *property,
    uint8_t propertyNum, NLSTK_Errcode_E ret)
{
    SSAP_LOGI("appId=%{public}d", appId);
    NL_CHECK_RETURN(property != nullptr, "property is nullptr");
    std::list<Property> ssapProperties;
    for (uint8_t i = 0; i < propertyNum; i++) {
        ssapProperties.emplace_back(g_ssapClientStackAdapter->BuildPropertyByStackProperty(&property[i]));
    }

    DoInSsapThread([clientStackAdapter = g_ssapClientStackAdapter,
        appId, properties = ssapProperties, r = ConvertFromPDUError(ret)]() mutable {
        NL_CHECK_RETURN(clientStackAdapter != nullptr, "clientStackAdapter is nullptr");
        clientStackAdapter->OnReadPropertiesByUuidTask(appId, properties, r);
    });
}

void SsapClientStackAdapter::OnWritePropertyTask(int appId, Property &property, int ret)
{
    pimpl->ssapClientStackCbk_.OnWriteProperty(appId, property, ret);
}

void SsapClientStackAdapter::OnWriteProperty(int appId, NLSTK_SsapClientReadPropertyInfo_S *property,
    NLSTK_Errcode_E ret)
{
    SSAP_LOGD("appId=%{public}d", appId);
    NL_CHECK_RETURN(property != nullptr, "property is nullptr");
    NL_CHECK_RETURN(property->value.data != nullptr, "property->value.data is nullptr");

    std::vector<uint8_t> value(property->value.data, property->value.data + property->value.len);
    Uuid uuid = Uuid::ConvertFromBytesSle(property->uuid.uuid);

    DoInSsapThread([clientStackAdapter = g_ssapClientStackAdapter, appId,
        ssapProperty = Property(property->handle, uuid, std::move(value)), r = property->errorCode]() mutable {
        NL_CHECK_RETURN(clientStackAdapter != nullptr, "clientStackAdapter is nullptr");
        clientStackAdapter->OnWritePropertyTask(appId, ssapProperty, r);
    });
}

void SsapClientStackAdapter::OnWriteDescriptorTask(int appId, Descriptor &descriptor, int ret)
{
    pimpl->ssapClientStackCbk_.OnWriteDescriptor(appId, descriptor, ret);
}

void SsapClientStackAdapter::OnWriteDescriptor(int32_t appId, NLSTK_SsapClientReadDescriptorInfo_S *descriptor,
    NLSTK_Errcode_E ret)
{
    SSAP_LOGI("appId=%{public}d", appId);
    NL_CHECK_RETURN(descriptor != nullptr, "descriptor is nullptr");
    NL_CHECK_RETURN(descriptor->value.data != nullptr, "descriptor->value.data is nullptr");

    DoInSsapThread([clientStackAdapter = g_ssapClientStackAdapter, appId,
        ssapDescriptor = Descriptor(descriptor->handle, descriptor->type), r = descriptor->errorCode]() mutable {
        NL_CHECK_RETURN(clientStackAdapter != nullptr, "clientStackAdapter is nullptr");
        clientStackAdapter->OnWriteDescriptorTask(appId, ssapDescriptor, r);
    });
}

void SsapClientStackAdapter::OnGetPropertyNotificationTask(int appId, const Property &property, bool enable, int ret)
{
    pimpl->ssapClientStackCbk_.OnGetPropertyNotification(appId, property, enable, ret);
}

void SsapClientStackAdapter::OnGetPropertyNotification(int appId, NLSTK_SsapUuid_S *stackUuid,
    uint16_t handle, bool enable, NLSTK_Errcode_E ret)
{
    SSAP_LOGI("appId=%{public}d", appId);
    NL_CHECK_RETURN(stackUuid != nullptr, "stackUuid is nullptr");
    Uuid uuid = Uuid::ConvertFromBytesSle(stackUuid->uuid);
    DoInSsapThread([clientStackAdapter = g_ssapClientStackAdapter,
        appId, ssapProperty = Property(handle, uuid), enable, r = ConvertFromPDUError(ret)]() mutable {
        NL_CHECK_RETURN(clientStackAdapter != nullptr, "clientStackAdapter is nullptr");
        clientStackAdapter->OnGetPropertyNotificationTask(appId, ssapProperty, enable, r);
    });
}

void SsapClientStackAdapter::OnGetPropertyIndicationTask(int appId, const Property &property, bool enable, int ret)
{
    pimpl->ssapClientStackCbk_.OnGetPropertyIndication(appId, property, enable, ret);
}

void SsapClientStackAdapter::OnGetPropertyIndication(int appId, NLSTK_SsapUuid_S *stackUuid,
    uint16_t handle, bool enable, NLSTK_Errcode_E ret)
{
    SSAP_LOGI("appId=%{public}d", appId);
    NL_CHECK_RETURN(stackUuid != nullptr, "stackUuid is nullptr");
    Uuid uuid = Uuid::ConvertFromBytesSle(stackUuid->uuid);
    DoInSsapThread([clientStackAdapter = g_ssapClientStackAdapter,
        appId, ssapProperty = Property(handle, uuid), enable, r = ConvertFromPDUError(ret)]() mutable {
        NL_CHECK_RETURN(clientStackAdapter != nullptr, "clientStackAdapter is nullptr");
        clientStackAdapter->OnGetPropertyIndicationTask(appId, ssapProperty, enable, r);
    });
}

void SsapClientStackAdapter::OnSetPropertyNotificationTask(int appId, const Property &property, bool enable, int ret)
{
    pimpl->ssapClientStackCbk_.OnSetPropertyNotification(appId, property, enable, ret);
}

void SsapClientStackAdapter::OnSetPropertyNotification(int appId, NLSTK_SsapUuid_S *stackUuid,
    uint16_t handle, bool enable, NLSTK_Errcode_E ret)
{
    SSAP_LOGI("appId=%{public}d", appId);
    NL_CHECK_RETURN(stackUuid != nullptr, "stackUuid is nullptr");
    Uuid uuid = Uuid::ConvertFromBytesSle(stackUuid->uuid);
    DoInSsapThread([clientStackAdapter = g_ssapClientStackAdapter,
        appId, ssapProperty = Property(handle, uuid), enable, r = ConvertFromPDUError(ret)]() mutable {
        NL_CHECK_RETURN(clientStackAdapter != nullptr, "clientStackAdapter is nullptr");
        clientStackAdapter->OnSetPropertyNotificationTask(appId, ssapProperty, enable, r);
    });
}

void SsapClientStackAdapter::OnSetPropertyIndicationTask(int appId, const Property &property, bool enable, int ret)
{
    pimpl->ssapClientStackCbk_.OnSetPropertyIndication(appId, property, enable, ret);
}

void SsapClientStackAdapter::OnSetPropertyIndication(int appId, NLSTK_SsapUuid_S *stackUuid,
    uint16_t handle, bool enable, NLSTK_Errcode_E ret)
{
    SSAP_LOGI("appId=%{public}d", appId);
    NL_CHECK_RETURN(stackUuid != nullptr, "stackUuid is nullptr");
    Uuid uuid = Uuid::ConvertFromBytesSle(stackUuid->uuid);
    DoInSsapThread([clientStackAdapter = g_ssapClientStackAdapter,
        appId, ssapProperty = Property(handle, uuid), enable, r = ConvertFromPDUError(ret)]() mutable {
        NL_CHECK_RETURN(clientStackAdapter != nullptr, "clientStackAdapter is nullptr");
        clientStackAdapter->OnSetPropertyIndicationTask(appId, ssapProperty, enable, r);
    });
}

void SsapClientStackAdapter::OnPropertyChangedTask(int appId, const Property &property)
{
    pimpl->ssapClientStackCbk_.OnPropertyChanged(appId, property);
}

void SsapClientStackAdapter::OnPropertyChanged(int appId, NLSTK_SsapClientReadPropertyInfo_S *property)
{
    SSAP_LOGD("appId=%{public}d", appId);
    NL_CHECK_RETURN(property != nullptr, "property is nullptr");
    NL_CHECK_RETURN(property->value.data != nullptr, "property->value.data is nullptr");

    std::vector<uint8_t> value(property->value.data, property->value.data + property->value.len);
    Uuid uuid = Uuid::ConvertFromBytesSle(property->uuid.uuid);

    DoInSsapThread([clientStackAdapter = g_ssapClientStackAdapter, appId,
        ssapProperty = Property(property->handle, uuid, std::move(value))]() mutable {
        NL_CHECK_RETURN(clientStackAdapter != nullptr, "clientStackAdapter is nullptr");
        clientStackAdapter->OnPropertyChangedTask(appId, ssapProperty);
    });
}

void SsapClientStackAdapter::OnEventTask(int appId, const Event &event)
{
    pimpl->ssapClientStackCbk_.OnEvent(appId, event);
}

void SsapClientStackAdapter::OnEvent(int appId, NLSTK_SsapClientEventInfo_S *event)
{
    SSAP_LOGI("appId=%{public}d", appId);
    NL_CHECK_RETURN(event != nullptr, "event is nullptr");
    NL_CHECK_RETURN(event->value.data != nullptr, "event->value.data is nullptr");

    std::vector<uint8_t> value(event->value.data, event->value.data + event->value.len);
    Uuid uuid = Uuid::ConvertFromBytesSle(event->uuid.uuid);

    DoInSsapThread([clientStackAdapter = g_ssapClientStackAdapter, appId,
        ssapEvent = Event(event->handle, uuid, std::move(value))]() mutable {
        NL_CHECK_RETURN(clientStackAdapter != nullptr, "clientStackAdapter is nullptr");
        clientStackAdapter->OnEventTask(appId, ssapEvent);
    });
}

void SsapClientStackAdapter::OnDisableTask(void)
{
    pimpl->ssapClientStackCbk_.OnDisable();
}

void SsapClientStackAdapter::OnDisable(void)
{
    SSAP_LOGI("enter");
    DoInSsapThread([clientStackAdapter = g_ssapClientStackAdapter]() -> void {
        NL_CHECK_RETURN(clientStackAdapter != nullptr, "clientStackAdapter is nullptr");
        clientStackAdapter->OnDisableTask();
    });
}

void SsapClientStackAdapter::OnServiceRediscoverTask(int appId, std::vector<Service> &services)
{
    pimpl->ssapClientStackCbk_.OnServicesRediscovered(appId, services);
}

void SsapClientStackAdapter::OnServiceRediscover(int32_t appId, NLSTK_SsapServ_S *service, uint16_t serviceNum)
{
    SSAP_LOGI("appId=%{public}d, serviceNum=%{public}d", appId, serviceNum);
    NL_CHECK_RETURN(service != nullptr, "service is nullptr");
    NL_CHECK_RETURN(g_ssapClientStackAdapter != nullptr, "adapter is nullptr");
    std::vector<Service> ssapServices;
    for (uint16_t i = 0; i < serviceNum; i++) {
        ssapServices.emplace_back(g_ssapClientStackAdapter->BuildServiceStructByStackService(&service[i]));
    }

    DoInSsapThread([clientStackAdapter = g_ssapClientStackAdapter,
        appId, services = ssapServices]() mutable {
        NL_CHECK_RETURN(clientStackAdapter != nullptr, "clientStackAdapter is nullptr");
        clientStackAdapter->OnServiceRediscoverTask(appId, services);
    });
}

void SsapClientStackAdapter::OnServiceChangeTask(int appId, uint16_t handle, const Uuid &uuid)
{
    pimpl->ssapClientStackCbk_.OnServiceChanged(appId, handle, uuid);
}

void SsapClientStackAdapter::OnServiceChange(int32_t appId, uint16_t handle, NLSTK_SsapUuid_S *stackUuid)
{
    SSAP_LOGI("appId=%{public}d, handle=%{public}d", appId, handle);
    NL_CHECK_RETURN(stackUuid != nullptr, "stackUuid is nullptr");
    NL_CHECK_RETURN(g_ssapClientStackAdapter != nullptr, "adapter is nullptr");
    Uuid uuid = Uuid::ConvertFromBytesSle(stackUuid->uuid);

    DoInSsapThread([clientStackAdapter = g_ssapClientStackAdapter,
        appId, handle, uuid]() {
        NL_CHECK_RETURN(clientStackAdapter != nullptr, "clientStackAdapter is nullptr");
        clientStackAdapter->OnServiceChangeTask(appId, handle, uuid);
    });
}

void SsapClientStackAdapter::Disable()
{
    NLSTK_Errcode_E ret = NLSTK_SsapCleanClientApp(OnDisable);
    SSAP_LOGI("enter");
    if (ret != NLSTK_ERRCODE_SUCCESS) {
        SSAP_LOGE("fail ! ret=%{public}d", ret);
    }
}

int SsapClientStackAdapter::RegisterApplication(int &appId, const RawAddress &addr, SsapSecureType secReq)
{
    NLSTK_SsapAppClientCb_S cbs = {};
    cbs.onMtuChanged = &OnMtuChanged;
    cbs.onFindService = &OnDiscoverComplete;
    cbs.onFindServiceByUuid = &OnDiscoverByUuidComplete;
    cbs.onConnectionStateChanged = &OnConnectionStateChanged;
    cbs.onReadProperty = &OnReadProperty;
    cbs.onCallMethod = &OnCallMethod;
    cbs.onReadDescriptor = &OnReadDescriptor;
    cbs.onReadPropertyByUuid = &OnReadPropertiesByUuid;
    cbs.onGetPropertyNtf = &OnGetPropertyNotification;
    cbs.onGetPropertyInd = &OnGetPropertyIndication;
    cbs.onSetPropertyNtf = &OnSetPropertyNotification;
    cbs.onSetPropertyInd = &OnSetPropertyIndication;
    cbs.onPropertyChanged = &OnPropertyChanged;
    cbs.onEvent = &OnEvent;
    cbs.onWriteProperty = &OnWriteProperty;
    cbs.onWriteDescriptor = &OnWriteDescriptor;
    cbs.onServiceChange = &OnServiceChange;
    cbs.onServiceRediscover = &OnServiceRediscover;

    SLE_Addr_S stackAddr = ConvertToSleAddr(addr);
    /* 协议栈只保存一份cbk */
    NLSTK_Errcode_E ret = NLSTK_SsapClientRegApp(&appId, &cbs, &stackAddr);
    SSAP_LOGD("appId=%{public}d", appId);
    if (ret != NLSTK_ERRCODE_SUCCESS) {
        SSAP_LOGE("fail ! ret=%{public}d", ret);
        return ConvertFromPDUError(ret);
    }
    return SsapStatus::SSAP_SUCCESS;
}

void SsapClientStackAdapter::DeregisterApplication(int appId)
{
    SSAP_LOGD("appId=%{public}d", appId);
    NLSTK_SsapClientDeregApp(appId);
}

void SsapClientStackAdapter::ExchangeMtu(int appId, uint16_t mtu)
{
    SSAP_LOGI("appId=%{public}d, mtu=%{public}d", appId, mtu);
    NLSTK_Errcode_E ret = NLSTK_SsapClientExchangeMtu(appId, mtu);
    if (ret != NLSTK_ERRCODE_SUCCESS) {
        SSAP_LOGE("fail ! ret=%{public}d", ret);
        OnMtuChangedTask(appId, mtu, ConvertFromPDUError(ret));
    }
}

void SsapClientStackAdapter::DiscoverServices(int appId)
{
    SSAP_LOGI("appId=%{public}d", appId);
    NLSTK_Errcode_E ret = NLSTK_SsapClientDiscoverServices(appId, MIN_ENTRY_HANDLE, MAX_ENTRY_HANDLE,
        FIND_STRUCTURE_TYPE_SERVICE_STRUCTURE);
    if (ret != NLSTK_ERRCODE_SUCCESS) {
        SSAP_LOGE("fail ! ret=%{public}d", ret);
        OnDiscoverCompleteTask(appId, ConvertFromPDUError(ret));
    }
}

void SsapClientStackAdapter::DiscoverServicesByUuid(int appId, const Uuid &uuid, uint16_t shdl, uint16_t ehdl)
{
    SSAP_LOGI("appId=%{public}d, uuid=%{public}s", appId, GET_ENCRYPT_UUID(uuid));
    NLSTK_SsapUuid_S stackUuid = ConvertToSleUuid(uuid);
    NLSTK_Errcode_E ret = NLSTK_SsapClientDiscoverServicesByUuid(appId, &stackUuid, shdl, ehdl,
        FIND_STRUCTURE_TYPE_SERVICE_STRUCTURE);
    if (ret != NLSTK_ERRCODE_SUCCESS) {
        SSAP_LOGE("fail ! ret=%{public}d", ret);
        OnDiscoverByUuidCompleteTask(appId, uuid, ConvertFromPDUError(ret));
    }
}

Descriptor SsapClientStackAdapter::BuildDescriptorStructByStackDescriptor(NLSTK_SsapDtor_S *stackDescriptor)
{
    NL_CHECK_RETURN_RET(stackDescriptor != nullptr, Descriptor(), "stackDescriptor is nullptr");
    return Descriptor(stackDescriptor->handle, stackDescriptor->type);
}

Property SsapClientStackAdapter::BuildPropertyStructByStackProperty(NLSTK_SsapPrty_S *stackProperty)
{
    NL_CHECK_RETURN_RET(stackProperty != nullptr, Property(), "stackProperty is nullptr");
    Property property(stackProperty->handle, Uuid::ConvertFromBytesSle(stackProperty->uuid.uuid),
        stackProperty->operation.operationValue);
    SSAP_LOGD("property.uuid=%{public}s", property.uuid_.GetEncryptUuid().c_str());
    for (uint16_t i = 0; i < stackProperty->descriptorNum; i++) {
        property.descriptors_.emplace_back(BuildDescriptorStructByStackDescriptor(&stackProperty->descriptors[i]));
    }
    return property;
}

Method SsapClientStackAdapter::BuildMethodStructByStackMethod(NLSTK_SsapPrty_S *stackMethod)
{
    NL_CHECK_RETURN_RET(stackMethod != nullptr, Method(), "stackMethod is nullptr");
    Method method(stackMethod->handle, Uuid::ConvertFromBytesSle(stackMethod->uuid.uuid));
    method.permission_ = stackMethod->permission.permissionValue;
    SSAP_LOGI("method.uuid=%{public}s", method.uuid_.GetEncryptUuid().c_str());
    return method;
}

Event SsapClientStackAdapter::BuildEventStructByStackEvent(NLSTK_SsapPrty_S *stackEvent)
{
    NL_CHECK_RETURN_RET(stackEvent != nullptr, Event(), "stackEvent is nullptr");
    Event event(stackEvent->handle, Uuid::ConvertFromBytesSle(stackEvent->uuid.uuid));
    SSAP_LOGD("event.uuid=%{public}s", event.uuid_.GetEncryptUuid().c_str());
    return event;
}

Service SsapClientStackAdapter::BuildServiceStructByStackService(NLSTK_SsapServ_S *stackService)
{
    NL_CHECK_RETURN_RET(stackService != nullptr, Service(), "stackService is nullptr");

    bool isPrimary = (stackService->serviceType == ITEM_TYPE_STD_PRIMARY_SERVICE ||
        stackService->serviceType == ITEM_TYPE_VENDOR_PRIMARY_SERVICE) ? true : false;
    Service service(stackService->handle, stackService->handle, stackService->endHandle, isPrimary,
        Uuid::ConvertFromBytesSle(stackService->uuid.uuid));
    SSAP_LOGD("service.uuid=%{public}s", service.uuid_.GetEncryptUuid().c_str());
    for (uint16_t i = 0; i < stackService->propertyNum; i++) {
        service.properties_.emplace_back(BuildPropertyStructByStackProperty(&stackService->properties[i]));
    }
    SSAP_LOGD("methodNum=%{public}d", stackService->methodNum);
    for (size_t i = 0; i < stackService->methodNum; i++) {
        service.methods_.emplace_back(BuildMethodStructByStackMethod(&stackService->methods[i]));
    }
    SSAP_LOGD("eventNum=%{public}d", stackService->eventNum);
    for (size_t i = 0; i < stackService->eventNum; i++) {
        service.events_.emplace_back(BuildEventStructByStackEvent(&stackService->events[i]));
    }
    return service;
}

std::list<Service> SsapClientStackAdapter::GetServices(int appId)
{
    SSAP_LOGD("appId=%{public}d", appId);
    NLSTK_SsapServ_S *stackService = nullptr;
    uint16_t serviceNum = 0;
    NLSTK_SsapClientFreeFunc freeFunc = nullptr;

    NLSTK_Errcode_E ret = NLSTK_SsapClientGetServices(appId, &stackService, &serviceNum, &freeFunc);
    if (ret != NLSTK_ERRCODE_SUCCESS || stackService == nullptr) {
        SSAP_LOGE("fail ! ret=%{public}d", ret);
        if (freeFunc != nullptr) {
            freeFunc(stackService, serviceNum);
            stackService = nullptr;
        }
        return std::list<Service>();
    }

    std::list<Service> services;
    for (uint16_t i = 0; i < serviceNum; i++) {
        services.emplace_back(BuildServiceStructByStackService(&stackService[i]));
    }

    if (freeFunc != nullptr) {
        freeFunc(stackService, serviceNum);
        stackService = nullptr;
    }
    return services;
}

std::list<Service> SsapClientStackAdapter::GetServicesByUuid(int appId, const Uuid &uuid)
{
    SSAP_LOGI("appId=%{public}d, uuid=%{public}s", appId, GET_ENCRYPT_UUID(uuid));
    NLSTK_SsapUuid_S stackUuid = ConvertToSleUuid(uuid);
    NLSTK_SsapServ_S *stackService = nullptr;
    uint16_t serviceNum = 0;
    NLSTK_SsapClientFreeFunc freeFunc = nullptr;

    NLSTK_Errcode_E ret = NLSTK_SsapClientGetServicesByUuid(appId, &stackUuid, &stackService, &serviceNum, &freeFunc);
    if (ret != NLSTK_ERRCODE_SUCCESS || stackService == nullptr) {
        SSAP_LOGE("fail ! ret=%{public}d", ret);
        if (freeFunc != nullptr) {
            freeFunc(stackService, serviceNum);
            stackService = nullptr;
        }
        return std::list<Service>();
    }

    std::list<Service> services;
    for (uint16_t i = 0; i < serviceNum; i++) {
        services.emplace_back(BuildServiceStructByStackService(&stackService[i]));
    }

    if (freeFunc != nullptr) {
        freeFunc(stackService, serviceNum);
        stackService = nullptr;
    }
    return services;
}

void SsapClientStackAdapter::ReadProperty(int appId, const Property &property)
{
    SSAP_LOGI("appId=%{public}d", appId);
    NLSTK_Errcode_E ret = NLSTK_SsapClientReadProperty(appId, property.handle_);
    if (ret != NLSTK_ERRCODE_SUCCESS) {
        SSAP_LOGE("fail ! ret=%{public}d", ret);
        Property p = property;
        OnReadPropertyTask(appId, p, ConvertFromPDUError(ret));
    }
}

void SsapClientStackAdapter::CallMethod(int appId, Method &method, bool withoutRsp)
{
    SSAP_LOGI("appId=%{public}d", appId);
    Method m = method;
    NLSTK_VariableData_S value = {};
    value.len = method.parameter_.size();
    if (value.len == 0) {
        OnCallMethodTask(appId, m, SsapStatus::SSAP_INVALID_PARAM);
        return;
    }
    value.data = new (std::nothrow) uint8_t[value.len];
    if (value.data == nullptr) {
        SSAP_LOGE("value.data is nullptr");
        OnCallMethodTask(appId, m, SsapStatus::SSAP_FAILURE);
        return;
    }
    (void)memset_s(value.data, value.len, 0x00, value.len);
    std::copy(method.parameter_.begin(), method.parameter_.end(), value.data);

    NLSTK_Errcode_E ret = NLSTK_SsapClientCallMethod(appId, method.handle_, &value, withoutRsp);
    if (ret != NLSTK_ERRCODE_SUCCESS) {
        SSAP_LOGE("fail ! ret=%{public}d", ret);
        OnCallMethodTask(appId, m, ConvertFromPDUError(ret));
        delete[] value.data;
        value.data = nullptr;
        return;
    }
    delete[] value.data;
    value.data = nullptr;
}

void SsapClientStackAdapter::ReadDescriptor(int appId, const Descriptor &descriptor)
{
    SSAP_LOGI("appId=%{public}d", appId);
    NLSTK_Errcode_E ret = NLSTK_SsapClientReadDescriptor(appId, descriptor.handle_, descriptor.type_);
    if (ret != NLSTK_ERRCODE_SUCCESS) {
        SSAP_LOGE("fail ! ret=%{public}d", ret);
        Descriptor d = descriptor;
        OnReadDescriptorTask(appId, d, ConvertFromPDUError(ret));
    }
}

void SsapClientStackAdapter::ReadPropertiesByUuid(int appId, const Uuid &uuid, uint16_t shdl, uint16_t ehdl)
{
    SSAP_LOGI("appId=%{public}d, uuid=%{public}s", appId, GET_ENCRYPT_UUID(uuid));
    NLSTK_SsapUuid_S stackUuid = ConvertToSleUuid(uuid);
    NLSTK_Errcode_E ret = NLSTK_SsapClientReadPropertyByUuid(appId, &stackUuid, shdl, ehdl);
    if (ret != NLSTK_ERRCODE_SUCCESS) {
        SSAP_LOGE("fail ! ret=%{public}d", ret);
        std::list<Property> properties;
        OnReadPropertiesByUuidTask(appId, properties, ConvertFromPDUError(ret));
    }
}

void SsapClientStackAdapter::WriteProperty(int appId, const Property &property, bool withoutRsp)
{
    SSAP_LOGD("appId=%{public}d", appId);
    Property p = property;
    NLSTK_VariableData_S value = {};
    value.len = property.value_.size();
    if (value.len == 0) {
        OnWritePropertyTask(appId, p, SsapStatus::SSAP_INVALID_PARAM);
        return;
    }
    value.data = new (std::nothrow) uint8_t[value.len];
    if (value.data == nullptr) {
        SSAP_LOGE("value.data is nullptr");
        OnWritePropertyTask(appId, p, SsapStatus::SSAP_FAILURE);
        return;
    }
    (void)memset_s(value.data, value.len, 0x00, value.len);
    std::copy(property.value_.begin(), property.value_.end(), value.data);

    NLSTK_Errcode_E ret = NLSTK_SsapClientWriteProperty(appId, property.handle_, &value, withoutRsp);
    if (ret != NLSTK_ERRCODE_SUCCESS) {
        SSAP_LOGE("fail ! ret=%{public}d", ret);
        OnWritePropertyTask(appId, p, ConvertFromPDUError(ret));
        delete[] value.data;
        value.data = nullptr;
        return;
    }
    delete[] value.data;
    value.data = nullptr;
}

void SsapClientStackAdapter::WriteDescriptor(int appId, Descriptor &descriptor, bool withoutRsp)
{
    SSAP_LOGI("appId=%{public}d", appId);
    Descriptor d = descriptor;
    NLSTK_VariableData_S value = {};
    value.len = descriptor.value_.size();
    if (value.len == 0) {
        OnWriteDescriptorTask(appId, d, SsapStatus::SSAP_INVALID_PARAM);
        return;
    }
    value.data = new (std::nothrow) uint8_t[value.len];
    if (value.data == nullptr) {
        SSAP_LOGE("value.data is nullptr");
        OnWriteDescriptorTask(appId, d, SsapStatus::SSAP_FAILURE);
        return;
    }
    (void)memset_s(value.data, value.len, 0x00, value.len);
    std::copy(descriptor.value_.begin(), descriptor.value_.end(), value.data);

    NLSTK_Errcode_E ret = NLSTK_SsapClientWriteDescriptor(
        appId, descriptor.handle_, &value, descriptor.type_, withoutRsp);
    if (ret != NLSTK_ERRCODE_SUCCESS) {
        SSAP_LOGE("fail ! ret=%{public}d", ret);
        OnWriteDescriptorTask(appId, d, ConvertFromPDUError(ret));
        delete[] value.data;
        value.data = nullptr;
        return;
    }
    delete[] value.data;
    value.data = nullptr;
}

void SsapClientStackAdapter::GetPropertyNotification(int appId, const Property &property)
{
    SSAP_LOGI("appId=%{public}d", appId);
    NLSTK_Errcode_E ret = NLSTK_SsapClientGetPropertyNtf(appId, property.handle_);
    if (ret != NLSTK_ERRCODE_SUCCESS) {
        SSAP_LOGE("fail ! ret=%{public}d", ret);
        OnGetPropertyNotificationTask(appId, property, false, ConvertFromPDUError(ret));
    }
}

void SsapClientStackAdapter::SetPropertyNotification(int appId, const Property &property, bool enable)
{
    SSAP_LOGI("appId=%{public}d", appId);
    NLSTK_Errcode_E ret = NLSTK_SsapClientSetPropertyNtf(appId, property.handle_, enable);
    if (ret != NLSTK_ERRCODE_SUCCESS) {
        SSAP_LOGE("fail ! ret=%{public}d", ret);
        OnSetPropertyNotificationTask(appId, property, enable, ConvertFromPDUError(ret));
    }
}

void SsapClientStackAdapter::GetPropertyIndication(int appId, const Property &property)
{
    SSAP_LOGI("appId=%{public}d", appId);
    NLSTK_Errcode_E ret = NLSTK_SsapClientGetPropertyInd(appId, property.handle_);
    if (ret != NLSTK_ERRCODE_SUCCESS) {
        SSAP_LOGE("fail ! ret=%{public}d", ret);
        OnGetPropertyIndicationTask(appId, property, false, ConvertFromPDUError(ret));
    }
}

void SsapClientStackAdapter::SetPropertyIndication(int appId, const Property &property, bool enable)
{
    SSAP_LOGI("appId=%{public}d", appId);
    NLSTK_Errcode_E ret = NLSTK_SsapClientSetPropertyInd(appId, property.handle_, enable);
    if (ret != NLSTK_ERRCODE_SUCCESS) {
        SSAP_LOGE("fail ! ret=%{public}d", ret);
        OnSetPropertyIndicationTask(appId, property, enable, ConvertFromPDUError(ret));
    }
}

void SsapClientStackAdapter::Connect(int appId, const RawAddress &addr, bool autoConnect)
{
    SSAP_LOGD("appId=%{public}d", appId);
    NLSTK_Errcode_E ret = NLSTK_SsapClientConnect(appId);
    if (ret != NLSTK_ERRCODE_SUCCESS) {
        SSAP_LOGE("fail ! ret=%{public}d", ret);
        OnConnectionStateChangedTask(appId, static_cast<uint8_t>(SleConnectState::DISCONNECTED),
            ConvertFromPDUError(ret), 0);
    }
}

void SsapClientStackAdapter::Disconnect(int appId, const RawAddress &addr)
{
    SSAP_LOGI("appId=%{public}d", appId);
    NLSTK_Errcode_E ret = NLSTK_SsapClientDisconnect(appId);
    if (ret != NLSTK_ERRCODE_SUCCESS) {
        SSAP_LOGE("fail ! ret=%{public}d", ret);
    }
}

} // namespace Nearlink
} // namespace OHOS
