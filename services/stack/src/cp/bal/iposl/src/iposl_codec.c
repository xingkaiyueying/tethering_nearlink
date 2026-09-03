/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */
#include "iposl_codec.h"

#include <string.h>

const uint8_t g_iposlTerminalCapability[IPOSL_TERMINAL_CAPABILITY_LEN] = {
    0x01, 0x00, 0x07, 0x01, 0x05, 0xDC, 0x02, 0x01, 0x03, 0x01
};
const uint8_t g_iposlEmptyState[IPOSL_EMPTY_STATE_LEN] = {0x01, 0x00, 0x00};
const uint8_t g_iposlGatewayCapability[IPOSL_GATEWAY_CAPABILITY_LEN] = {
    0x01, 0x00, 0x09, 0x01, 0x03, 0x02, 0x05, 0xDC, 0x03, 0x01, 0x04, 0x01
};
const uint8_t g_iposlGatewayServing[IPOSL_GATEWAY_CAPABILITY_LEN] = {
    0x01, 0x00, 0x09, 0x01, 0x02, 0x02, 0x05, 0xDC, 0x03, 0x01, 0x04, 0x01
};

int32_t IposlCodecEncodeConfigRequest(const uint8_t layer2[IPOSL_LAYER2_ID_LEN], uint8_t *out, size_t outLen)
{
    if (layer2 == NULL || out == NULL || outLen < IPOSL_CONFIG_REQUEST_LEN) {
        return IPOSL_ERR_INVALID_PARAM;
    }
    out[0] = IPOSL_OPCODE_CONFIGURE;
    (void)memcpy(out + 1, layer2, IPOSL_LAYER2_ID_LEN);
    out[7] = (uint8_t)(IPOSL_MTU >> 8);
    out[8] = (uint8_t)(IPOSL_MTU & 0xFFu);
    out[9] = 0x01;
    out[10] = IPOSL_IP_TYPE_IPV4;
    return (int32_t)IPOSL_CONFIG_REQUEST_LEN;
}

int32_t IposlCodecEncodeOpenRequest(const uint8_t layer2[IPOSL_LAYER2_ID_LEN], uint8_t *out, size_t outLen)
{
    if (layer2 == NULL || out == NULL || outLen < IPOSL_OPEN_REQUEST_LEN) {
        return IPOSL_ERR_INVALID_PARAM;
    }
    out[0] = IPOSL_OPCODE_ENABLE;
    (void)memcpy(out + 1, layer2, IPOSL_LAYER2_ID_LEN);
    return (int32_t)IPOSL_OPEN_REQUEST_LEN;
}

int32_t IposlCodecEncodeResponse(uint8_t opcode, const uint8_t layer2[IPOSL_LAYER2_ID_LEN], uint8_t result,
    uint8_t *out, size_t outLen)
{
    if (layer2 == NULL || out == NULL || outLen < IPOSL_RESPONSE_LEN ||
        (opcode != IPOSL_OPCODE_CONFIGURE && opcode != IPOSL_OPCODE_ENABLE)) {
        return IPOSL_ERR_INVALID_PARAM;
    }
    out[0] = opcode;
    (void)memcpy(out + 1, layer2, IPOSL_LAYER2_ID_LEN);
    out[7] = result;
    return (int32_t)IPOSL_RESPONSE_LEN;
}

int32_t IposlCodecDecodeRequest(const uint8_t *data, size_t len, uint8_t *opcode,
    uint8_t layer2[IPOSL_LAYER2_ID_LEN])
{
    if (data == NULL || opcode == NULL || layer2 == NULL || len < 1) {
        return IPOSL_ERR_INVALID_PARAM;
    }
    if (data[0] == IPOSL_OPCODE_CONFIGURE) {
        if (len != IPOSL_CONFIG_REQUEST_LEN || data[7] != 0x05 || data[8] != 0xDC ||
            data[9] != 0x01 || data[10] != IPOSL_IP_TYPE_IPV4) {
            return IPOSL_ERR_INVALID_PARAM;
        }
    } else if (data[0] == IPOSL_OPCODE_ENABLE) {
        if (len != IPOSL_OPEN_REQUEST_LEN) {
            return IPOSL_ERR_INVALID_PARAM;
        }
    } else {
        return IPOSL_ERR_INVALID_PARAM;
    }
    *opcode = data[0];
    (void)memcpy(layer2, data + 1, IPOSL_LAYER2_ID_LEN);
    return IPOSL_SUCCESS;
}

int32_t IposlCodecDecodeResponse(const uint8_t *data, size_t len, uint8_t expectedOpcode,
    uint8_t layer2[IPOSL_LAYER2_ID_LEN], uint8_t *result)
{
    if (data == NULL || layer2 == NULL || result == NULL || len != IPOSL_RESPONSE_LEN ||
        (expectedOpcode != IPOSL_OPCODE_CONFIGURE && expectedOpcode != IPOSL_OPCODE_ENABLE) ||
        data[0] != expectedOpcode) {
        return IPOSL_ERR_INVALID_PARAM;
    }
    if (data[7] != 0xFF && ((expectedOpcode == IPOSL_OPCODE_CONFIGURE && data[7] > 0x07) ||
        (expectedOpcode == IPOSL_OPCODE_ENABLE && data[7] > 0x03))) {
        return IPOSL_ERR_INVALID_PARAM;
    }
    (void)memcpy(layer2, data + 1, IPOSL_LAYER2_ID_LEN);
    *result = data[7];
    return IPOSL_SUCCESS;
}

bool IposlCodecVerifyGoldenVectors(void)
{
    static const uint8_t layer2[IPOSL_LAYER2_ID_LEN] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
    static const uint8_t configRequest[IPOSL_CONFIG_REQUEST_LEN] = {
        0x01, 0x02, 0x11, 0x22, 0x33, 0x44, 0x55, 0x05, 0xDC, 0x01, 0x01
    };
    static const uint8_t openRequest[IPOSL_OPEN_REQUEST_LEN] = {
        0x02, 0x02, 0x11, 0x22, 0x33, 0x44, 0x55
    };
    static const uint8_t configResponse[IPOSL_RESPONSE_LEN] = {
        0x01, 0x02, 0x11, 0x22, 0x33, 0x44, 0x55, 0x00
    };
    uint8_t out[IPOSL_CONFIG_REQUEST_LEN] = {0};
    uint8_t decodedLayer2[IPOSL_LAYER2_ID_LEN] = {0};
    uint8_t opcode = 0;
    uint8_t result = 0xFF;
    if (IposlCodecEncodeConfigRequest(layer2, out, sizeof(out)) != (int32_t)sizeof(configRequest) ||
        memcmp(out, configRequest, sizeof(configRequest)) != 0 ||
        IposlCodecDecodeRequest(out, sizeof(configRequest), &opcode, decodedLayer2) != IPOSL_SUCCESS ||
        opcode != IPOSL_OPCODE_CONFIGURE || memcmp(decodedLayer2, layer2, sizeof(layer2)) != 0) {
        return false;
    }
    if (IposlCodecEncodeOpenRequest(layer2, out, sizeof(out)) != (int32_t)sizeof(openRequest) ||
        memcmp(out, openRequest, sizeof(openRequest)) != 0 ||
        IposlCodecDecodeRequest(out, sizeof(openRequest), &opcode, decodedLayer2) != IPOSL_SUCCESS ||
        opcode != IPOSL_OPCODE_ENABLE) {
        return false;
    }
    if (IposlCodecDecodeResponse(configResponse, sizeof(configResponse), IPOSL_OPCODE_CONFIGURE,
        decodedLayer2, &result) != IPOSL_SUCCESS || result != 0) {
        return false;
    }
    out[0] = 0x7F;
    return IposlCodecDecodeRequest(out, 1, &opcode, decodedLayer2) == IPOSL_ERR_INVALID_PARAM;
}
