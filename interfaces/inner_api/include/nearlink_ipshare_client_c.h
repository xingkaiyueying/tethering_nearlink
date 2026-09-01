/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */
#ifndef NEARLINK_IPSHARE_CLIENT_C_H
#define NEARLINK_IPSHARE_CLIENT_C_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NL_IPSHARE_ADDRESS_TEXT_LEN 18
#define NL_IPSHARE_IFACE_TEXT_LEN 16
#define NL_IPSHARE_IPV4_TEXT_LEN 16
#define NL_IPSHARE_ERROR_STAGE_LEN 32

typedef struct NlIpShareStatusC {
    int32_t role;
    int32_t state;
    char peerAddress[NL_IPSHARE_ADDRESS_TEXT_LEN];
    char ifaceName[NL_IPSHARE_IFACE_TEXT_LEN];
    char ipv4Address[NL_IPSHARE_IPV4_TEXT_LEN];
    int32_t hasUpstream;
    char errorStage[NL_IPSHARE_ERROR_STAGE_LEN];
    int32_t errorCode;
} NlIpShareStatusC;

int32_t NlIpShareIsPeerSupported(const char *peerAddress, int32_t *supported);
int32_t NlIpShareStartGateway(const char *peerAddress);
int32_t NlIpShareStartTerminal(const char *gatewayAddress);
int32_t NlIpShareStop(void);
int32_t NlIpShareGetStatus(NlIpShareStatusC *status);

#ifdef __cplusplus
}
#endif
#endif  // NEARLINK_IPSHARE_CLIENT_C_H
