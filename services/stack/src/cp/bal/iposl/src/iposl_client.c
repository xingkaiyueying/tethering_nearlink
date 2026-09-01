/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */
#include "iposl_internal.h"

#include <string.h>

#include "iposl_codec.h"
#include "nlstk_ssap_app_client.h"
#include "nlstk_ssap_app_link.h"
#include "nlstk_log.h"

static const uint8_t g_identifierUuid[16] = {
    0x8F, 0x6F, 0x1D, 0x00, 0x7B, 0x0C, 0x4A, 0x73, 0x9D, 0x4E, 0x6E, 0x65, 0x61, 0x72, 0x6C, 0x01
};
static const uint8_t g_configUuid[16] = {
    0x5B, 0x28, 0x50, 0x05, 0x45, 0x55, 0x4F, 0xA5, 0xB9, 0x57, 0xC2, 0x9C, 0x0D, 0x8A, 0x60, 0xC3
};
static const uint8_t g_methodUuid[16] = {
    0x7A, 0xA3, 0x12, 0x0E, 0xF0, 0xD2, 0x45, 0x60, 0xB7, 0x11, 0xA5, 0xB6, 0x18, 0xB7, 0xA3, 0x2B
};

static int32_t g_clientAppId = SSAP_APP_INVALID_ID;
static bool g_terminal;
static bool g_finishing;
static uint8_t g_peer[IPOSL_LAYER2_ID_LEN];
static uint8_t g_localLayer2[IPOSL_LAYER2_ID_LEN];
static uint16_t g_methodHandle;
static uint8_t g_expectedOpcode;

static void SetUuid(NLSTK_SsapUuid_S *uuid, const uint8_t value[16])
{
    (void)memcpy(uuid->uuid, value, sizeof(uuid->uuid));
}

static void NotifySupported(bool supported, int32_t error)
{
    const IposlProfileCallbacks *callbacks = IposlGetCallbacks();
    if (callbacks != NULL && callbacks->onPeerSupported != NULL) {
        NLSTK_LOG_INFO("[IpShare][IPoSL][Client] support callback supported=%d error=%d", supported, error);
        callbacks->onPeerSupported(g_peer, supported, error);
    } else {
        NLSTK_LOG_ERROR("[IpShare][IPoSL][Client] support callback dropped: callback unavailable");
    }
}

static void NotifyConfigured(bool opened, int32_t error)
{
    const IposlProfileCallbacks *callbacks = IposlGetCallbacks();
    if (callbacks != NULL && callbacks->onConfigured != NULL) {
        NLSTK_LOG_INFO("[IpShare][IPoSL][Client] configuration callback opened=%d error=%d", opened, error);
        callbacks->onConfigured(g_peer, opened, error);
    } else {
        NLSTK_LOG_ERROR("[IpShare][IPoSL][Client] configuration callback dropped: callback unavailable");
    }
}

static void Finish(bool keepClient)
{
    if (keepClient || g_clientAppId == SSAP_APP_INVALID_ID || g_finishing) {
        return;
    }
    g_finishing = true;
    int32_t appId = g_clientAppId;
    NLSTK_Errcode_E disconnectRet = NLSTK_SsapClientDisconnect(appId);
    if (disconnectRet != NLSTK_ERRCODE_SUCCESS) {
        NLSTK_LOG_ERROR("[IpShare][IPoSL][Client] disconnect failed appId=%d ret=%d", appId, disconnectRet);
    }
    NLSTK_SsapClientDeregApp(appId);
    NLSTK_LOG_INFO("[IpShare][IPoSL][Client] client deregistered appId=%d", appId);
    g_clientAppId = SSAP_APP_INVALID_ID;
    g_finishing = false;
}

static void FinishDisconnected(void)
{
    if (g_clientAppId == SSAP_APP_INVALID_ID || g_finishing) {
        return;
    }
    g_finishing = true;
    int32_t appId = g_clientAppId;
    NLSTK_SsapClientDeregApp(appId);
    NLSTK_LOG_INFO("[IpShare][IPoSL][Client] disconnected client deregistered appId=%d", appId);
    g_clientAppId = SSAP_APP_INVALID_ID;
    g_finishing = false;
}

static int32_t SendRequest(uint8_t opcode)
{
    uint8_t data[IPOSL_CONFIG_REQUEST_LEN] = {0};
    int32_t len = opcode == IPOSL_OPCODE_CONFIGURE ?
        IposlCodecEncodeConfigRequest(g_localLayer2, data, sizeof(data)) :
        IposlCodecEncodeOpenRequest(g_localLayer2, data, sizeof(data));
    if (len <= 0) {
        NLSTK_LOG_ERROR("[IpShare][IPoSL][Client] request encode failed opcode=%u ret=%d", opcode, len);
        return IPOSL_ERR_INVALID_PARAM;
    }
    NLSTK_VariableData_S value = {.len = (uint16_t)len, .data = data};
    g_expectedOpcode = opcode;
    NLSTK_Errcode_E ret = NLSTK_SsapClientCallMethod(g_clientAppId, g_methodHandle, &value, false);
    if (ret != NLSTK_ERRCODE_SUCCESS) {
        NLSTK_LOG_ERROR("[IpShare][IPoSL][Client] method request failed appId=%d handle=%u opcode=%u ret=%d",
            g_clientAppId, g_methodHandle, opcode, ret);
        return IPOSL_ERR_SSAP;
    }
    NLSTK_LOG_INFO("[IpShare][IPoSL][Client] method request submitted appId=%d handle=%u opcode=%u",
        g_clientAppId, g_methodHandle, opcode);
    return IPOSL_SUCCESS;
}

static void OnCallMethod(int32_t appId, NLSTK_SsapClientCallMethodResult_S *response, NLSTK_Errcode_E ret)
{
    uint8_t layer2[IPOSL_LAYER2_ID_LEN] = {0};
    uint8_t result = 0xFF;
    int32_t responseError = response == NULL ? IPOSL_ERR_INVALID_PARAM : response->errorCode;
    int32_t decodeRet = response == NULL ? IPOSL_ERR_INVALID_PARAM :
        IposlCodecDecodeResponse(response->value.data, response->value.len, g_expectedOpcode, layer2, &result);
    if (appId != g_clientAppId || response == NULL || ret != NLSTK_ERRCODE_SUCCESS ||
        responseError != NLSTK_ERRCODE_SUCCESS || decodeRet != IPOSL_SUCCESS ||
        memcmp(layer2, g_localLayer2, sizeof(layer2)) != 0 || result != 0) {
        NLSTK_LOG_ERROR("[IpShare][IPoSL][Client] method response rejected appId=%d expectedAppId=%d ret=%d "
            "responseError=%d decodeRet=%d opcode=%u result=%u", appId, g_clientAppId, ret, responseError,
            decodeRet, g_expectedOpcode, result);
        NotifyConfigured(false, IPOSL_ERR_SSAP);
        Finish(false);
        return;
    }
    if (g_expectedOpcode == IPOSL_OPCODE_CONFIGURE) {
        NLSTK_LOG_INFO("[IpShare][IPoSL][Client] configuration response accepted; sending enable");
        NotifyConfigured(false, IPOSL_SUCCESS);
        if (SendRequest(IPOSL_OPCODE_ENABLE) != IPOSL_SUCCESS) {
            NotifyConfigured(false, IPOSL_ERR_SSAP);
            Finish(false);
        }
        return;
    }
    NLSTK_LOG_INFO("[IpShare][IPoSL][Client] enable response accepted");
    NotifyConfigured(true, IPOSL_SUCCESS);
}

static void OnFindServiceByUuid(int32_t appId, NLSTK_SsapUuid_S *uuid, NLSTK_Errcode_E ret)
{
    NLSTK_SsapServ_S *services = NULL;
    uint16_t serviceNum = 0;
    NLSTK_SsapClientFreeFunc freeFunc = NULL;
    if (appId != g_clientAppId || uuid == NULL || ret != NLSTK_ERRCODE_SUCCESS ||
        NLSTK_SsapClientGetServicesByUuid(appId, uuid, &services, &serviceNum, &freeFunc) != NLSTK_ERRCODE_SUCCESS ||
        services == NULL || serviceNum == 0) {
        NLSTK_LOG_ERROR("[IpShare][IPoSL][Client] service discovery failed appId=%d expectedAppId=%d terminal=%d "
            "ret=%d", appId, g_clientAppId, g_terminal, ret);
        if (g_terminal) {
            NotifyConfigured(false, IPOSL_ERR_NOT_SUPPORTED);
        } else {
            NotifySupported(false, IPOSL_SUCCESS);
        }
        Finish(false);
        return;
    }
    if (!g_terminal) {
        if (freeFunc != NULL) {
            freeFunc(services, serviceNum);
        }
        NotifySupported(true, IPOSL_SUCCESS);
        NLSTK_LOG_INFO("[IpShare][IPoSL][Client] support service discovered count=%u", serviceNum);
        Finish(false);
        return;
    }
    g_methodHandle = 0;
    for (uint16_t i = 0; i < serviceNum && g_methodHandle == 0; ++i) {
        for (uint16_t j = 0; j < services[i].methodNum; ++j) {
            if (memcmp(services[i].methods[j].uuid.uuid, g_methodUuid, sizeof(g_methodUuid)) == 0) {
                g_methodHandle = services[i].methods[j].handle;
                break;
            }
        }
    }
    if (freeFunc != NULL) {
        freeFunc(services, serviceNum);
    }
    if (g_methodHandle == 0 || SendRequest(IPOSL_OPCODE_CONFIGURE) != IPOSL_SUCCESS) {
        NLSTK_LOG_ERROR("[IpShare][IPoSL][Client] terminal configuration unavailable methodHandle=%u", g_methodHandle);
        NotifyConfigured(false, IPOSL_ERR_NOT_SUPPORTED);
        Finish(false);
    } else {
        NLSTK_LOG_INFO("[IpShare][IPoSL][Client] terminal configuration method discovered handle=%u", g_methodHandle);
    }
}

static void OnConnectionStateChanged(int32_t appId, uint8_t state, NLSTK_Errcode_E ret, int32_t reason)
{
    if (appId != g_clientAppId || g_finishing) {
        NLSTK_LOG_WARN("[IpShare][IPoSL][Client] connection callback ignored appId=%d expectedAppId=%d finishing=%d",
            appId, g_clientAppId, g_finishing);
        return;
    }
    NLSTK_LOG_INFO("[IpShare][IPoSL][Client] connection callback appId=%d state=%u ret=%d reason=%d terminal=%d",
        appId, state, ret, reason, g_terminal);
    if (state == SSAP_CONNECT_STATE_CONNECTED && ret == NLSTK_ERRCODE_SUCCESS) {
        NLSTK_SsapUuid_S uuid = {0};
        SetUuid(&uuid, g_terminal ? g_configUuid : g_identifierUuid);
        NLSTK_Errcode_E discoverRet = NLSTK_SsapClientDiscoverServicesByUuid(appId, &uuid, SSAP_START_HANDLE,
            SSAP_END_HANDLE, ITEM_TYPE_VENDOR_PRIMARY_SERVICE);
        if (discoverRet != NLSTK_ERRCODE_SUCCESS) {
            NLSTK_LOG_ERROR("[IpShare][IPoSL][Client] service discovery request failed appId=%d ret=%d", appId,
                discoverRet);
            if (g_terminal) {
                NotifyConfigured(false, IPOSL_ERR_SSAP);
            } else {
                NotifySupported(false, IPOSL_ERR_SSAP);
            }
            Finish(false);
        } else {
            NLSTK_LOG_INFO("[IpShare][IPoSL][Client] service discovery request submitted appId=%d", appId);
        }
    } else if (state == SSAP_CONNECT_STATE_DISCONNECTED) {
        NLSTK_LOG_ERROR("[IpShare][IPoSL][Client] connection disconnected appId=%d ret=%d reason=%d", appId, ret,
            reason);
        if (g_terminal) {
            NotifyConfigured(false, IPOSL_ERR_SSAP);
        } else {
            NotifySupported(false, IPOSL_ERR_SSAP);
        }
        FinishDisconnected();
    }
}

int32_t IposlClientStart(const uint8_t peer[IPOSL_LAYER2_ID_LEN], uint8_t addressType, bool terminal,
    const uint8_t localLayer2[IPOSL_LAYER2_ID_LEN])
{
    if (peer == NULL || (terminal && localLayer2 == NULL) || g_clientAppId != SSAP_APP_INVALID_ID) {
        NLSTK_LOG_ERROR("[IpShare][IPoSL][Client] start rejected peerNull=%d localNull=%d activeAppId=%d", peer == NULL,
            terminal && localLayer2 == NULL, g_clientAppId);
        return IPOSL_ERR_INVALID_STATE;
    }
    SLE_Addr_S addr = {0};
    (void)memcpy(addr.addr, peer, IPOSL_LAYER2_ID_LEN);
    addr.type = addressType;
    NLSTK_SsapAppClientCb_S callbacks = {0};
    callbacks.onConnectionStateChanged = OnConnectionStateChanged;
    callbacks.onFindServiceByUuid = OnFindServiceByUuid;
    callbacks.onCallMethod = OnCallMethod;
    NLSTK_Errcode_E registerRet = NLSTK_SsapClientRegApp(&g_clientAppId, &callbacks, &addr);
    if (registerRet != NLSTK_ERRCODE_SUCCESS || g_clientAppId == SSAP_APP_INVALID_ID) {
        g_clientAppId = SSAP_APP_INVALID_ID;
        NLSTK_LOG_ERROR("[IpShare][IPoSL][Client] register failed terminal=%d addressType=%u ret=%d", terminal,
            addressType, registerRet);
        return IPOSL_ERR_SSAP;
    }
    (void)memcpy(g_peer, peer, sizeof(g_peer));
    if (terminal) {
        (void)memcpy(g_localLayer2, localLayer2, sizeof(g_localLayer2));
    }
    g_terminal = terminal;
    g_methodHandle = 0;
    g_expectedOpcode = 0;
    NLSTK_LOG_INFO("[IpShare][IPoSL][Client] registered appId=%d terminal=%d addressType=%u", g_clientAppId,
        terminal, addressType);
    NLSTK_Errcode_E connectRet = NLSTK_SsapClientConnect(g_clientAppId);
    if (connectRet != NLSTK_ERRCODE_SUCCESS) {
        NLSTK_LOG_ERROR("[IpShare][IPoSL][Client] connect request failed appId=%d ret=%d", g_clientAppId, connectRet);
        Finish(false);
        return IPOSL_ERR_SSAP;
    }
    NLSTK_LOG_INFO("[IpShare][IPoSL][Client] connect request submitted appId=%d", g_clientAppId);
    return IPOSL_SUCCESS;
}

void IposlClientStop(void)
{
    NLSTK_LOG_INFO("[IpShare][IPoSL][Client] stop requested appId=%d", g_clientAppId);
    Finish(false);
    (void)memset(g_peer, 0, sizeof(g_peer));
    (void)memset(g_localLayer2, 0, sizeof(g_localLayer2));
    g_terminal = false;
    g_methodHandle = 0;
    g_expectedOpcode = 0;
    NLSTK_LOG_INFO("[IpShare][IPoSL][Client] stop completed");
}
