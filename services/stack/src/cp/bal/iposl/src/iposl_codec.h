/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */
#ifndef IPOSL_CODEC_H
#define IPOSL_CODEC_H

#include "iposl_profile.h"

#define IPOSL_CONFIG_REQUEST_LEN 11u
#define IPOSL_OPEN_REQUEST_LEN 7u
#define IPOSL_RESPONSE_LEN 8u
#define IPOSL_TERMINAL_CAPABILITY_LEN 10u
#define IPOSL_EMPTY_STATE_LEN 3u
#define IPOSL_GATEWAY_CAPABILITY_LEN 12u

extern const uint8_t g_iposlTerminalCapability[IPOSL_TERMINAL_CAPABILITY_LEN];
extern const uint8_t g_iposlEmptyState[IPOSL_EMPTY_STATE_LEN];
extern const uint8_t g_iposlGatewayCapability[IPOSL_GATEWAY_CAPABILITY_LEN];
extern const uint8_t g_iposlGatewayServing[IPOSL_GATEWAY_CAPABILITY_LEN];

#endif  // IPOSL_CODEC_H
