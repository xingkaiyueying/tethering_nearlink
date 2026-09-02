/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */
#include "iposl_internal.h"

#include <string.h>

#include "iposl_codec.h"
#include "nlstk_log.h"
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
    /* Demo UUIDs are private 128-bit UUIDs, so their SSAP item types must stay vendor-specific. */
    property->type = ITEM_TYPE_VENDOR_PROPERTY;
    SetUuid(&property->uuid, uuid);
    property->permission.permissionValue = SSAP_PERMISSION_AUTHENTICATION_NEED | SSAP_PERMISSION_ENCRYPTION_NEED;
    property->operation.operationValue = operation;
    property->val.data = (uint8_t *)value;
    property->val.len = valueLen;
}

static int32_t AddIdentifierService(void)
{
    /* The SSAP allocator requires a non-null property array even when the service has no properties. */
    NLSTK_SsapServicePropertyParam_S emptyProperties = {0};
    NLSTK_ServiceParam_S service = {0};
    SetUuid(&service.serviceStatement.uuid, g_identifierUuid);
    service.serviceStatement.serviceType = ITEM_TYPE_VENDOR_PRIMARY_SERVICE;
    service.property = &emptyProperties;
    service.servicePropertyNum = 0;
    NLSTK_Errcode_E ret = NLSTK_SsapServerAddService(g_serverAppId, &service);
    if (ret != NLSTK_ERRCODE_SUCCESS) {
        NLSTK_LOG_ERROR("[IpShare][IPoSL][Server] identifier service add failed appId=%d ret=%d", g_serverAppId, ret);
        return IPOSL_ERR_SSAP;
    }
    NLSTK_LOG_INFO("[IpShare][IPoSL][Server] identifier service added appId=%d", g_serverAppId);
    return IPOSL_SUCCESS;
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
    NLSTK_Errcode_E ret = NLSTK_SsapServerAddService(g_serverAppId, &service);
    if (ret != NLSTK_ERRCODE_SUCCESS) {
        NLSTK_LOG_ERROR("[IpShare][IPoSL][Server] configuration service add failed appId=%d ret=%d", g_serverAppId,
            ret);
        return IPOSL_ERR_SSAP;
    }
    NLSTK_LOG_INFO("[IpShare][IPoSL][Server] configuration service added appId=%d", g_serverAppId);
    return IPOSL_SUCCESS;
}

static void OnCallMethod(int32_t appId, uint16_t requestId, NLSTK_SsapServerCallMethodRequestInfo_S *method,
    bool needReturn, bool needAuth)
{
    uint8_t opcode = 0;
    uint8_t layer2[IPOSL_LAYER2_ID_LEN] = {0};
    uint8_t response[IPOSL_RESPONSE_LEN] = {0};
    uint8_t result = 0xFF;
    bool appMatch = appId == g_serverAppId;
    bool peerMatch = method != NULL && IsExpectedPeer(&method->addr);
    bool methodMatch = method != NULL && memcmp(method->uuid.uuid, g_methodUuid, sizeof(g_methodUuid)) == 0;
    int32_t decodeRet = method == NULL ? IPOSL_ERR_INVALID_PARAM :
        IposlCodecDecodeRequest(method->param.data, method->param.len, &opcode, layer2);
    bool accepted = g_serverActive && appMatch && method != NULL && peerMatch && methodMatch &&
        decodeRet == IPOSL_SUCCESS;
    if (!accepted) {
        NLSTK_LOG_ERROR("[IpShare][IPoSL][Server] method rejected requestId=%u appId=%d active=%d appMatch=%d "
            "method=%d peerMatch=%d methodMatch=%d decodeRet=%d", requestId, appId, g_serverActive, appMatch,
            method != NULL, peerMatch, methodMatch, decodeRet);
    } else {
        NLSTK_LOG_INFO("[IpShare][IPoSL][Server] method request accepted requestId=%u appId=%d opcode=%u", requestId,
            appId, opcode);
    }
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
        NLSTK_Errcode_E sendRet = NLSTK_SsapServerSendMethodCallRes(g_serverAppId, requestId, &value);
        if (sendRet != NLSTK_ERRCODE_SUCCESS) {
            NLSTK_LOG_ERROR("[IpShare][IPoSL][Server] method response send failed requestId=%u opcode=%u result=%u "
                "ret=%d", requestId, opcode, result, sendRet);
        } else {
            NLSTK_LOG_INFO("[IpShare][IPoSL][Server] method response sent requestId=%u opcode=%u result=%u",
                requestId, opcode, result);
        }
    } else if (needAuth) {
        NLSTK_Errcode_E authRet = NLSTK_SsapServerAuthorizeResult(g_serverAppId, requestId, result == 0);
        if (authRet != NLSTK_ERRCODE_SUCCESS) {
            NLSTK_LOG_ERROR("[IpShare][IPoSL][Server] authorization response failed requestId=%u ret=%d", requestId,
                authRet);
        } else {
            NLSTK_LOG_INFO("[IpShare][IPoSL][Server] authorization response sent requestId=%u allow=%d", requestId,
                result == 0);
        }
    } else {
        NLSTK_LOG_ERROR("[IpShare][IPoSL][Server] method response not sent requestId=%u needReturn=%d needAuth=%d",
            requestId, needReturn, needAuth);
    }
    if (result == 0 && opcode == IPOSL_OPCODE_CONFIGURE) {
        const IposlProfileCallbacks *callbacks = IposlGetCallbacks();
        if (callbacks != NULL && callbacks->onConfigured != NULL) {
            callbacks->onConfigured(g_expectedPeer, false, IPOSL_SUCCESS);
            NLSTK_LOG_INFO("[IpShare][IPoSL][Server] configuration accepted; callback sent");
        } else {
            NLSTK_LOG_ERROR("[IpShare][IPoSL][Server] configuration accepted but callback unavailable");
        }
    }
    if (result == 0 && opcode == IPOSL_OPCODE_ENABLE) {
        const IposlProfileCallbacks *callbacks = IposlGetCallbacks();
        if (callbacks != NULL && callbacks->onConfigured != NULL) {
            callbacks->onConfigured(g_expectedPeer, true, IPOSL_SUCCESS);
            NLSTK_LOG_INFO("[IpShare][IPoSL][Server] enable accepted; callback sent");
        } else {
            NLSTK_LOG_ERROR("[IpShare][IPoSL][Server] enable accepted but callback unavailable");
        }
    }
}

int32_t IposlServerInitialize(void)
{
    if (g_serverAppId != SSAP_APP_INVALID_ID) {
        NLSTK_LOG_INFO("[IpShare][IPoSL][Server] initialize skipped appId=%d", g_serverAppId);
        return IPOSL_SUCCESS;
    }
    NLSTK_SsapAppServerCb_S callbacks = {0};
    callbacks.onCallMethod = OnCallMethod;
    NLSTK_Errcode_E registerRet = NLSTK_SsapServerRegApp(&callbacks, &g_serverAppId);
    if (registerRet != NLSTK_ERRCODE_SUCCESS || g_serverAppId == SSAP_APP_INVALID_ID) {
        g_serverAppId = SSAP_APP_INVALID_ID;
        NLSTK_LOG_ERROR("[IpShare][IPoSL][Server] register failed ret=%d", registerRet);
        return IPOSL_ERR_SSAP;
    }
    NLSTK_LOG_INFO("[IpShare][IPoSL][Server] registered appId=%d", g_serverAppId);
    if (AddIdentifierService() != IPOSL_SUCCESS || AddConfigService() != IPOSL_SUCCESS) {
        NLSTK_LOG_ERROR("[IpShare][IPoSL][Server] initialize failed while adding services appId=%d", g_serverAppId);
        IposlServerDeinit();
        return IPOSL_ERR_SSAP;
    }
    NLSTK_LOG_INFO("[IpShare][IPoSL][Server] initialize completed appId=%d", g_serverAppId);
    return IPOSL_SUCCESS;
}

int32_t IposlServerStart(const uint8_t peer[IPOSL_LAYER2_ID_LEN], uint8_t addressType)
{
    if (peer == NULL || g_serverAppId == SSAP_APP_INVALID_ID || g_serverActive) {
        NLSTK_LOG_ERROR("[IpShare][IPoSL][Server] start rejected peerNull=%d appId=%d active=%d", peer == NULL,
            g_serverAppId, g_serverActive);
        return IPOSL_ERR_INVALID_STATE;
    }
    (void)memcpy(g_expectedPeer, peer, sizeof(g_expectedPeer));
    g_expectedAddressType = addressType;
    g_configured = false;
    g_serverActive = true;
    NLSTK_LOG_INFO("[IpShare][IPoSL][Server] start completed appId=%d addressType=%u", g_serverAppId, addressType);
    return IPOSL_SUCCESS;
}

void IposlServerStop(void)
{
    bool wasActive = g_serverActive;
    (void)memset(g_expectedPeer, 0, sizeof(g_expectedPeer));
    (void)memset(g_configuredLayer2, 0, sizeof(g_configuredLayer2));
    g_expectedAddressType = 0;
    g_configured = false;
    g_serverActive = false;
    NLSTK_LOG_INFO("[IpShare][IPoSL][Server] stop completed wasActive=%d", wasActive);
}

void IposlServerDeinit(void)
{
    NLSTK_LOG_INFO("[IpShare][IPoSL][Server] deinit started appId=%d", g_serverAppId);
    IposlServerStop();
    if (g_serverAppId != SSAP_APP_INVALID_ID) {
        NLSTK_Errcode_E clearRet = NLSTK_SsapServerClearServices(g_serverAppId);
        if (clearRet != NLSTK_ERRCODE_SUCCESS) {
            NLSTK_LOG_ERROR("[IpShare][IPoSL][Server] clear services failed appId=%d ret=%d", g_serverAppId,
                clearRet);
        }
        NLSTK_SsapServerDeregisterApplication(g_serverAppId);
        NLSTK_LOG_INFO("[IpShare][IPoSL][Server] deregistered appId=%d", g_serverAppId);
    }
    g_serverAppId = SSAP_APP_INVALID_ID;
    NLSTK_LOG_INFO("[IpShare][IPoSL][Server] deinit completed");
}
