/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */
#include "iposl_internal.h"

#include <string.h>

#include "dtap.h"

static IposlProfileCallbacks g_callbacks;
static bool g_initialized;

const IposlProfileCallbacks *IposlGetCallbacks(void)
{
    return g_initialized ? &g_callbacks : NULL;
}

int32_t IposlProfileInit(const IposlProfileCallbacks *callbacks)
{
    if (callbacks == NULL || callbacks->onPeerSupported == NULL || callbacks->onConfigured == NULL) {
        return IPOSL_ERR_INVALID_PARAM;
    }
    if (!IposlCodecVerifyGoldenVectors()) {
        return IPOSL_ERR_INVALID_STATE;
    }
    g_callbacks = *callbacks;
    g_initialized = true;
    if (IposlServerInitialize() != IPOSL_SUCCESS) {
        (void)memset(&g_callbacks, 0, sizeof(g_callbacks));
        g_initialized = false;
        return IPOSL_ERR_SSAP;
    }
    return IPOSL_SUCCESS;
}

void IposlProfileDeinit(void)
{
    IposlClientStop();
    IposlServerDeinit();
    (void)memset(&g_callbacks, 0, sizeof(g_callbacks));
    g_initialized = false;
}

int32_t IposlProfileStartServer(const uint8_t peer[IPOSL_LAYER2_ID_LEN], uint8_t addressType)
{
    if (!g_initialized) {
        return IPOSL_ERR_INVALID_STATE;
    }
    return IposlServerStart(peer, addressType);
}

void IposlProfileStopServer(void)
{
    IposlServerStop();
}

int32_t IposlProfileProbePeer(const uint8_t peer[IPOSL_LAYER2_ID_LEN], uint8_t addressType)
{
    if (!g_initialized) {
        return IPOSL_ERR_INVALID_STATE;
    }
    return IposlClientStart(peer, addressType, false, NULL);
}

int32_t IposlProfileStartTerminal(const uint8_t gateway[IPOSL_LAYER2_ID_LEN], uint8_t addressType,
    const uint8_t localLayer2[IPOSL_LAYER2_ID_LEN])
{
    if (!g_initialized) {
        return IPOSL_ERR_INVALID_STATE;
    }
    return IposlClientStart(gateway, addressType, true, localLayer2);
}

void IposlProfileStopClient(void)
{
    IposlClientStop();
}

uint16_t IposlProfileIdentityServiceMemberCount(void)
{
    return 0;
}

uint8_t IposlProfileDataProtocolIndicator(void)
{
    return DTAP_PI_IPV4;
}
