/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */
#ifndef IPOSL_PROFILE_H
#define IPOSL_PROFILE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IPOSL_IDENTIFIER_SERVICE_UUID "8f6f1d00-7b0c-4a73-9d4e-6e6561726c01"
#define IPOSL_CONFIG_SERVICE_UUID "5b285005-4555-4fa5-b957-c29c0d8a60c3"
#define IPOSL_TERMINAL_CAPABILITY_UUID "fa361b52-cb0a-49ed-8427-1a675accd9db"
#define IPOSL_TERMINAL_STATE_UUID "e4720ab0-e62a-42d2-ad96-98627ceeaedb"
#define IPOSL_GATEWAY_CAPABILITY_UUID "2e68c566-e314-466d-9629-402f16cab219"
#define IPOSL_GATEWAY_STATE_UUID "43117fb2-909e-4b70-b6f9-1052d729c89b"
#define IPOSL_NODE_CONFIG_METHOD_UUID "7aa3120e-f0d2-4560-b711-a5b618b7a32b"

#define IPOSL_IP_TYPE_IPV4 0x01u
#define IPOSL_OPCODE_CONFIGURE 0x01u
#define IPOSL_OPCODE_ENABLE 0x02u
#define IPOSL_MTU 1500u
#define IPOSL_LAYER2_ID_LEN 6u

enum {
    IPOSL_SUCCESS = 0,
    IPOSL_ERR_INVALID_PARAM = -1,
    IPOSL_ERR_INVALID_STATE = -2,
    IPOSL_ERR_SSAP = -3,
    IPOSL_ERR_NOT_SUPPORTED = -4,
};

typedef struct IposlProfileCallbacks {
    void (*onPeerSupported)(const uint8_t peer[IPOSL_LAYER2_ID_LEN], bool supported, int32_t error);
    void (*onConfigured)(const uint8_t peer[IPOSL_LAYER2_ID_LEN], bool opened, int32_t error);
} IposlProfileCallbacks;

int32_t IposlProfileInit(const IposlProfileCallbacks *callbacks);
void IposlProfileDeinit(void);
int32_t IposlProfileStartServer(const uint8_t peer[IPOSL_LAYER2_ID_LEN], uint8_t addressType);
void IposlProfileStopServer(void);
int32_t IposlProfileProbePeer(const uint8_t peer[IPOSL_LAYER2_ID_LEN], uint8_t addressType);
int32_t IposlProfileStartTerminal(const uint8_t gateway[IPOSL_LAYER2_ID_LEN], uint8_t addressType,
    const uint8_t localLayer2[IPOSL_LAYER2_ID_LEN]);
void IposlProfileStopClient(void);
uint16_t IposlProfileIdentityServiceMemberCount(void);
uint8_t IposlProfileDataProtocolIndicator(void);

int32_t IposlCodecEncodeConfigRequest(const uint8_t layer2[IPOSL_LAYER2_ID_LEN], uint8_t *out, size_t outLen);
int32_t IposlCodecEncodeOpenRequest(const uint8_t layer2[IPOSL_LAYER2_ID_LEN], uint8_t *out, size_t outLen);
int32_t IposlCodecEncodeResponse(uint8_t opcode, const uint8_t layer2[IPOSL_LAYER2_ID_LEN], uint8_t result,
    uint8_t *out, size_t outLen);
int32_t IposlCodecDecodeRequest(const uint8_t *data, size_t len, uint8_t *opcode,
    uint8_t layer2[IPOSL_LAYER2_ID_LEN]);
int32_t IposlCodecDecodeResponse(const uint8_t *data, size_t len, uint8_t expectedOpcode,
    uint8_t layer2[IPOSL_LAYER2_ID_LEN], uint8_t *result);
bool IposlCodecVerifyGoldenVectors(void);

#ifdef __cplusplus
}
#endif
#endif  // IPOSL_PROFILE_H
