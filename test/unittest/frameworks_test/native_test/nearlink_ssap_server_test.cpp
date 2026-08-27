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

#include <array>
#include <gtest/gtest.h>

#include "nearlink_access_token_mock.h"
#include "nearlink_def.h"
#include "nearlink_host.h"
#include "nearlink_ssap_server.cpp"
#include "nearlink_remote_device.h"
#include "log.h"

namespace OHOS {
namespace Nearlink {
namespace TEST {
using namespace testing::ext;

class SsapServerCallbackTest final: public Nearlink::SsapServerCallback {
public:
    SsapServerCallbackTest() {};
    ~SsapServerCallbackTest() {};

    void OnConnectionStateUpdate(const NearlinkRemoteDevice &device, int state, int reason)
    {
        HILOGI("state(%{public}d)", state);
    }
    void OnServiceAdded(std::shared_ptr<SsapService> service, int ret) {}
    void OnPropertyReadRequest(
        const NearlinkRemoteDevice &device, const SsapProperty &property, int ret) {}
    void OnPropertyWriteRequest(
        const NearlinkRemoteDevice &device, const SsapProperty &property, int ret) {}
    void OnDescriptorReadRequest(const NearlinkRemoteDevice &device, const SsapDescriptor &descriptor, int ret) {}
    void OnDescriptorWriteRequest(
        const NearlinkRemoteDevice &device, const SsapDescriptor &descriptor, int ret) {}
    void OnMtuUpdate(const NearlinkRemoteDevice &device, int mtu) {}
    void OnNotifyPropertyChanged(
        const NearlinkRemoteDevice &device, const UUID &uuid, uint16_t handle, int result) {}
    void OnNotifyEventChanged(
        const NearlinkRemoteDevice &device, const UUID &uuid, uint16_t handle, int result) {}
    void OnConnectionParameterChanged(
        const NearlinkRemoteDevice &device, int interval, int latency, int timeout, int status) {}
};

class NearlinkSsapServerTest : public testing::Test {
public:
    static void SetUpTestCase();
    static void TearDownTestCase();
    void SetUp();
    void TearDown();

    std::shared_ptr<SsapServerCallbackTest> ssapServercallback_ {};
    std::shared_ptr<SsapServer::impl::NearlinkSsapServerCallbackStubImpl> serverCallbackImpl_;
    std::shared_ptr<SsapServer> ssapServer_ {};
};

void NearlinkSsapServerTest::SetUpTestCase()
{
    HILOGI("SetUpTestCase start");
    NearlinkAccessTokenMock::SetNativeTokenInfo();
    if (!NearlinkHost::GetInstance().IsSleEnabled()) {
        HILOGI("start enable nearlink");
        NearlinkHost::GetInstance().EnableNl();
        sleep(2); // sleep 2s
    }
    HILOGI("SetUpTestCase end");
}

void NearlinkSsapServerTest::TearDownTestCase()
{}

void NearlinkSsapServerTest::SetUp()
{
    ssapServercallback_ = std::make_shared<SsapServerCallbackTest>();
    ssapServer_ = SsapServer::CreateSsapServer(ssapServercallback_);
}

void NearlinkSsapServerTest::TearDown()
{
    ssapServer_ = nullptr;
    ssapServercallback_ = nullptr;
}

/**
 * @tc.name: NearlinkSsapServerTest001
 * @tc.desc: Test AddService and RemoveSsapService
 * @tc.type: FUNC
 */
HWTEST_F(NearlinkSsapServerTest, NearlinkSsapServerTest001_RemoveSsapService, TestSize.Level1)
{
    HILOGI("NearlinkSsapServerTest001 start");
    EXPECT_NE(nullptr, ssapServer_);

    Nearlink::UUID uuid =  Nearlink::UUID::FromString("11223344-0000-1000-8000-00805F9B34FB");
    SsapService service(uuid, SsapServiceType::VENDOR_PROMARY);
    NlErrCode ret = ssapServer_->AddService(service);
    EXPECT_EQ(NL_NO_ERROR, ret);
    sleep(2); // sleep 2s
    ret = ssapServer_->RemoveSsapService(service);
    EXPECT_EQ(NL_NO_ERROR, ret);
    HILOGI("NearlinkSsapServerTest001 end");
}

/**
 * @tc.name: NearlinkSsapServerTest_IpShareDiscoveryProbe
 * @tc.desc: Register the Demo marker service and IPoSL configuration service with four properties and one method.
 * @tc.type: FUNC
 */
HWTEST_F(NearlinkSsapServerTest, NearlinkSsapServerTest_IpShareDiscoveryProbe, TestSize.Level1)
{
    ASSERT_NE(nullptr, ssapServer_);

    UUID markerUuid = UUID::FromString("7C2305C3-570A-4FDD-BB7A-01BF1A714683");
    SsapService markerService(markerUuid, SsapServiceType::VENDOR_PROMARY);
    ASSERT_EQ(NL_NO_ERROR, ssapServer_->AddService(markerService));

    UUID configUuid = UUID::FromString("5B285005-4555-4FA5-B957-C29C0D8A60C3");
    SsapService configService(configUuid, SsapServiceType::VENDOR_PROMARY);
    constexpr std::array<const char *, 4> PROPERTY_UUIDS = {
        "FA361B52-CB0A-49ED-8427-1A675ACCD9DB",
        "E4720AB0-E62A-42D2-AD96-98627CEEAEDB",
        "2E68C566-E314-466D-9629-402F16CAB219",
        "43117FB2-909E-4B70-B6F9-1052D729C89B",
    };
    constexpr uint8_t INITIAL_VALUES[] = {0x00};
    constexpr uint32_t OPERATION_INDICATION =
        static_cast<uint32_t>(SsapProperty::OperationIndication::OPERATION_READ);
    for (const char *propertyUuid : PROPERTY_UUIDS) {
        SsapProperty property(static_cast<int>(SsapProperty::PropertyType::ENTRY_TYPE_VENDOR_PROPERTY),
            UUID::FromString(propertyUuid), OPERATION_INDICATION, 0);
        property.SetValue(INITIAL_VALUES, sizeof(INITIAL_VALUES));
        configService.AddProperty(property);
    }

    SsapMethod configMethod(static_cast<int>(SsapMethod::MethodType::ENTRY_TYPE_VENDOR_METHOD),
        UUID::FromString("7AA3120E-F0D2-4560-B711-A5B618B7A32B"), 0);
    configService.AddMethod(configMethod);
    ASSERT_EQ(NL_NO_ERROR, ssapServer_->AddService(configService));

    // The peer-side discovery test must additionally assert two services, four non-empty properties,
    // one method, and invoke the method using the discovered handle rather than a hard-coded handle.
}

/**
 * @tc.name: NearlinkSsapServerTest002
 * @tc.desc: Test ClearServices
 * @tc.type: FUNC
 */
HWTEST_F(NearlinkSsapServerTest, NearlinkSsapServerTest002_ClearServices, TestSize.Level1)
{
    HILOGI("NearlinkSsapServerTest002 start");

    Nearlink::UUID uuid =  Nearlink::UUID::FromString("11223344-0000-1000-8000-00805F9B34FB");
    SsapService service(uuid, SsapServiceType::VENDOR_PROMARY);
    NlErrCode ret = ssapServer_->AddService(service);
    EXPECT_EQ(NL_NO_ERROR, ret);
    sleep(2); // sleep 2s
    ret = ssapServer_->ClearServices();
    EXPECT_EQ(NL_NO_ERROR, ret);
    HILOGI("NearlinkSsapServerTest002 end");
}

/**
 * @tc.name: NearlinkSsapServerTest003
 * @tc.desc: Test GetService, SetPropertyValue, SetDescriptorValue and NotifyPropertyChanged
 * @tc.type: FUNC
 */
HWTEST_F(NearlinkSsapServerTest, NearlinkSsapServerTest003_NotifyPropertyChanged, TestSize.Level1)
{
    HILOGI("NearlinkSsapServerTest003 start");

    Nearlink::UUID serviceUuid =  Nearlink::UUID::FromString("11223344-0000-1000-8000-00805F9B34FB");
    SsapService service(serviceUuid, SsapServiceType::VENDOR_PROMARY);
    Nearlink::UUID propertyUuid =  Nearlink::UUID::FromString("11223344-0000-1000-8000-00805F9B1111");
    uint32_t operationIndication = static_cast<uint32_t>(SsapProperty::OperationIndication::OPERATION_READ) +
        static_cast<uint32_t>(SsapProperty::OperationIndication::OPERATION_WRITE_NO_RESPONSE) +
        static_cast<uint32_t>(SsapProperty::OperationIndication::OPERATION_NOTIFY);
    SsapProperty property(static_cast<int>(SsapProperty::PropertyType::ENTRY_TYPE_PROPERTY), propertyUuid,
        operationIndication, 0);
    service.AddProperty(property);
    NlErrCode ret = ssapServer_->AddService(service);
    EXPECT_EQ(NL_NO_ERROR, ret);
    sleep(2); // sleep 2s

    std::shared_ptr<SsapService> servicePtr = ssapServer_->GetService(serviceUuid, true);
    EXPECT_NE(nullptr, servicePtr);
    SsapDescriptor descriptor(SsapDescriptor::PropertyDescriptorType::DESCRIPTOR_TYPE_CLIENT_PROPERTY_CONFIG, 0);
    ret = ssapServer_->SetDescriptorValue(descriptor);
    const size_t valueLen = 2; // length of property value is 2
    uint8_t propertyValue[valueLen] = {1, 1};
    property.SetValue(propertyValue, valueLen);
    ret = ssapServer_->SetPropertyValue(property);
    EXPECT_EQ(NL_NO_ERROR, ret);
    NearlinkRemoteDevice device {};
    ret = ssapServer_->NotifyPropertyChanged(device, property, false);
    EXPECT_NE(NL_NO_ERROR, ret);
    HILOGI("NearlinkSsapServerTest003 end");
}

/**
 * @tc.name: NearlinkSsapServerTest004
 * @tc.desc: Test CancelConnection
 * @tc.type: FUNC
 */
HWTEST_F(NearlinkSsapServerTest, NearlinkSsapServerTest004_CancelConnection, TestSize.Level1)
{
    HILOGI("NearlinkSsapServerTest004 start");

    NearlinkRemoteDevice device("11:22:33:44:55:66", ADAPTER_SLE);
    NlErrCode ret = ssapServer_->CancelConnection(device);
    EXPECT_EQ(NL_ERR_INTERNAL_ERROR, ret);
    HILOGI("NearlinkSsapServerTest004 end");
}

/**
 * @tc.name: NearlinkSsapServerTest005
 * @tc.desc: Test AuthorizeResponse
 * @tc.type: FUNC
 */
HWTEST_F(NearlinkSsapServerTest, NearlinkSsapServerTest005_AuthorizeResponse, TestSize.Level1)
{
    HILOGI("NearlinkSsapServerTest005 start");

    NlErrCode ret = ssapServer_->AuthorizeResponse(0, true);
    EXPECT_EQ(NL_NO_ERROR, ret);
    HILOGI("NearlinkSsapServerTest005 end");
}

/**
 * @tc.name: NearlinkSsapServerTest006
 * @tc.desc: Test Close
 * @tc.type: FUNC
 */
HWTEST_F(NearlinkSsapServerTest, NearlinkSsapServerTest006_Close, TestSize.Level1)
{
    HILOGI("NearlinkSsapServerTest006 start");

    NlErrCode ret = ssapServer_->Close();
    EXPECT_EQ(NL_NO_ERROR, ret);
    HILOGI("NearlinkSsapServerTest006 end");
}

/**
 * @tc.name: NearlinkSsapServerTest007
 * @tc.desc: Test OnConnectionStateChanged
 * @tc.type: FUNC
 */
HWTEST_F(NearlinkSsapServerTest, NearlinkSsapServerTest007_OnConnectionStateChanged, TestSize.Level1)
{
    HILOGI("NearlinkSsapServerTest007 start");

    serverCallbackImpl_ = std::make_shared<SsapServer::impl::NearlinkSsapServerCallbackStubImpl>(ssapServer_);
    EXPECT_TRUE(serverCallbackImpl_ != nullptr);
    std::shared_ptr<SsapServer> servPtr = serverCallbackImpl_->GetServerSptr();
    EXPECT_NE(servPtr, nullptr);
    NearlinkSsapDevice device;
    uint8_t state = 1; // SleConnectState::CONNECTED
    int reason = 0;
    serverCallbackImpl_->OnConnectionStateChanged(device, state, reason);
    state = 3; // SleConnectState::DISCONNECTED
    serverCallbackImpl_->OnConnectionStateChanged(device, state, reason);
    NearlinkSsapPropertyParcel property;
    serverCallbackImpl_->OnPropertyReadRequest(device, property, 0);
    serverCallbackImpl_->OnPropertyWriteRequest(device, property, 0);
    NearlinkSsapDescriptorParcel descriptor;
    serverCallbackImpl_->OnDescriptorReadRequest(device, descriptor, 0);
    serverCallbackImpl_->OnDescriptorWriteRequest(device, descriptor, 0);
    uint16_t mtu = 512;
    serverCallbackImpl_->OnMtuChanged(device, mtu);
    Uuid uuid =  Uuid::ConvertFromString("11223344-0000-1000-8000-00805F9B34FB");
    uint16_t handle = 1;
    int result = 0;
    serverCallbackImpl_->OnNotifyPropertyChanged(device, uuid, handle, result);
    serverCallbackImpl_->OnNotifyEventChanged(device, uuid, handle, result);
    HILOGI("NearlinkSsapServerTest007 end");
}

} // namespace TEST
} // namespace Nearlink
} // namespace OHOS
