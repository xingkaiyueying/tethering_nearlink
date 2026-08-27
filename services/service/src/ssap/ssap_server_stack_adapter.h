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
#ifndef SSAP_SERVER_STACK_ADAPTER_H
#define SSAP_SERVER_STACK_ADAPTER_H

#include "nearlink_types.h"
#include "ssap_data.h"
#include "nlstk_ssap_app_server.h"
#include "nlstk_ssap_app_link.h"

#define NTF_TIMEOUT 15000

namespace OHOS {
namespace Nearlink {

class SsapServerStackCallback {
public:
    virtual ~SsapServerStackCallback() = default;
    virtual void OnMtuChanged(int appId, const RawAddress &addr, uint16_t mtu) {}
    virtual void OnAddService(int appId, Service &service, int ret) {}
    virtual void OnSetPropertyValue(int appId, Property &property, int ret) {}
    virtual void OnSetDescriptorValue(int appId, Descriptor &descriptor, int ret) {}
    virtual void OnReadPropertyAuthorizeRequest(
        int appId, const RawAddress &addr, uint16_t requestId, Property &property) {}
    virtual void OnReadDescriptorAuthorizeRequest(
        int appId, const RawAddress &addr, uint16_t requestId, Descriptor &descriptor) {}
    virtual void OnWritePropertyAuthorizeRequest(
        int appId, const RawAddress &addr, uint16_t requestId, Property &property) {}
    virtual void OnWriteDescriptorAuthorizeRequest(
        int appId, const RawAddress &addr, uint16_t requestId, Descriptor &descriptor) {}
    virtual void OnReadProperty(int appId, const RawAddress &addr, Property &property, int ret) {}
    virtual void OnReadDescriptor(int appId, const RawAddress &addr, Descriptor &descriptor, int ret) {}
    virtual void OnWriteProperty(int appId, const RawAddress &addr, Property &property, int ret) {}
    virtual void OnWriteDescriptor(int appId, const RawAddress &addr, Descriptor &descriptor, int ret) {}
    virtual void OnNotifyProperty(int appId, const RawAddress &addr, Property &property, int ret) {}
    virtual void OnConnectionStateChanged(int appId, const RawAddress &addr, int state, int ret, int reason) {}
    virtual void OnDisable(void) {}
};

class SsapServerStackAdapter {
public:
    explicit SsapServerStackAdapter(SsapServerStackCallback &callback);
    ~SsapServerStackAdapter();

    int RegisterApplication(int &appId);
    void DeregisterApplication(int appId);
    void SetMtu(uint16_t mtu);
    void AddService(int appId, Service &service);
    void RemoveService(int appId, uint16_t handle);
    void ClearServices(int appId);
    bool CheckServiceExistByUuid(int appId, const Uuid &uuid);
    void SetPropertyValue(int appId, Property &property);
    void SetDescriptorValue(int appId, Descriptor &descriptor);
    void AuthorizeResponse(int appId, uint16_t requestId, bool allow);
    void Notify(int appId, Property &property, const RawAddress &addr);
    void Disable(void);

private:
    static void OnMtuChanged(int appId, SLE_Addr_S *addr, uint16_t mtu);
    static void OnAddService(int appId, SSAP_Service_S *service, NLSTK_Errcode_E ret);
    static void OnSetPropertyValue(
        int appId, NLSTK_SsapServerOnSetPropertyParam_S *param, NLSTK_Errcode_E ret);
    static void OnSetDescriptorValue(
        int appId, NLSTK_SsapServerOnSetDescriptorParam_S *param, NLSTK_Errcode_E ret);
    static void OnReadPropertyAuthorizeRequest(
        int appId, uint16_t requestId, NLSTK_SsapServerReadPropertyInfo_S *param);
    static void OnReadDescriptorAuthorizeRequest(
        int appId, uint16_t requestId, NLSTK_SsapServerReadDescriptorInfo_S *param);
    static void OnWritePropertyAuthorizeRequest(
        int appId, uint16_t requestId, NLSTK_SsapServerWritePropertyInfo_S *param);
    static void OnWriteDescriptorAuthorizeRequest(
        int appId, uint16_t requestId, NLSTK_SsapServerWriteDescriptorInfo_S *param);
    static void OnReadProperty(int appId, NLSTK_SsapServerReadPropertyInfo_S *param);
    static void OnReadDescriptor(int appId, NLSTK_SsapServerReadDescriptorInfo_S *param);
    static void OnWriteProperty(int appId, NLSTK_SsapServerWritePropertyInfo_S *param);
    static void OnWriteDescriptor(int appId, NLSTK_SsapServerWriteDescriptorInfo_S *param);
    static void OnNotifyProperty(int appId, NLSTK_SsapServerOnNotifyPropertyParam_S *param, NLSTK_Errcode_E ret);
    static void OnConnectionStateChanged(
        int appId, const SLE_Addr_S *addr, uint8_t state, NLSTK_Errcode_E ret, int reason);
    static void OnDisable(void);

    void OnMtuChangedTask(int appId, const RawAddress addr, uint16_t mtu);
    void OnAddServiceTask(int appId, Service &ssapService, int ret);
    void OnSetPropertyValueTask(int appId, Property &ssapProperty, int ret);
    void OnSetDescriptorValueTask(int appId, Descriptor &ssapDescriptor, int ret);
    void OnReadPropertyAuthorizeRequestTask(
        int appId, const RawAddress &addr, uint16_t requestId, Property &ssapProperty);
    void OnReadDescriptorAuthorizeRequestTask(
        int appId, const RawAddress &addr, uint16_t requestId, Descriptor &ssapDescriptor);
    void OnWritePropertyAuthorizeRequestTask(
        int appId, const RawAddress &addr, uint16_t requestId, Property &ssapProperty);
    void OnWriteDescriptorAuthorizeRequestTask(
        int appId, const RawAddress &addr, uint16_t requestId, Descriptor &ssapDescriptor);
    void OnReadPropertyTask(int appId, const RawAddress &addr, Property &ssapProperty);
    void OnReadDescriptorTask(int appId, const RawAddress &addr, Descriptor &ssapDescriptor);
    void OnWritePropertyTask(int appId, const RawAddress &addr, Property &ssapProperty);
    void OnWriteDescriptorTask(int appId, const RawAddress &addr, Descriptor &ssapDescriptor);
    void OnNotifyPropertyTask(
        int appId, const RawAddress &addr, Property &ssapProperty, int ret);
    void OnConnectionStateChangedTask(
        int appId, const RawAddress &addr, int state, int ret, int reason);
    void OnDisableTask(void);

    void ConvertToServiceType(const Uuid &uuid, bool isPrimary);
    bool FillDescriptorToProperty(
        const Property &srcProperty, NLSTK_SsapServicePropertyParam_S *dstProperty);
    bool FillPropertyToService(const Service &srcService, NLSTK_ServiceParam_S *dstService);
    bool FillMethodToService(const Service &srcService, NLSTK_ServiceParam_S *dstService);
    void FreeStackServiceStatement(NLSTK_SsapServiceStatementParam_S *serviceStatement);
    void FreeStackDescriptor(NLSTK_SsapServiceDescriptorParam_S *descriptors, uint32_t descriptorNum);
    void FreeStackProperty(NLSTK_SsapServicePropertyParam_S *property, uint16_t servicePropertyNum);
    void FreeStackService(NLSTK_ServiceParam_S *stackService);

    NEARLINK_DECLARE_IMPL();
};

} // namespace Nearlink
} // namespace OHOS

#endif // SSAP_SERVER_STACK_ADAPTER_H
