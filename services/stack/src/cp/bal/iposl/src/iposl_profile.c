/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "iposl_internal.h"

#include "cp_worker.h"
#include "sdf_buff.h"
#include "sdf_mem.h"
#include <stdatomic.h>
#include <string.h>

#include "dtap.h"
#include "nlstk_log.h"

static IposlProfileCallbacks g_callbacks;
static atomic_bool g_initialized;
static bool g_callbacksInstalled;

const IposlProfileCallbacks *IposlGetCallbacks(void)
{
    return g_initialized ? &g_callbacks : NULL;
}

int32_t IposlProfileInit(const IposlProfileCallbacks *callbacks)
{
    if (callbacks == NULL || callbacks->onPeerSupported == NULL || callbacks->onConfigured == NULL ||
        callbacks->prepareMode == NULL || callbacks->isSecure == NULL || callbacks->canSend == NULL) {
        NLSTK_LOG_ERROR("[IpShare][IPoSL] profile init rejected: callback incomplete");
        return IPOSL_ERR_INVALID_PARAM;
    }
    NLSTK_LOG_INFO("[IpShare][IPoSL] profile init started");
    if (!g_callbacksInstalled) {
        g_callbacks = *callbacks;
        g_callbacksInstalled = true;
    } else if (g_callbacks.onPeerSupported != callbacks->onPeerSupported ||
               g_callbacks.onConfigured != callbacks->onConfigured ||
               g_callbacks.prepareMode != callbacks->prepareMode || g_callbacks.isSecure != callbacks->isSecure ||
               g_callbacks.canSend != callbacks->canSend ||
               g_callbacks.onPeerCapabilities != callbacks->onPeerCapabilities ||
               g_callbacks.isSecureAddress != callbacks->isSecureAddress) {
        return IPOSL_ERR_INVALID_STATE;
    }
    int32_t ret = IposlServerInitialize();
    if (ret != IPOSL_SUCCESS) {
        g_initialized = false;
        NLSTK_LOG_ERROR("[IpShare][IPoSL] profile init failed: server init ret=%d", ret);
        return IPOSL_ERR_SSAP;
    }
    g_initialized = true;
    NLSTK_LOG_INFO("[IpShare][IPoSL] profile init completed pi=%u", DTAP_PI_IPV4);
    return IPOSL_SUCCESS;
}

void IposlProfileDeinit(void)
{
    NLSTK_LOG_INFO("[IpShare][IPoSL] profile deinit started");
    g_initialized = false;
    IposlClientStop();
    IposlServerDeinit();
    NLSTK_LOG_INFO("[IpShare][IPoSL] profile deinit completed");
}

int32_t IposlProfileStartServerAny(uint8_t mode, uint32_t capacity, uint64_t generation)
{
    if (!g_initialized) {
        return IPOSL_ERR_INVALID_STATE;
    }
    return IposlServerStartAny(mode, capacity, generation);
}

int32_t IposlProfileReleaseServerPeerChecked(const uint8_t peer[IPOSL_LAYER2_ID_LEN], uint8_t addressType)
{
    return IposlServerReleasePeer(peer, addressType);
}

void IposlProfileStopServer(void)
{
    IposlServerStop();
    NLSTK_LOG_INFO("[IpShare][IPoSL] server stopped");
}

bool IposlProfileServerIsDrained(void)
{
    return IposlServerIsDrained();
}

int32_t IposlProfileProbePeer(const uint8_t peer[IPOSL_LAYER2_ID_LEN], uint8_t addressType, uint8_t mode,
                              uint64_t generation)
{
    if (!g_initialized) {
        NLSTK_LOG_ERROR("[IpShare][IPoSL] support probe rejected: profile not initialized");
        return IPOSL_ERR_INVALID_STATE;
    }
    int32_t ret = IposlClientStart(peer, addressType, false, NULL, mode, generation);
    if (ret != IPOSL_SUCCESS) {
        NLSTK_LOG_ERROR("[IpShare][IPoSL] support probe client start failed addressType=%u ret=%d", addressType, ret);
    } else {
        NLSTK_LOG_INFO("[IpShare][IPoSL] support probe client start completed addressType=%u", addressType);
    }
    return ret;
}

int32_t IposlProfileStartTerminal(const uint8_t gateway[IPOSL_LAYER2_ID_LEN], uint8_t addressType,
                                  const uint8_t localLayer2[IPOSL_LAYER2_ID_LEN], uint8_t mode, uint64_t generation)
{
    if (!g_initialized) {
        NLSTK_LOG_ERROR("[IpShare][IPoSL] terminal start rejected: profile not initialized");
        return IPOSL_ERR_INVALID_STATE;
    }
    int32_t ret = IposlClientStart(gateway, addressType, true, localLayer2, mode, generation);
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

/* The caller and CP task each own one reference, including post failure/timeout. */
typedef struct {
    atomic_int refs;
    int32_t result;
    uint16_t lcid;
    uint16_t length;
    uint8_t tcid;
    uint8_t pi;
    uint64_t generation;
    uint8_t payload[];
} IposlSendTask;

static void IposlSendRelease(void *arg)
{
    IposlSendTask *task = arg;
    if (atomic_fetch_sub(&task->refs, 1) == 1) {
        SDF_MemFree(task);
    }
}

int32_t IposlProfileSendIpOnCp(uint16_t lcid, uint8_t tcid, uint8_t pi, const uint8_t *data, uint16_t length,
                               uint64_t generation)
{
    if (!IposlCodecValidatePacket(pi, data, length)) {
        return IPOSL_ERR_INVALID_PARAM;
    }
    const IposlProfileCallbacks *callbacks = IposlGetCallbacks();
    if (callbacks == NULL || callbacks->canSend == NULL || !callbacks->canSend(lcid, tcid, pi, generation)) {
        return IPOSL_ERR_INVALID_STATE;
    }
    SDF_Buff_S *buff = SDF_BuffNewWithReserve(length);
    if (buff == NULL) {
        return IPOSL_ERR_INVALID_STATE;
    }
    uint8_t *payload = SDF_BuffAppend(buff, length);
    if (payload == NULL) {
        SDF_BuffFree(buff);
        return IPOSL_ERR_INVALID_STATE;
    }
    (void)memcpy(payload, data, length);
    DTAP_Data_S packet = {.pi = pi, .lcid = lcid, .tcid = tcid, .buff = buff};
    int32_t result = (int32_t)DTAP_DataSend(&packet);
    if (result != 0) {
        SDF_BuffFree(buff);
    }
    return result;
}

static void IposlSendOnCp(void *arg)
{
    IposlSendTask *task = arg;
    task->result = IposlProfileSendIpOnCp(task->lcid, task->tcid, task->pi, task->payload,
                                       task->length, task->generation);
}

int32_t IposlProfileSendIp(uint16_t lcid, uint8_t tcid, uint8_t pi, const uint8_t *data, uint16_t length,
                           uint64_t generation)
{
    if (!IposlCodecValidatePacket(pi, data, length)) {
        return IPOSL_ERR_INVALID_PARAM;
    }
    IposlSendTask *task = SDF_MemAlloc(sizeof(*task) + length);
    if (task == NULL) {
        return IPOSL_ERR_INVALID_STATE;
    }
    atomic_init(&task->refs, 2);
    task->result = IPOSL_ERR_INVALID_STATE;
    task->lcid = lcid;
    task->tcid = tcid;
    task->pi = pi;
    task->generation = generation;
    task->length = length;
    (void)memcpy(task->payload, data, length);
    /* Like Transport, wait at most 500 ms. The task owns its copy after timeout. */
    uint32_t posted = CP_PostTaskBlocked(IposlSendOnCp, task, IposlSendRelease, 500);
    int32_t result = posted == 0 ? task->result : IPOSL_ERR_INVALID_STATE;
    IposlSendRelease(task);
    return result;
}
