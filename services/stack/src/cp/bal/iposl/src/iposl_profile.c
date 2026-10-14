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

#include <string.h>
#include <stdatomic.h>
#include <pthread.h>
#include <time.h>
#include "cp_worker.h"
#include "sdf_mem.h"
#include "sdf_buff.h"

#include "dtap.h"
#include "nlstk_log.h"

static IposlProfileCallbacks g_callbacks;
static atomic_bool g_initialized;
static bool g_callbacksInstalled;
static atomic_uint_fast64_t g_sendGeneration;
static void IposlCancelAllSends(void);
static bool IposlAllSendsDrained(void);

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
    if (!IposlAllSendsDrained()) return IPOSL_ERR_INVALID_STATE;
    if (!IposlCodecVerifyGoldenVectors()) {
        NLSTK_LOG_ERROR("[IpShare][IPoSL] profile init failed: codec golden vectors");
        return IPOSL_ERR_INVALID_STATE;
    }
    if (!g_callbacksInstalled) {
        g_callbacks = *callbacks;
        g_callbacksInstalled = true;
    } else if (g_callbacks.onPeerSupported != callbacks->onPeerSupported ||
        g_callbacks.onConfigured != callbacks->onConfigured || g_callbacks.prepareMode != callbacks->prepareMode ||
        g_callbacks.isSecure != callbacks->isSecure || g_callbacks.canSend != callbacks->canSend ||
        g_callbacks.onPeerCapabilities != callbacks->onPeerCapabilities ||
        g_callbacks.isSecureAddress != callbacks->isSecureAddress ||
        g_callbacks.onSendComplete != callbacks->onSendComplete) {
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
    atomic_store(&g_sendGeneration, 0);
    IposlCancelAllSends();
    IposlClientStop();
    IposlServerDeinit();
    NLSTK_LOG_INFO("[IpShare][IPoSL] profile deinit completed");
}

int32_t IposlProfileStartServerAny(uint8_t mode, uint32_t capacity, uint64_t generation)
{
    if (!g_initialized) return IPOSL_ERR_INVALID_STATE;
    int32_t ret = IposlServerStartAny(mode, capacity, generation);
    if (ret == IPOSL_SUCCESS) atomic_store(&g_sendGeneration, generation);
    return ret;
}

void IposlProfileReleaseServerPeer(const uint8_t peer[IPOSL_LAYER2_ID_LEN], uint8_t addressType)
{
    (void)IposlServerReleasePeer(peer, addressType);
}

int32_t IposlProfileReleaseServerPeerChecked(const uint8_t peer[IPOSL_LAYER2_ID_LEN], uint8_t addressType)
{
    return IposlServerReleasePeer(peer, addressType);
}

int32_t IposlProfileStartServer(const uint8_t peer[IPOSL_LAYER2_ID_LEN], uint8_t addressType, uint8_t mode, uint64_t generation)
{
    if (!g_initialized) {
        NLSTK_LOG_ERROR("[IpShare][IPoSL] server start rejected: profile not initialized");
        return IPOSL_ERR_INVALID_STATE;
    }
    atomic_store(&g_sendGeneration, generation);
    int32_t ret = IposlServerStart(peer, addressType, mode, generation);
    if (ret != IPOSL_SUCCESS) {
        NLSTK_LOG_ERROR("[IpShare][IPoSL] server start failed addressType=%u ret=%d", addressType, ret);
    } else {
        NLSTK_LOG_INFO("[IpShare][IPoSL] server start completed addressType=%u", addressType);
    }
    return ret;
}

void IposlProfileStopServer(void)
{
    atomic_store(&g_sendGeneration, 0);
    IposlCancelAllSends();
    IposlServerStop();
    NLSTK_LOG_INFO("[IpShare][IPoSL] server stopped");
}

bool IposlProfileServerIsDrained(void)
{
    return IposlServerIsDrained() && IposlAllSendsDrained();
}

int32_t IposlProfileProbePeer(const uint8_t peer[IPOSL_LAYER2_ID_LEN], uint8_t addressType, uint8_t mode, uint64_t generation)
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
    atomic_store(&g_sendGeneration, generation);
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
    atomic_store(&g_sendGeneration, 0);
    IposlCancelAllSends();
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

/* IPoSL-owned bounded queues. All DTAP access stays on CP; no shared scheduler changes. */
#define IPOSL_SEND_CHANNELS 32u
#define IPOSL_SEND_LIMIT 64u
#define IPOSL_SEND_HIGH 48u
#define IPOSL_SEND_LOW 16u
#define IPOSL_SEND_BATCH 8u
typedef struct IposlSendPacket {
    struct IposlSendPacket *next;
    SDF_Buff_S *buff;
    uint16_t lcid;
    uint8_t tcid;
    uint8_t pi;
    uint64_t generation;
} IposlSendPacket;
typedef struct {
    IposlSendPacket *head;
    IposlSendPacket *tail;
    uint16_t lcid;
    uint8_t tcid;
    uint64_t generation;
    uint32_t count; /* Includes the packet being processed outside the lock. */
    bool used;
    bool scheduled;
    bool cancelled;
    bool paused;
} IposlSendQueue;
static IposlSendQueue g_sendQueues[IPOSL_SEND_CHANNELS];
static pthread_mutex_t g_sendMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_sendCapacity = PTHREAD_COND_INITIALIZER;

static IposlSendQueue *IposlFindSendQueue(uint16_t lcid, uint8_t tcid, uint64_t generation)
{
    for (size_t i = 0; i < IPOSL_SEND_CHANNELS; ++i) {
        IposlSendQueue *q = &g_sendQueues[i];
        if (q->used && q->lcid == lcid && q->tcid == tcid && q->generation == generation) return q;
    }
    return NULL;
}

static void IposlFinishPacket(IposlSendPacket *packet, int32_t result)
{
    const IposlProfileCallbacks *callbacks = IposlGetCallbacks();
    if (callbacks != NULL && callbacks->onSendComplete != NULL) {
        callbacks->onSendComplete(packet->lcid, packet->tcid, packet->pi, packet->generation, result);
    }
    if (packet->buff != NULL) SDF_BuffFree(packet->buff);
    SDF_MemFree(packet);
}

static void IposlFreeUnadmitted(IposlSendPacket *packet)
{
    if (packet->buff != NULL) SDF_BuffFree(packet->buff);
    SDF_MemFree(packet);
}

static void IposlDropPackets(IposlSendPacket *packet)
{
    while (packet != NULL) {
        IposlSendPacket *next = packet->next;
        IposlFinishPacket(packet, IPOSL_ERR_INVALID_STATE);
        packet = next;
    }
}

static void IposlSendOnCp(void *arg)
{
    IposlSendQueue *q = arg;
    struct timespec started;
    bool timed = clock_gettime(CLOCK_MONOTONIC, &started) == 0;
    for (unsigned n = 0; n < IPOSL_SEND_BATCH; ++n) {
        pthread_mutex_lock(&g_sendMutex);
        IposlSendPacket *packet = q->head;
        if (packet == NULL) {
            pthread_mutex_unlock(&g_sendMutex);
            break;
        }
        q->head = packet->next;
        if (q->head == NULL) q->tail = NULL;
        bool cancelled = q->cancelled;
        pthread_mutex_unlock(&g_sendMutex);
        int32_t result = IPOSL_ERR_INVALID_STATE;
        const IposlProfileCallbacks *callbacks = IposlGetCallbacks();
        if (!cancelled && callbacks != NULL &&
            callbacks->canSend(packet->lcid, packet->tcid, packet->pi, packet->generation)) {
            pthread_mutex_lock(&g_sendMutex);
            cancelled = q->cancelled;
            pthread_mutex_unlock(&g_sendMutex);
            if (!cancelled) {
                DTAP_Data_S data = {.pi = packet->pi, .lcid = packet->lcid,
                    .tcid = packet->tcid, .buff = packet->buff};
                result = (int32_t)DTAP_DataSend(&data);
                if (result == 0) packet->buff = NULL; /* DTAP owns it after admission. */
                else NLSTK_LOG_ERROR("[IpShare][TX] CP DTAP send failed ret=%d", result);
            }
        }
        IposlFinishPacket(packet, result);
        pthread_mutex_lock(&g_sendMutex);
        --q->count;
        if (q->count <= IPOSL_SEND_LOW) {
            q->paused = false;
            pthread_cond_broadcast(&g_sendCapacity);
        }
        pthread_mutex_unlock(&g_sendMutex);
        struct timespec now;
        if (timed && clock_gettime(CLOCK_MONOTONIC, &now) == 0 &&
            (now.tv_sec - started.tv_sec) * 1000000000LL + now.tv_nsec - started.tv_nsec >= 2000000LL) break;
    }
    pthread_mutex_lock(&g_sendMutex);
    IposlSendPacket *dropped = NULL;
    if (q->head != NULL && CP_PostTask(IposlSendOnCp, q, NULL) != 0) {
        dropped = q->head;
        q->head = q->tail = NULL;
        q->cancelled = true;
    }
    if (q->head == NULL && dropped == NULL) {
        q->scheduled = false;
        q->paused = false;
        if (q->cancelled) q->used = false;
        pthread_cond_broadcast(&g_sendCapacity);
    }
    pthread_mutex_unlock(&g_sendMutex);
    IposlDropPackets(dropped);
    if (dropped != NULL) {
        pthread_mutex_lock(&g_sendMutex);
        q->count = 0; q->scheduled = false; q->used = false;
        pthread_cond_broadcast(&g_sendCapacity);
        pthread_mutex_unlock(&g_sendMutex);
    }
}

static void IposlCancelAllSends(void)
{
    pthread_mutex_lock(&g_sendMutex);
    for (size_t i = 0; i < IPOSL_SEND_CHANNELS; ++i) {
        g_sendQueues[i].cancelled = true;
        if (!g_sendQueues[i].scheduled && g_sendQueues[i].count == 0) g_sendQueues[i].used = false;
    }
    pthread_cond_broadcast(&g_sendCapacity);
    pthread_mutex_unlock(&g_sendMutex);
}

static bool IposlAllSendsDrained(void)
{
    bool drained = true;
    pthread_mutex_lock(&g_sendMutex);
    for (size_t i = 0; i < IPOSL_SEND_CHANNELS; ++i) {
        if (g_sendQueues[i].scheduled || g_sendQueues[i].count != 0) drained = false;
    }
    pthread_mutex_unlock(&g_sendMutex);
    return drained;
}

void IposlProfileCancelSend(uint16_t lcid, uint8_t tcid, uint64_t generation)
{
    pthread_mutex_lock(&g_sendMutex);
    IposlSendQueue *q = IposlFindSendQueue(lcid, tcid, generation);
    if (q != NULL) {
        q->cancelled = true;
        if (!q->scheduled && q->count == 0) q->used = false;
    }
    pthread_cond_broadcast(&g_sendCapacity);
    pthread_mutex_unlock(&g_sendMutex);
}

bool IposlProfileSendIsDrained(uint16_t lcid, uint8_t tcid, uint64_t generation)
{
    pthread_mutex_lock(&g_sendMutex);
    IposlSendQueue *q = IposlFindSendQueue(lcid, tcid, generation);
    bool drained = q == NULL || (!q->scheduled && q->count == 0);
    pthread_mutex_unlock(&g_sendMutex);
    return drained;
}

bool IposlProfileWaitSendCapacity(uint16_t lcid, uint8_t tcid, uint64_t generation)
{
    pthread_mutex_lock(&g_sendMutex);
    IposlSendQueue *q = IposlFindSendQueue(lcid, tcid, generation);
    if (q != NULL && q->count >= IPOSL_SEND_HIGH) q->paused = true;
    if (q != NULL && q->paused && !q->cancelled && atomic_load(&g_initialized)) {
        struct timespec deadline;
        if (clock_gettime(CLOCK_REALTIME, &deadline) != 0) {
            pthread_mutex_unlock(&g_sendMutex);
            return false;
        }
        deadline.tv_nsec += 100000000L;
        if (deadline.tv_nsec >= 1000000000L) { ++deadline.tv_sec; deadline.tv_nsec -= 1000000000L; }
        (void)pthread_cond_timedwait(&g_sendCapacity, &g_sendMutex, &deadline);
        q = IposlFindSendQueue(lcid, tcid, generation);
    }
    bool ready = atomic_load(&g_initialized) && (q == NULL || (!q->cancelled && !q->paused));
    pthread_mutex_unlock(&g_sendMutex);
    return ready;
}

int32_t IposlProfileSendIpv4(uint16_t lcid, uint8_t tcid, const uint8_t *data, uint16_t length)
{
    return IposlProfileSendIp(lcid, tcid, DTAP_PI_IPV4, data, length, atomic_load(&g_sendGeneration));
}

int32_t IposlProfileSendIp(uint16_t lcid, uint8_t tcid, uint8_t pi, const uint8_t *data,
    uint16_t length, uint64_t generation)
{
    if (generation == 0 || !atomic_load(&g_initialized) || !IposlCodecValidatePacket(pi, data, length)) {
        return IPOSL_ERR_INVALID_PARAM;
    }
    IposlSendPacket *packet = SDF_MemAlloc(sizeof(*packet));
    if (packet == NULL) return IPOSL_ERR_INVALID_STATE;
    memset(packet, 0, sizeof(*packet));
    packet->buff = SDF_BuffNewWithReserve(length);
    uint8_t *payload = packet->buff == NULL ? NULL : SDF_BuffAppend(packet->buff, length);
    if (payload == NULL) {
        if (packet->buff != NULL) SDF_BuffFree(packet->buff);
        SDF_MemFree(packet);
        return IPOSL_ERR_INVALID_STATE;
    }
    memcpy(payload, data, length);
    packet->lcid = lcid; packet->tcid = tcid; packet->pi = pi; packet->generation = generation;
    pthread_mutex_lock(&g_sendMutex);
    IposlSendQueue *q = IposlFindSendQueue(lcid, tcid, generation);
    if (q == NULL) {
        for (size_t i = 0; i < IPOSL_SEND_CHANNELS; ++i) {
            if (!g_sendQueues[i].used) {
                q = &g_sendQueues[i];
                memset(q, 0, sizeof(*q));
                q->used = true; q->lcid = lcid; q->tcid = tcid; q->generation = generation;
                break;
            }
        }
    }
    if (!atomic_load(&g_initialized) || q == NULL || q->cancelled || q->count >= IPOSL_SEND_LIMIT) {
        pthread_mutex_unlock(&g_sendMutex);
        IposlFreeUnadmitted(packet);
        return IPOSL_ERR_INVALID_STATE;
    }
    if (q->tail == NULL) q->head = packet;
    else q->tail->next = packet;
    q->tail = packet;
    ++q->count;
    if (!q->scheduled) {
        q->scheduled = true;
        if (CP_PostTask(IposlSendOnCp, q, NULL) != 0) {
            q->head = q->tail = NULL; q->count = 0; q->scheduled = false;
            pthread_mutex_unlock(&g_sendMutex);
            IposlFreeUnadmitted(packet);
            return IPOSL_ERR_INVALID_STATE;
        }
    }
    pthread_mutex_unlock(&g_sendMutex);
    return IPOSL_SUCCESS;
}
