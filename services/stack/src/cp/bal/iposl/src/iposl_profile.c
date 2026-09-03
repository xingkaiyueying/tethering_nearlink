/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */
#include "iposl_internal.h"

#include <string.h>

#include "dtap.h"
#include "nlstk_log.h"

static IposlProfileCallbacks g_callbacks;
static bool g_initialized;

const IposlProfileCallbacks *IposlGetCallbacks(void)
{
    return g_initialized ? &g_callbacks : NULL;
}

int32_t IposlProfileInit(const IposlProfileCallbacks *callbacks)
{
    if (callbacks == NULL || callbacks->onPeerSupported == NULL || callbacks->onConfigured == NULL) {
        NLSTK_LOG_ERROR("[IpShare][IPoSL] profile init rejected: callback incomplete");
        return IPOSL_ERR_INVALID_PARAM;
    }
    NLSTK_LOG_INFO("[IpShare][IPoSL] profile init started");
    if (!IposlCodecVerifyGoldenVectors()) {
        NLSTK_LOG_ERROR("[IpShare][IPoSL] profile init failed: codec golden vectors");
        return IPOSL_ERR_INVALID_STATE;
    }
    g_callbacks = *callbacks;
    g_initialized = true;
    int32_t ret = IposlServerInitialize();
    if (ret != IPOSL_SUCCESS) {
        (void)memset(&g_callbacks, 0, sizeof(g_callbacks));
        g_initialized = false;
        NLSTK_LOG_ERROR("[IpShare][IPoSL] profile init failed: server init ret=%d", ret);
        return IPOSL_ERR_SSAP;
    }
    NLSTK_LOG_INFO("[IpShare][IPoSL] profile init completed pi=%u", DTAP_PI_IPV4);
    return IPOSL_SUCCESS;
}

void IposlProfileDeinit(void)
{
    NLSTK_LOG_INFO("[IpShare][IPoSL] profile deinit started");
    IposlClientStop();
    IposlServerDeinit();
    (void)memset(&g_callbacks, 0, sizeof(g_callbacks));
    g_initialized = false;
    NLSTK_LOG_INFO("[IpShare][IPoSL] profile deinit completed");
}

int32_t IposlProfileStartServer(const uint8_t peer[IPOSL_LAYER2_ID_LEN], uint8_t addressType)
{
    if (!g_initialized) {
        NLSTK_LOG_ERROR("[IpShare][IPoSL] server start rejected: profile not initialized");
        return IPOSL_ERR_INVALID_STATE;
    }
    int32_t ret = IposlServerStart(peer, addressType);
    if (ret != IPOSL_SUCCESS) {
        NLSTK_LOG_ERROR("[IpShare][IPoSL] server start failed addressType=%u ret=%d", addressType, ret);
    } else {
        NLSTK_LOG_INFO("[IpShare][IPoSL] server start completed addressType=%u", addressType);
    }
    return ret;
}

void IposlProfileStopServer(void)
{
    IposlServerStop();
    NLSTK_LOG_INFO("[IpShare][IPoSL] server stopped");
}

int32_t IposlProfileProbePeer(const uint8_t peer[IPOSL_LAYER2_ID_LEN], uint8_t addressType)
{
    if (!g_initialized) {
        NLSTK_LOG_ERROR("[IpShare][IPoSL] support probe rejected: profile not initialized");
        return IPOSL_ERR_INVALID_STATE;
    }
    int32_t ret = IposlClientStart(peer, addressType, false, NULL);
    if (ret != IPOSL_SUCCESS) {
        NLSTK_LOG_ERROR("[IpShare][IPoSL] support probe client start failed addressType=%u ret=%d", addressType, ret);
    } else {
        NLSTK_LOG_INFO("[IpShare][IPoSL] support probe client start completed addressType=%u", addressType);
    }
    return ret;
}

int32_t IposlProfileStartTerminal(const uint8_t gateway[IPOSL_LAYER2_ID_LEN], uint8_t addressType,
    const uint8_t localLayer2[IPOSL_LAYER2_ID_LEN])
{
    if (!g_initialized) {
        NLSTK_LOG_ERROR("[IpShare][IPoSL] terminal start rejected: profile not initialized");
        return IPOSL_ERR_INVALID_STATE;
    }
    int32_t ret = IposlClientStart(gateway, addressType, true, localLayer2);
    if (ret != IPOSL_SUCCESS) {
        NLSTK_LOG_ERROR("[IpShare][IPoSL] terminal client start failed addressType=%u ret=%d", addressType, ret);
    } else {
        NLSTK_LOG_INFO("[IpShare][IPoSL] terminal client start completed addressType=%u", addressType);
    }
    return ret;
}

void IposlProfileStopClient(void)
{
    IposlClientStop();
    NLSTK_LOG_INFO("[IpShare][IPoSL] client stopped");
}

uint16_t IposlProfileIdentityServiceMemberCount(void)
{
    return 0;
}

uint8_t IposlProfileDataProtocolIndicator(void)
{
    return DTAP_PI_IPV4;
}
