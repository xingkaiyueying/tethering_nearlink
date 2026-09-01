/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */
#include "iposl_internal.h"

#include <string.h>

#include "iposl_codec.h"
#include "nlstk_ssap_app_server.h"

static const uint8_t g_identifierUuid[16] = {
    0x8F, 0x6F, 0x1D, 0x00, 0x7B, 0x0C, 0x4A, 0x73, 0x9D, 0x4E, 0x6E, 0x65, 0x61, 0x72, 0x6C, 0x01
};
static const uint8_t g_configUuid[16] = {
    0x5B, 0x28, 0x50, 0x05, 0x45, 0x55, 0x4F, 0xA5, 0xB9, 0x57, 0xC2, 0x9C, 0x0D, 0x8A, 0x60, 0xC3
};
static const uint8_t g_terminalCapabilityUuid[16] = {
    0xFA, 0x36, 0x1B, 0x52, 0xCB, 0x0A, 0x49, 0xED, 0x84, 0x27, 0x1A, 0x67, 0x5A, 0xCC, 0xD9, 0xDB
};
static const uint8_t g_terminalStateUuid[16] = {
    0xE4, 0x72, 0x0A, 0xB0, 0xE6, 0x2A, 0x42, 0xD2, 0xAD, 0x96, 0x98, 0x62, 0x7C, 0xEE, 0xAE, 0xDB
};
static const uint8_t g_gatewayCapabilityUuid[16] = {
    0x2E, 0x68, 0xC5, 0x66, 0xE3, 0x14, 0x46, 0x6D, 0x96, 0x29, 0x40, 0x2F, 0x16, 0xCA, 0xB2, 0x19
};
static const uint8_t g_gatewayStateUuid[16] = {
    0x43, 0x11, 0x7F, 0xB2, 0x90, 0x9E, 0x4B, 0x70, 0xB6, 0xF9, 0x10, 0x52, 0xD7, 0x29, 0xC8, 0x9B
};
static const uint8_t g_methodUuid[16] = {
    0x7A, 0xA3, 0x12, 0x0E, 0xF0, 0xD2, 0x45, 0x60, 0xB7, 0x11, 0xA5, 0xB6, 0x18, 0xB7, 0xA3, 0x2B
};

static int32_t g_serverAppId = SSAP_APP_INVALID_ID;
static uint8_t g_expectedPeer[IPOSL_LAYER2_ID_LEN];
static uint8_t g_expectedAddressType;
static uint8_t g_configuredLayer2[IPOSL_LAYER2_ID_LEN];
static bool g_configured;
static bool g_serverActive;

static void SetUuid(NLSTK_SsapUuid_S *uuid, const uint8_t value[16])
{
    (void)memcpy(uuid->uuid, value, sizeof(uuid->uuid));
}

static bool IsExpectedPeer(const SLE_Addr_S *addr)
{
    return addr != NULL && addr->type == g_expectedAddressType &&
        memcmp(addr->addr, g_expectedPeer, IPOSL_LAYER2_ID_LEN) == 0;
}

static void FillProperty(NLSTK_SsapServicePropertyParam_S *property, const uint8_t uuid[16],
    const uint8_t *value, uint16_t valueLen, uint32_t operation)
{
    property->type = ITEM_TYPE_VENDOR_PROPERTY;
    SetUuid(&property->uuid, uuid);
    property->permission.permissionValue = SSAP_PERMISSION_AUTHENTICATION_NEED | SSAP_PERMISSION_ENCRYPTION_NEED;
    property->operation.operationValue = operation;
    property->val.data = (uint8_t *)value;
    property->val.len = valueLen;
}

static int32_t AddIdentifierService(void)
{
    NLSTK_ServiceParam_S service = {0};
    SetUuid(&service.serviceStatement.uuid, g_identifierUuid);
    service.serviceStatement.serviceType = ITEM_TYPE_VENDOR_PRIMARY_SERVICE;
    return NLSTK_SsapServerAddService(g_serverAppId, &service) == NLSTK_ERRCODE_SUCCESS ?
        IPOSL_SUCCESS : IPOSL_ERR_SSAP;
}

static int32_t AddConfigService(void)
{
    NLSTK_SsapServicePropertyParam_S properties[4] = {0};
    FillProperty(&properties[0], g_terminalCapabilityUuid, g_iposlTerminalCapability,
        IPOSL_TERMINAL_CAPABILITY_LEN, SSAP_OPERATE_INDICATION_READ);
    FillProperty(&properties[1], g_terminalStateUuid, g_iposlEmptyState,
        IPOSL_EMPTY_STATE_LEN, SSAP_OPERATE_INDICATION_READ | SSAP_OPERATE_INDICATION_NOTIFY);
    FillProperty(&properties[2], g_gatewayCapabilityUuid, g_iposlGatewayCapability,
        IPOSL_GATEWAY_CAPABILITY_LEN, SSAP_OPERATE_INDICATION_READ);
    FillProperty(&properties[3], g_gatewayStateUuid, g_iposlGatewayServing,
        IPOSL_GATEWAY_CAPABILITY_LEN, SSAP_OPERATE_INDICATION_READ | SSAP_OPERATE_INDICATION_NOTIFY);

    NLSTK_SsapServiceMethodParam_S method = {0};
    method.type = ITEM_TYPE_VENDOR_METHOD;
    SetUuid(&method.uuid, g_methodUuid);
    method.permission.permissionValue = SSAP_PERMISSION_AUTHENTICATION_NEED | SSAP_PERMISSION_ENCRYPTION_NEED;

    NLSTK_ServiceParam_S service = {0};
    SetUuid(&service.serviceStatement.uuid, g_configUuid);
    service.serviceStatement.serviceType = ITEM_TYPE_VENDOR_PRIMARY_SERVICE;
    service.servicePropertyNum = 4;
    service.property = properties;
    service.serviceMethodNum = 1;
    service.method = &method;
    return NLSTK_SsapServerAddService(g_serverAppId, &service) == NLSTK_ERRCODE_SUCCESS ?
        IPOSL_SUCCESS : IPOSL_ERR_SSAP;
}

static void OnCallMethod(int32_t appId, uint16_t requestId, NLSTK_SsapServerCallMethodRequestInfo_S *method,
    bool needReturn, bool needAuth)
{
    uint8_t opcode = 0;
    uint8_t layer2[IPOSL_LAYER2_ID_LEN] = {0};
    uint8_t response[IPOSL_RESPONSE_LEN] = {0};
    uint8_t result = 0xFF;
    bool accepted = g_serverActive && appId == g_serverAppId && method != NULL && IsExpectedPeer(&method->addr) &&
        memcmp(method->uuid.uuid, g_methodUuid, sizeof(g_methodUuid)) == 0 &&
        IposlCodecDecodeRequest(method->param.data, method->param.len, &opcode, layer2) == IPOSL_SUCCESS;
    if (accepted && opcode == IPOSL_OPCODE_CONFIGURE &&
        memcmp(layer2, g_expectedPeer, sizeof(g_expectedPeer)) == 0) {
        (void)memcpy(g_configuredLayer2, layer2, sizeof(g_configuredLayer2));
        g_configured = true;
        result = 0;
    } else if (accepted && opcode == IPOSL_OPCODE_ENABLE && g_configured &&
        memcmp(g_configuredLayer2, layer2, sizeof(g_configuredLayer2)) == 0) {
        result = 0;
    }
    if (method != NULL && IposlCodecEncodeResponse(opcode, layer2, result, response, sizeof(response)) > 0 &&
        needReturn) {
        NLSTK_VariableData_S value = {.len = sizeof(response), .data = response};
        (void)NLSTK_SsapServerSendMethodCallRes(g_serverAppId, requestId, &value);
    } else if (needAuth) {
        (void)NLSTK_SsapServerAuthorizeResult(g_serverAppId, requestId, result == 0);
    }
    if (result == 0 && opcode == IPOSL_OPCODE_CONFIGURE) {
        const IposlProfileCallbacks *callbacks = IposlGetCallbacks();
        if (callbacks != NULL && callbacks->onConfigured != NULL) {
            callbacks->onConfigured(g_expectedPeer, false, IPOSL_SUCCESS);
        }
    }
    if (result == 0 && opcode == IPOSL_OPCODE_ENABLE) {
        const IposlProfileCallbacks *callbacks = IposlGetCallbacks();
        if (callbacks != NULL && callbacks->onConfigured != NULL) {
            callbacks->onConfigured(g_expectedPeer, true, IPOSL_SUCCESS);
        }
    }
}

int32_t IposlServerInitialize(void)
{
    if (g_serverAppId != SSAP_APP_INVALID_ID) {
        return IPOSL_SUCCESS;
    }
    NLSTK_SsapAppServerCb_S callbacks = {0};
    callbacks.onCallMethod = OnCallMethod;
    if (NLSTK_SsapServerRegApp(&callbacks, &g_serverAppId) != NLSTK_ERRCODE_SUCCESS ||
        g_serverAppId == SSAP_APP_INVALID_ID) {
        g_serverAppId = SSAP_APP_INVALID_ID;
        return IPOSL_ERR_SSAP;
    }
    if (AddIdentifierService() != IPOSL_SUCCESS || AddConfigService() != IPOSL_SUCCESS) {
        IposlServerDeinit();
        return IPOSL_ERR_SSAP;
    }
    return IPOSL_SUCCESS;
}

int32_t IposlServerStart(const uint8_t peer[IPOSL_LAYER2_ID_LEN], uint8_t addressType)
{
    if (peer == NULL || g_serverAppId == SSAP_APP_INVALID_ID || g_serverActive) {
        return IPOSL_ERR_INVALID_STATE;
    }
    (void)memcpy(g_expectedPeer, peer, sizeof(g_expectedPeer));
    g_expectedAddressType = addressType;
    g_configured = false;
    g_serverActive = true;
    return IPOSL_SUCCESS;
}

void IposlServerStop(void)
{
    (void)memset(g_expectedPeer, 0, sizeof(g_expectedPeer));
    (void)memset(g_configuredLayer2, 0, sizeof(g_configuredLayer2));
    g_expectedAddressType = 0;
    g_configured = false;
    g_serverActive = false;
}

void IposlServerDeinit(void)
{
    IposlServerStop();
    if (g_serverAppId != SSAP_APP_INVALID_ID) {
        (void)NLSTK_SsapServerClearServices(g_serverAppId);
        NLSTK_SsapServerDeregisterApplication(g_serverAppId);
    }
    g_serverAppId = SSAP_APP_INVALID_ID;
}
