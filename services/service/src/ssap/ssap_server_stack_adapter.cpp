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
#include "ssap_server_stack_adapter.h"

#include "log.h"
#include "ssap_log.h"
#include "ssap_utils.h"
#include "SleInterfaceAdapterSub.h"
#include "SleInterfaceManager.h"
#include "ThreadUtil.h"

#include "ssap_type.h"

namespace OHOS {
namespace Nearlink {
constexpr uint16_t MAX_PROPERTY_NUM = 10000;
static SsapServerStackAdapter *g_ssapServerStackAdapter = nullptr;
struct SsapServerStackAdapter::impl {
    impl(SsapServerStackCallback &callback) : ssapServerStackCbk_(callback) {}

    SsapServerStackCallback &ssapServerStackCbk_;
};

SsapServerStackAdapter::SsapServerStackAdapter(SsapServerStackCallback &callback)
    : pimpl(std::make_unique<impl>(callback))
{
    g_ssapServerStackAdapter = this;
}

SsapServerStackAdapter::~SsapServerStackAdapter()
{
    g_ssapServerStackAdapter = nullptr;
}

void SsapServerStackAdapter::OnMtuChangedTask(int appId, const RawAddress addr, uint16_t mtu)
{
    pimpl->ssapServerStackCbk_.OnMtuChanged(appId, addr, mtu);
}

void SsapServerStackAdapter::OnMtuChanged(int appId, SLE_Addr_S *addr, uint16_t mtu)
{
    SSAP_LOGI("appId=%{public}d", appId);
    NL_CHECK_RETURN(addr != nullptr, "addr is nullptr");

    DoInSsapThread([serverStackAdapter = g_ssapServerStackAdapter, appId,
        rawAddress = RawAddress::ConvertToString(addr->addr), mtu]() -> void {
        NL_CHECK_RETURN(serverStackAdapter != nullptr, "serverStackAdapter is nullptr");
        serverStackAdapter->OnMtuChangedTask(appId, rawAddress, mtu);
    });
}

void SsapServerStackAdapter::OnAddServiceTask(int appId, Service &ssapService, int ret)
{
    pimpl->ssapServerStackCbk_.OnAddService(appId, ssapService, ret);
}

void SsapServerStackAdapter::OnAddService(int appId, SSAP_Service_S *service, NLSTK_Errcode_E ret)
{
    SSAP_LOGI("appId=%{public}d", appId);
    NL_CHECK_RETURN(service != nullptr, "service is nullptr");
    Uuid uuid = Uuid::ConvertFromBytesSle(service->uuid.uuid);
    Service ssapService(service->handle, service->handle, service->endHandle, true, uuid);
    SDF_Vector_S *properties = service->properties;
    NL_CHECK_RETURN(properties != nullptr, "properties is nullptr");
    NL_CHECK_RETURN(properties->size <= MAX_PROPERTY_NUM, "properties size overflow");
    uint16_t propertyNum = static_cast<uint16_t>(properties->size);
    for (uint16_t i = 0; i < propertyNum; i++) {
        SSAP_Property_S *property = static_cast<SSAP_Property_S *>(SDF_VectorElementAt(properties, i));
        if (property == nullptr) {
            SSAP_LOGE("property is null");
            break;
        }
        if (property->handle < service->handle || service->endHandle < property->handle) {
            SSAP_LOGE("out of range prop hdl=%{public}d shdl=%{public}d ehdl=%{public}d", property->handle,
                service->handle, service->endHandle);
            continue;
        }
        Property ssapProperty(property->handle, Uuid::ConvertFromBytesSle(property->uuid.uuid),
            property->operation.operationValue);
        SDF_Vector_S *descriptors = property->descriptors;
        if (descriptors == nullptr) {
            SSAP_LOGE("descriptors is null");
            continue;
        }
        for (size_t j = 0; j < descriptors->size; j++) {
            SSAP_Descriptor_S *descriptor = static_cast<SSAP_Descriptor_S *>(SDF_VectorElementAt(descriptors, j));
            if (!descriptor) {
                SSAP_LOGE("descriptor is null");
                break;
            }
            ssapProperty.descriptors_.emplace_back(Descriptor(property->handle, descriptor->type));
        }
        ssapService.properties_.emplace_back(ssapProperty);
    }

    DoInSsapThread([serverStackAdapter = g_ssapServerStackAdapter, appId,
        s = ssapService, r = ConvertFromPDUError(ret)]() mutable {
        NL_CHECK_RETURN(serverStackAdapter != nullptr, "serverStackAdapter is nullptr");
        serverStackAdapter->OnAddServiceTask(appId, s, r);
    });
}

void SsapServerStackAdapter::OnSetPropertyValueTask(int appId, Property &ssapProperty, int ret)
{
    pimpl->ssapServerStackCbk_.OnSetPropertyValue(appId, ssapProperty, ret);
}

void SsapServerStackAdapter::OnSetPropertyValue(
    int appId, NLSTK_SsapServerOnSetPropertyParam_S *param, NLSTK_Errcode_E ret)
{
    SSAP_LOGI("appId=%{public}d", appId);
    NL_CHECK_RETURN(param != nullptr, "param is nullptr");
    Uuid uuid = Uuid::ConvertFromBytesSle(param->uuid.uuid);

    DoInSsapThread([serverStackAdapter = g_ssapServerStackAdapter, appId,
        ssapProperty = Property(param->handle, uuid), r = ConvertFromPDUError(ret)]() mutable {
        NL_CHECK_RETURN(serverStackAdapter != nullptr, "serverStackAdapter is nullptr");
        serverStackAdapter->OnSetPropertyValueTask(appId, ssapProperty, r);
    });
}

void SsapServerStackAdapter::OnSetDescriptorValueTask(int appId, Descriptor &ssapDescriptor, int ret)
{
    pimpl->ssapServerStackCbk_.OnSetDescriptorValue(appId, ssapDescriptor, ret);
}

void SsapServerStackAdapter::OnSetDescriptorValue(
    int appId, NLSTK_SsapServerOnSetDescriptorParam_S *param, NLSTK_Errcode_E ret)
{
    SSAP_LOGI("appId=%{public}d", appId);
    NL_CHECK_RETURN(param != nullptr, "param is nullptr");

    DoInSsapThread([serverStackAdapter = g_ssapServerStackAdapter, appId,
        ssapDescriptor = Descriptor(param->handle, param->type), r = ConvertFromPDUError(ret)]() mutable {
        NL_CHECK_RETURN(serverStackAdapter != nullptr, "serverStackAdapter is nullptr");
        serverStackAdapter->OnSetDescriptorValueTask(appId, ssapDescriptor, r);
    });
}

void SsapServerStackAdapter::OnReadPropertyAuthorizeRequestTask(
    int appId, const RawAddress &addr, uint16_t requestId, Property &ssapProperty)
{
    pimpl->ssapServerStackCbk_.OnReadPropertyAuthorizeRequest(appId, addr, requestId, ssapProperty);
}

void SsapServerStackAdapter::OnReadPropertyAuthorizeRequest(
    int appId, uint16_t requestId, NLSTK_SsapServerReadPropertyInfo_S *param)
{
    SSAP_LOGI("appId=%{public}d", appId);
    NL_CHECK_RETURN(param != nullptr, "param is nullptr");
    Uuid uuid = Uuid::ConvertFromBytesSle(param->uuid.uuid);

    DoInSsapThread([serverStackAdapter = g_ssapServerStackAdapter, appId,
        rawAddress = RawAddress::ConvertToString(param->addr.addr), requestId,
        ssapProperty = Property(param->handle, uuid)]() mutable {
        NL_CHECK_RETURN(serverStackAdapter != nullptr, "serverStackAdapter is nullptr");
        serverStackAdapter->OnReadPropertyAuthorizeRequestTask(appId, rawAddress, requestId, ssapProperty);
    });
}

void SsapServerStackAdapter::OnReadDescriptorAuthorizeRequestTask(
    int appId, const RawAddress &addr, uint16_t requestId, Descriptor &ssapDescriptor)
{
    pimpl->ssapServerStackCbk_.OnReadDescriptorAuthorizeRequest(appId, addr, requestId, ssapDescriptor);
}

void SsapServerStackAdapter::OnReadDescriptorAuthorizeRequest(
    int appId, uint16_t requestId, NLSTK_SsapServerReadDescriptorInfo_S *param)
{
    SSAP_LOGI("appId=%{public}d", appId);
    NL_CHECK_RETURN(param != nullptr, "param is nullptr");

    DoInSsapThread([serverStackAdapter = g_ssapServerStackAdapter, appId,
        rawAddress = RawAddress::ConvertToString(param->addr.addr), requestId,
        ssapDescriptor = Descriptor(param->handle, param->type)]() mutable {
        NL_CHECK_RETURN(serverStackAdapter != nullptr, "serverStackAdapter is nullptr");
        serverStackAdapter->OnReadDescriptorAuthorizeRequestTask(appId, rawAddress, requestId, ssapDescriptor);
    });
}

void SsapServerStackAdapter::OnWritePropertyAuthorizeRequestTask(
    int appId, const RawAddress &addr, uint16_t requestId, Property &ssapProperty)
{
    pimpl->ssapServerStackCbk_.OnWritePropertyAuthorizeRequest(appId, addr, requestId, ssapProperty);
}

void SsapServerStackAdapter::OnWritePropertyAuthorizeRequest(
    int appId, uint16_t requestId, NLSTK_SsapServerWritePropertyInfo_S *param)
{
    SSAP_LOGI("appId=%{public}d", appId);
    NL_CHECK_RETURN(param != nullptr, "param is nullptr");
    NL_CHECK_RETURN(param->value.data != nullptr, "param->value.data is nullptr");
    std::vector<uint8_t> value(param->value.data, param->value.data + param->value.len);
    Uuid uuid = Uuid::ConvertFromBytesSle(param->uuid.uuid);

    DoInSsapThread([serverStackAdapter = g_ssapServerStackAdapter, appId,
        rawAddress = RawAddress::ConvertToString(param->addr.addr), requestId,
        ssapProperty = Property(param->handle, uuid, std::move(value))]() mutable {
        NL_CHECK_RETURN(serverStackAdapter != nullptr, "serverStackAdapter is nullptr");
        serverStackAdapter->OnWritePropertyAuthorizeRequestTask(appId, rawAddress, requestId, ssapProperty);
    });
}

void SsapServerStackAdapter::OnWriteDescriptorAuthorizeRequestTask(
    int appId, const RawAddress &addr, uint16_t requestId, Descriptor &ssapDescriptor)
{
    pimpl->ssapServerStackCbk_.OnWriteDescriptorAuthorizeRequest(appId, addr, requestId, ssapDescriptor);
}

void SsapServerStackAdapter::OnWriteDescriptorAuthorizeRequest(
    int appId, uint16_t requestId, NLSTK_SsapServerWriteDescriptorInfo_S *param)
{
    SSAP_LOGI("appId=%{public}d", appId);
    NL_CHECK_RETURN(param != nullptr, "param is nullptr");
    NL_CHECK_RETURN(param->value.data != nullptr, "param->value.data is nullptr");
    std::vector<uint8_t> value(param->value.data, param->value.data + param->value.len);

    DoInSsapThread([serverStackAdapter = g_ssapServerStackAdapter, appId,
        rawAddress = RawAddress::ConvertToString(param->addr.addr), requestId,
        ssapDescriptor = Descriptor(param->handle, param->type, std::move(value))]() mutable {
        NL_CHECK_RETURN(serverStackAdapter != nullptr, "serverStackAdapter is nullptr");
        serverStackAdapter->OnWriteDescriptorAuthorizeRequestTask(appId, rawAddress, requestId, ssapDescriptor);
    });
}

void SsapServerStackAdapter::OnReadPropertyTask(int appId, const RawAddress &addr, Property &ssapProperty)
{
    pimpl->ssapServerStackCbk_.OnReadProperty(appId, addr, ssapProperty, SsapStatus::SSAP_SUCCESS);
}

void SsapServerStackAdapter::OnReadProperty(int appId, NLSTK_SsapServerReadPropertyInfo_S *param)
{
    SSAP_LOGI("appId=%{public}d", appId);
    NL_CHECK_RETURN(param != nullptr, "param is nullptr");
    Uuid uuid = Uuid::ConvertFromBytesSle(param->uuid.uuid);

    DoInSsapThread([serverStackAdapter = g_ssapServerStackAdapter, appId,
        rawAddress = RawAddress::ConvertToString(param->addr.addr),
        ssapProperty = Property(param->handle, uuid)]() mutable {
        NL_CHECK_RETURN(serverStackAdapter != nullptr, "serverStackAdapter is nullptr");
        serverStackAdapter->OnReadPropertyTask(appId, rawAddress, ssapProperty);
    });
}

void SsapServerStackAdapter::OnReadDescriptorTask(int appId, const RawAddress &addr, Descriptor &ssapDescriptor)
{
    pimpl->ssapServerStackCbk_.OnReadDescriptor(appId, addr, ssapDescriptor, SsapStatus::SSAP_SUCCESS);
}

void SsapServerStackAdapter::OnReadDescriptor(int appId, NLSTK_SsapServerReadDescriptorInfo_S *param)
{
    SSAP_LOGI("appId=%{public}d", appId);
    NL_CHECK_RETURN(param != nullptr, "param is nullptr");

    DoInSsapThread([serverStackAdapter = g_ssapServerStackAdapter, appId,
        rawAddress = RawAddress::ConvertToString(param->addr.addr),
        ssapDescriptor = Descriptor(param->handle, param->type)]() mutable {
        NL_CHECK_RETURN(serverStackAdapter != nullptr, "serverStackAdapter is nullptr");
        serverStackAdapter->OnReadDescriptorTask(appId, rawAddress, ssapDescriptor);
    });
}

void SsapServerStackAdapter::OnWritePropertyTask(int appId, const RawAddress &addr, Property &ssapProperty)
{
    pimpl->ssapServerStackCbk_.OnWriteProperty(appId, addr, ssapProperty, SsapStatus::SSAP_SUCCESS);
}

void SsapServerStackAdapter::OnWriteProperty(int appId, NLSTK_SsapServerWritePropertyInfo_S *param)
{
    SSAP_LOGI("appId=%{public}d", appId);
    NL_CHECK_RETURN(param != nullptr, "param is nullptr");
    NL_CHECK_RETURN(param->value.data != nullptr, "param->value.data is nullptr");
    std::vector<uint8_t> value(param->value.data, param->value.data + param->value.len);
    Uuid uuid = Uuid::ConvertFromBytesSle(param->uuid.uuid);

    DoInSsapThread([serverStackAdapter = g_ssapServerStackAdapter, appId,
        rawAddress = RawAddress::ConvertToString(param->addr.addr),
        ssapProperty = Property(param->handle, uuid, std::move(value))]() mutable {
        NL_CHECK_RETURN(serverStackAdapter != nullptr, "serverStackAdapter is nullptr");
        serverStackAdapter->OnWritePropertyTask(appId, rawAddress, ssapProperty);
    });
}

void SsapServerStackAdapter::OnWriteDescriptorTask(int appId, const RawAddress &addr, Descriptor &ssapDescriptor)
{
    pimpl->ssapServerStackCbk_.OnWriteDescriptor(appId, addr, ssapDescriptor, SsapStatus::SSAP_SUCCESS);
}

void SsapServerStackAdapter::OnWriteDescriptor(int appId, NLSTK_SsapServerWriteDescriptorInfo_S *param)
{
    SSAP_LOGI("appId=%{public}d", appId);
    NL_CHECK_RETURN(param != nullptr, "param is nullptr");
    NL_CHECK_RETURN(param->value.data != nullptr, "param->value.data is nullptr");
    std::vector<uint8_t> value(param->value.data, param->value.data + param->value.len);

    DoInSsapThread([serverStackAdapter = g_ssapServerStackAdapter, appId,
        rawAddress = RawAddress::ConvertToString(param->addr.addr),
        ssapDescriptor = Descriptor(param->handle, param->type, std::move(value))]() mutable {
        NL_CHECK_RETURN(serverStackAdapter != nullptr, "serverStackAdapter is nullptr");
        serverStackAdapter->OnWriteDescriptorTask(appId, rawAddress, ssapDescriptor);
    });
}

void SsapServerStackAdapter::OnNotifyPropertyTask(
    int appId, const RawAddress &addr, Property &ssapProperty, int ret)
{
    pimpl->ssapServerStackCbk_.OnNotifyProperty(appId, addr, ssapProperty, ret);
}

void SsapServerStackAdapter::OnNotifyProperty(int appId, NLSTK_SsapServerOnNotifyPropertyParam_S *param,
    NLSTK_Errcode_E ret)
{
    SSAP_LOGI("appId=%{public}d", appId);
    NL_CHECK_RETURN(param != nullptr, "param is nullptr");
    Uuid uuid = Uuid::ConvertFromBytesSle(param->uuid.uuid);

    DoInSsapThread([serverStackAdapter = g_ssapServerStackAdapter, appId,
        rawAddress = RawAddress::ConvertToString(param->addr.addr),
        ssapProperty = Property(param->handle, uuid), r = ConvertFromPDUError(ret)]() mutable {
        NL_CHECK_RETURN(serverStackAdapter != nullptr, "serverStackAdapter is nullptr");
        serverStackAdapter->OnNotifyPropertyTask(appId, rawAddress, ssapProperty, r);
    });
}

void SsapServerStackAdapter::OnConnectionStateChangedTask(
    int appId, const RawAddress &addr, int state, int ret, int reason)
{
    pimpl->ssapServerStackCbk_.OnConnectionStateChanged(appId, addr, state, ret, reason);
}

void SsapServerStackAdapter::OnConnectionStateChanged(
    int appId, const SLE_Addr_S *addr, uint8_t state, NLSTK_Errcode_E ret, int reason)
{
    SSAP_LOGI("appId=%{public}d", appId);
    NL_CHECK_RETURN(addr != nullptr, "addr is nullptr");

    DoInSsapThread([serverStackAdapter = g_ssapServerStackAdapter, appId,
        rawAddress = RawAddress::ConvertToString(addr->addr),
        s = ConvertStateFromStackSsapState(state), r = ConvertFromPDUError(ret), reason]() -> void {
        NL_CHECK_RETURN(serverStackAdapter != nullptr, "serverStackAdapter is nullptr");
        serverStackAdapter->OnConnectionStateChangedTask(appId, rawAddress, s, r, reason);
    });
}

void SsapServerStackAdapter::OnDisableTask(void)
{
    pimpl->ssapServerStackCbk_.OnDisable();
}

void SsapServerStackAdapter::OnDisable(void)
{
    SSAP_LOGI("enter");
    DoInSsapThread([serverStackAdapter = g_ssapServerStackAdapter]() -> void {
        NL_CHECK_RETURN(serverStackAdapter != nullptr, "serverStackAdapter is nullptr");
        serverStackAdapter->OnDisableTask();
    });
}

void SsapServerStackAdapter::Disable()
{
    NLSTK_Errcode_E ret = NLSTK_SsapCleanServerApp(OnDisable);
    SSAP_LOGI("enter");
    if (ret != NLSTK_ERRCODE_SUCCESS) {
        SSAP_LOGE("fail ! ret=%{public}d", ret);
    }
}

int SsapServerStackAdapter::RegisterApplication(int &appId)
{
    NLSTK_SsapAppServerCb_S cbk = {};
    cbk.onMtuChanged = &OnMtuChanged;
    cbk.onAddService = &OnAddService;
    cbk.onSetPropertyValue = &OnSetPropertyValue;
    cbk.onSetDescriptorValue = &OnSetDescriptorValue;
    cbk.onReadPropertyAuthorizeRequest = &OnReadPropertyAuthorizeRequest;
    cbk.onReadDescriptorAuthorizeRequest = &OnReadDescriptorAuthorizeRequest;
    cbk.onWritePropertyAuthorizeRequest = &OnWritePropertyAuthorizeRequest;
    cbk.onWriteDescriptorAuthorizeRequest = &OnWriteDescriptorAuthorizeRequest;
    cbk.onReadProperty = &OnReadProperty;
    cbk.onReadDescriptor = &OnReadDescriptor;
    cbk.onWriteProperty = &OnWriteProperty;
    cbk.onWriteDescriptor = &OnWriteDescriptor;
    cbk.onNotifyProperty = &OnNotifyProperty;
    cbk.onConnectionStateChanged = &OnConnectionStateChanged;

    /* 协议栈只保存一份cbk */
    NLSTK_Errcode_E ret = NLSTK_SsapServerRegApp(&cbk, &appId);
    HILOGI("appId=%{public}d", appId);
    NL_CHECK_RETURN_RET(ret == NLSTK_ERRCODE_SUCCESS, ConvertFromPDUError(ret),
        "reg server stack fail=%{public}d, appId=%{public}d", ret, appId);
    return SsapStatus::SSAP_SUCCESS;
}

void SsapServerStackAdapter::DeregisterApplication(int appId)
{
    SSAP_LOGI("appId=%{public}d", appId);
    NLSTK_SsapServerDeregisterApplication(appId);
}

void SsapServerStackAdapter::SetMtu(uint16_t mtu)
{
    SSAP_LOGD("set mtu=%{public}d", mtu);
    NLSTK_Errcode_E ret = NLSTK_SsapServerSetMtu(mtu);
    if (ret != NLSTK_ERRCODE_SUCCESS) {
        SSAP_LOGE("fail ! ret=%{public}d", ret);
    }
}

bool SsapServerStackAdapter::FillDescriptorToProperty(
    const Property &srcProperty, NLSTK_SsapServicePropertyParam_S *dstProperty)
{
    NL_CHECK_RETURN_RET(dstProperty != nullptr, false, "dstProperty is nullptr");
    HILOGI("srcProperty.descriptors_.size=%{public}d", srcProperty.descriptors_.size());
    uint16_t descriptorNum = srcProperty.descriptors_.size();
    dstProperty->descriptorNum = descriptorNum;
    if (descriptorNum == 0) {
        HILOGI("descriptorNum == 0");
        return true;
    }
    dstProperty->descriptors = new (std::nothrow) NLSTK_SsapServiceDescriptorParam_S[descriptorNum];
    NL_CHECK_RETURN_RET(dstProperty->descriptors != nullptr, false, "descriptors is nullptr");
    (void)memset_s(dstProperty->descriptors, sizeof(NLSTK_SsapServiceDescriptorParam_S) * descriptorNum, 0x00,
        sizeof(NLSTK_SsapServiceDescriptorParam_S) * descriptorNum);
    for (uint16_t i = 0; i < descriptorNum; i++) {
        dstProperty->descriptors[i].permission.permissionValue = (uint8_t)srcProperty.descriptors_[i].permission_;
        dstProperty->descriptors[i].type = (NLSTK_SsapDescriptorType_E)srcProperty.descriptors_[i].type_;
        dstProperty->descriptors[i].operation.operationValue = srcProperty.opInd_;
        uint32_t valLen = srcProperty.descriptors_[i].value_.size();
        dstProperty->descriptors[i].val.len = valLen;
        if (valLen != 0) {
            dstProperty->descriptors[i].val.data = new (std::nothrow) uint8_t[valLen];
            NL_CHECK_RETURN_RET(dstProperty->descriptors[i].val.data != nullptr, false, "val.data is nullptr");
            (void)memset_s(dstProperty->descriptors[i].val.data, valLen, 0x00, valLen);
            std::copy(srcProperty.descriptors_[i].value_.begin(), srcProperty.descriptors_[i].value_.end(),
                dstProperty->descriptors[i].val.data);
        }
    }
    return true;
}

bool SsapServerStackAdapter::FillPropertyToService(const Service &srcService, NLSTK_ServiceParam_S *dstService)
{
    NL_CHECK_RETURN_RET(dstService != nullptr, false, "dstService is nullptr");
    uint32_t servicePropertyNum = srcService.properties_.size();
    dstService->servicePropertyNum = servicePropertyNum;
    if (servicePropertyNum == 0) {
        return true;
    }
    dstService->property = new (std::nothrow) NLSTK_SsapServicePropertyParam_S[servicePropertyNum];
    NL_CHECK_RETURN_RET(dstService->property != nullptr, false, "property is nullptr");
    (void)memset_s(dstService->property, sizeof(NLSTK_SsapServicePropertyParam_S) * servicePropertyNum, 0x00,
        sizeof(NLSTK_SsapServicePropertyParam_S) * servicePropertyNum);
    for (uint32_t i = 0; i < servicePropertyNum; i++) {
        dstService->property[i].uuid = ConvertToSleUuid(srcService.properties_[i].uuid_);
        dstService->property[i].permission.permissionValue = srcService.properties_[i].permission_;
        dstService->property[i].operation.operationValue = srcService.properties_[i].opInd_;

        uint32_t valLen = srcService.properties_[i].value_.size();
        dstService->property[i].val.len = valLen;
        if (valLen != 0) {
            dstService->property[i].val.data = new (std::nothrow) uint8_t[valLen];
            NL_CHECK_RETURN_RET(dstService->property[i].val.data != nullptr, false, "val.data is nullptr");
            (void)memset_s(dstService->property[i].val.data, valLen, 0x00, valLen);
            std::copy(srcService.properties_[i].value_.begin(), srcService.properties_[i].value_.end(),
                dstService->property[i].val.data);
        }

        if (!FillDescriptorToProperty(srcService.properties_[i], &dstService->property[i])) {
            return false;
        }
    }
    return true;
}

void SsapServerStackAdapter::FreeStackServiceStatement(NLSTK_SsapServiceStatementParam_S *serviceStatement)
{
    NL_CHECK_RETURN(serviceStatement != nullptr, "serviceStatement is nullptr");
    NL_CHECK_RETURN(serviceStatement->descriptors != nullptr && serviceStatement->descriptorNum != 0,
        "serviceStatement->descriptors is nullptr");

    for (uint16_t i = 0; i < serviceStatement->descriptorNum; i++) {
        if (serviceStatement->descriptors[i].val.data == nullptr) {
            continue;
        }
        delete[] serviceStatement->descriptors[i].val.data;
        serviceStatement->descriptors[i].val.data = nullptr;
    }
    delete[] serviceStatement->descriptors;
    serviceStatement->descriptors = nullptr;
}

void SsapServerStackAdapter::FreeStackDescriptor(NLSTK_SsapServiceDescriptorParam_S *descriptors,
    uint32_t descriptorNum)
{
    NL_CHECK_RETURN(descriptors != nullptr && descriptorNum != 0, "descriptors is nullptr");

    for (uint32_t i = 0; i < descriptorNum; i++) {
        if (descriptors[i].val.data == nullptr) {
            continue;
        }
        delete[] descriptors[i].val.data;
        descriptors[i].val.data = nullptr;
    }
    delete[] descriptors;
    descriptors = nullptr;
}

void SsapServerStackAdapter::FreeStackProperty(NLSTK_SsapServicePropertyParam_S *property, uint16_t servicePropertyNum)
{
    NL_CHECK_RETURN(property != nullptr && servicePropertyNum != 0, "property is nullptr");

    for (uint16_t i = 0; i < servicePropertyNum; i++) {
        if (property[i].val.data != nullptr) {
            delete[] property[i].val.data;
            property[i].val.data = nullptr;
        }

        FreeStackDescriptor(property[i].descriptors, property[i].descriptorNum);
        property[i].descriptors = nullptr;
    }
    delete[] property;
    property = nullptr;
}

void SsapServerStackAdapter::FreeStackService(NLSTK_ServiceParam_S *stackService)
{
    NL_CHECK_RETURN(stackService != nullptr, "stackService is nullptr");
    // 释放serviceStatement
    FreeStackServiceStatement(&stackService->serviceStatement);

    // 释放serviceReference
    if (stackService->serviceReference != nullptr) {
        delete[] stackService->serviceReference;
        stackService->serviceReference = nullptr;
    }

    // 释放property
    FreeStackProperty(stackService->property, stackService->servicePropertyNum);
    stackService->property = nullptr;

    // 释放method
    if (stackService->method != nullptr) {
        delete[] stackService->method;
        stackService->method = nullptr;
    }

    // 释放event
    if (stackService->event != nullptr) {
        delete[] stackService->event;
        stackService->event = nullptr;
    }
}

void SsapServerStackAdapter::AddService(int appId, Service &service)
{
    SSAP_LOGI("appId=%{public}d, uuid=%{public}s", appId, GET_ENCRYPT_UUID(service.uuid_));

    NLSTK_ServiceParam_S stackService = {};
    stackService.serviceStatement.uuid = ConvertToSleUuid(service.uuid_);

    if (!FillPropertyToService(service, &stackService)) {
        FreeStackService(&stackService);
        OnAddServiceTask(appId, service, SsapStatus::SSAP_FAILURE);
        return;
    }
    if (!FillMethodsToStackService(service, &stackService)) {
        FreeStackService(&stackService);
        OnAddServiceTask(appId, service, SsapStatus::SSAP_FAILURE);
        return;
    }

    NLSTK_Errcode_E ret = NLSTK_SsapServerAddService(appId, &stackService);
    if (ret != NLSTK_ERRCODE_SUCCESS) {
        SSAP_LOGE("fail ! ret=%{public}d", ret);
        OnAddServiceTask(appId, service, ConvertFromPDUError(ret));
        FreeStackService(&stackService);
        return;
    }

    FreeStackService(&stackService);
}

void SsapServerStackAdapter::RemoveService(int appId, uint16_t handle)
{
    SSAP_LOGI("appId=%{public}d, handle=%{public}d", appId, handle);
    NLSTK_Errcode_E ret = NLSTK_SsapServerRemoveService(appId, handle);
    if (ret != NLSTK_ERRCODE_SUCCESS) {
        SSAP_LOGE("fail ! ret=%{public}d", ret);
    }
}

void SsapServerStackAdapter::ClearServices(int appId)
{
    SSAP_LOGI("appId=%{public}d", appId);
    NLSTK_Errcode_E ret = NLSTK_SsapServerClearServices(appId);
    if (ret != NLSTK_ERRCODE_SUCCESS) {
        SSAP_LOGE("fail ! ret=%{public}d", ret);
    }
}

bool SsapServerStackAdapter::CheckServiceExistByUuid(int appId, const Uuid &uuid)
{
    SSAP_LOGI("appId=%{public}d, uuid=%{public}s", appId, GET_ENCRYPT_UUID(uuid));
    NLSTK_SsapUuid_S stackUuid = ConvertToSleUuid(uuid);
    bool isExist = false;
    NLSTK_Errcode_E ret = NLSTK_SsapServerCheckServiceExistByUuid(appId, &stackUuid, &isExist);
    if (ret != NLSTK_ERRCODE_SUCCESS) {
        SSAP_LOGE("fail ! ret=%{public}d", ret);
        return false;
    }

    return isExist;
}

void SsapServerStackAdapter::SetPropertyValue(int appId, Property &property)
{
    SSAP_LOGI("appId=%{public}d", appId);
    NLSTK_VariableData_S value = {};
    value.len = property.value_.size();
    if (value.len == 0) {
        SSAP_LOGE("value.len == 0");
        OnSetPropertyValueTask(appId, property, SsapStatus::SSAP_INVALID_PARAM);
        return;
    }
    value.data = new (std::nothrow) uint8_t[value.len];
    if (value.data == nullptr) {
        SSAP_LOGE("value.data is nullptr");
        OnSetPropertyValueTask(appId, property, SsapStatus::SSAP_FAILURE);
        return;
    }
    (void)memset_s(value.data, value.len, 0x00, value.len);
    std::copy(property.value_.begin(), property.value_.end(), value.data);

    NLSTK_Errcode_E ret = NLSTK_SsapServerUpdatePropertyValue(appId, property.handle_, &value);
    if (ret != NLSTK_ERRCODE_SUCCESS) {
        SSAP_LOGE("fail ! ret=%{public}d", ret);
        OnSetPropertyValueTask(appId, property, ConvertFromPDUError(ret));
        delete[] value.data;
        return;
    }
    delete[] value.data;
}

void SsapServerStackAdapter::SetDescriptorValue(int appId, Descriptor &descriptor)
{
    SSAP_LOGI("appId=%{public}d", appId);
    NLSTK_VariableData_S value = {};
    value.len = descriptor.value_.size();
    if (value.len == 0) {
        SSAP_LOGE("value.len == 0");
        OnSetDescriptorValueTask(appId, descriptor, SsapStatus::SSAP_INVALID_PARAM);
        return;
    }
    value.data = new (std::nothrow) uint8_t[value.len];
    if (value.data == nullptr) {
        SSAP_LOGE("value.data is nullptr");
        OnSetDescriptorValueTask(appId, descriptor, SsapStatus::SSAP_FAILURE);
        return;
    }
    (void)memset_s(value.data, value.len, 0x00, value.len);
    std::copy(descriptor.value_.begin(), descriptor.value_.end(), value.data);

    NLSTK_Errcode_E ret = NLSTK_SsapServerUpdateDescriptorValue(appId, descriptor.handle_, descriptor.type_, &value);
    if (ret != NLSTK_ERRCODE_SUCCESS) {
        SSAP_LOGE("fail ! ret=%{public}d", ret);
        OnSetDescriptorValueTask(appId, descriptor, ConvertFromPDUError(ret));
        delete[] value.data;
        return;
    }
    delete[] value.data;
}

void SsapServerStackAdapter::AuthorizeResponse(int appId, uint16_t requestId, bool allow)
{
    SSAP_LOGI("appId=%{public}d", appId);
    NLSTK_Errcode_E ret = NLSTK_SsapServerAuthorizeResult(appId, requestId, allow);
    if (ret != NLSTK_ERRCODE_SUCCESS) {
        SSAP_LOGE("fail ! ret=%{public}d", ret);
    }
}

void SsapServerStackAdapter::Notify(int appId, Property &property, const RawAddress &addr)
{
    SSAP_LOGD("appId=%{public}d", appId);
    NLSTK_VariableData_S value = {};
    value.len = property.value_.size();
    if (value.len == 0) {
        SSAP_LOGE("value.len == 0");
        OnNotifyPropertyTask(appId, addr, property, SsapStatus::SSAP_INVALID_PARAM);
        return;
    }
    value.data = new (std::nothrow) uint8_t[value.len];
    if (value.data == nullptr) {
        SSAP_LOGE("value.data is nullptr");
        OnNotifyPropertyTask(appId, addr, property, SsapStatus::SSAP_FAILURE);
        return;
    }
    (void)memset_s(value.data, value.len, 0x00, value.len);
    std::copy(property.value_.begin(), property.value_.end(), value.data);

    SLE_Addr_S stackAddr = ConvertToSleAddr(addr);
    NLSTK_Errcode_E ret = NLSTK_SsapServerUpdateAndNotifyProperty(appId, property.handle_, &stackAddr, &value);
    if (ret != NLSTK_ERRCODE_SUCCESS) {
        SSAP_LOGE("fail ! ret=%{public}d", ret);
        OnNotifyPropertyTask(appId, addr, property, ConvertFromPDUError(ret));
        delete[] value.data;
        return;
    }
    delete[] value.data;
}
} // namespace Nearlink
} // namespace OHOS
