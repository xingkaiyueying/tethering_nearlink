/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */
#ifndef IPOSL_INTERNAL_H
#define IPOSL_INTERNAL_H

#include "iposl_profile.h"

const IposlProfileCallbacks *IposlGetCallbacks(void);
int32_t IposlServerInitialize(void);
void IposlServerDeinit(void);
int32_t IposlServerStart(const uint8_t peer[IPOSL_LAYER2_ID_LEN], uint8_t addressType);
void IposlServerStop(void);
int32_t IposlClientStart(const uint8_t peer[IPOSL_LAYER2_ID_LEN], uint8_t addressType, bool terminal,
    const uint8_t localLayer2[IPOSL_LAYER2_ID_LEN]);
void IposlClientStop(void);

#endif  // IPOSL_INTERNAL_H
