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
#ifndef SLE_THREAD_UTIL_H
#define SLE_THREAD_UTIL_H

#include <functional>
#include "nearlink_safe_map.h"


namespace OHOS {
namespace Nearlink {

enum ThreadId {
    THREAD_ID_DIS = 0,
    THREAD_ID_LIS,
    THREAD_ID_ICCE,
    THREAD_ID_DATATRANSFER,
    THREAD_ID_SCAN,
    THREAD_ID_PORT,
    THREAD_ID_ADV,
    THREAD_ID_SVC_MGR,
    THREAD_ID_HADM,
    THREAD_ID_HID,
    THREAD_ID_SSAP,
    THREAD_ID_CDSM,
    THREAD_ID_MCP,
    THREAD_ID_ADAPTER,
    THREAD_ID_ASC,
    THREAD_ID_TWS,
    THREAD_ID_VCP,
    THREAD_ID_CCP,
    THREAD_ID_VAS,
    THREAD_ID_COLLABORATION,
    THREAD_ID_BAS,
    THREAD_ID_MIC,
    THREAD_ID_AUDIO_FW_ADAPTER,
    THREAD_ID_DEVICE_ADAPTER,
    THREAD_ID_FIND_CLIENT,
    THREAD_ID_FIND_SERVER,
    THREAD_ID_FIND_PDR,
    THREAD_ID_ANTENNA,
    THREAD_ID_IPSHARE,
    // please add before this.
    THREAD_ID_BUTT
};

using ThreadUtilFunc = std::function<void(void)>;


/**
 * @brief Post the task to the dis thread.
 * @param func The task to be executed.
 * @param delayTime Process the event after 'delayTime' milliseconds.
 */
void DoInDisThread(const ThreadUtilFunc &func, uint64_t delayTime = 0);

/**
 * @brief Post the task to the lis thread.
 * @param func The task to be executed.
 * @param delayTime Process the event after 'delayTime' milliseconds.
 */
void DoInLisThread(const ThreadUtilFunc &func, uint64_t delayTime = 0);

/**
 * @brief Post the task to the icce thread.
 * @param func The task to be executed.
 * @param delayTime Process the event after 'delayTime' milliseconds.
 */
void DoInIcceThread(const ThreadUtilFunc &func, uint64_t delayTime = 0);

/**
 * @brief Post the task to the data transfer thread.
 * @param func The task to be executed.
 * @param delayTime Process the event after 'delayTime' milliseconds.
 */
void DoInDataTransferThread(const ThreadUtilFunc &func, uint64_t delayTime = 0);

/**
 * @brief Post the task to the scan thread.
 * @param func The task to be executed.
 * @param delayTime Process the event after 'delayTime' milliseconds.
 */
void DoInScanThread(const ThreadUtilFunc &func, uint64_t delayTime = 0);

/**
 * @brief Post the task to the port thread.
 * @param func The task to be executed.
 * @param delayTime Process the event after 'delayTime' milliseconds.
 */
void DoInPortThread(const ThreadUtilFunc &func, uint64_t delayTime = 0);

/**
 * @brief Post the task to the adv thread.
 * @param func The task to be executed.
 * @param delayTime Process the event after 'delayTime' milliseconds.
 */
void DoInAdvThread(const ThreadUtilFunc &func, uint64_t delayTime = 0);

/**
 * @brief Post the task to the sle_service_manager thread.
 * @param func The task to be executed.
 * @param delayTime Process the event after 'delayTime' milliseconds.
 */
void DoInServiceManagerThread(const ThreadUtilFunc &func, uint64_t delayTime = 0);

/**
 * @brief Post the task to the hadm thread.
 * @param func The task to be executed.
 * @param delayTime Process the event after 'delayTime' milliseconds.
 */
void DoInHadmThread(const ThreadUtilFunc &func, uint64_t delayTime = 0);

/**
 * @brief Post the task to the hid thread.
 * @param func The task to be executed.
 * @param delayTime Process the event after 'delayTime' milliseconds.
 */
void DoInHidThread(const ThreadUtilFunc &func, uint64_t delayTime = 0);

/**
 * @brief Post the task to the ssap thread.
 * @param func The task to be executed.
 * @param delayTime Process the event after 'delayTime' milliseconds.
 */
void DoInSsapThread(const ThreadUtilFunc &func, uint64_t delayTime = 0);

/**
 * @brief Post the task to the cdsm thread.
 * @param func The task to be executed.
 * @param delayTime Process the event after 'delayTime' milliseconds.
 */
void DoInCdsmThread(const ThreadUtilFunc &func, uint64_t delayTime = 0);

/**
 * @brief Post the task to the MCP thread.
 * @param func The task to be executed.
 * @param delayTime Process the event after 'delayTime' milliseconds.
 */
void DoInMcpThread(const ThreadUtilFunc &func, uint64_t delayTime = 0);

/**
 * @brief Post the task to the SleAdapter thread.
 * @param func The task to be executed.
 * @param delayTime Process the event after 'delayTime' milliseconds.
 */
void DoInAdapterThread(const ThreadUtilFunc &func, uint64_t delayTime = 0);

/**
 * @brief Post the task to the audio asc thread.
 * @param func The task to be executed.
 * @param delayTime Process the event after 'delayTime' milliseconds.
 */
void DoInAscThread(const ThreadUtilFunc &func, uint64_t delayTime = 0);

/**
 * @brief Post the task to the tws thread.
 * @param func The task to be executed.
 * @param delayTime Process the event after 'delayTime' milliseconds.
 */
void DoInTwsThread(const ThreadUtilFunc &func, uint64_t delayTime = 0);

/**
 * @brief Post the task to the VCP thread.
 * @param func The task to be executed.
 * @param delayTime Process the event after 'delayTime' milliseconds.
 */
void DoInVcpThread(const ThreadUtilFunc &func, uint64_t delayTime = 0);

/**
 * @brief Post the task to the ccs thread.
 * @param func The task to be executed.
 * @param delayTime Process the event after 'delayTime' milliseconds.
 */
void DoInCcpThread(const ThreadUtilFunc &func, uint64_t delayTime = 0);

/**
 * @brief Post the task to the vas thread.
 * @param func The task to be executed.
 * @param delayTime Process the event after 'delayTime' milliseconds.
 */
void DoInVasThread(const ThreadUtilFunc &func, uint64_t delayTime = 0);

/**
 * @brief Post the task to the collaboration thread.
 * @param func The task to be executed.
 * @param delayTime Process the event after 'delayTime' milliseconds.
 */
void DoInCollaborationThread(const ThreadUtilFunc &func, uint64_t delayTime = 0);

/**
 * @brief Post the task to the find client thread.
 * @param func The task to be executed.
 * @param delayTime Process the event after 'delayTime' milliseconds.
 */
void DoInFindClientThread(const ThreadUtilFunc &func, uint64_t delayTime = 0);

/**
 * @brief Post the task to the find server thread.
 * @param func The task to be executed.
 * @param delayTime Process the event after 'delayTime' milliseconds.
 */
void DoInFindServerThread(const ThreadUtilFunc &func, uint64_t delayTime = 0);

/**
 * @brief Post the task to the find pdr thread.
 * @param func The task to be executed.
 * @param delayTime Process the event after 'delayTime' milliseconds.
 */
void DoInFindPdrThread(const ThreadUtilFunc &func, uint64_t delayTime = 0);

/**
 * @brief Post the task to the bas thread.
 * @param func The task to be executed.
 * @param delayTime Process the event after 'delayTime' milliseconds.
 */
void DoInBasThread(const ThreadUtilFunc &func, uint64_t delayTime = 0);

/**
 * @brief Post the task to the mic thread.
 * @param func The task to be executed.
 * @param delayTime Process the event after 'delayTime' milliseconds.
 */
void DoInMicThread(const ThreadUtilFunc &func, uint64_t delayTime = 0);

/**
 * @brief Post the task to the audio framwork adapter thread.
 * @param func The task to be executed.
 * @param delayTime Process the event after 'delayTime' milliseconds.
 */
void DoInAudioFwAdapterThread(const ThreadUtilFunc &func, uint64_t delayTime = 0);

/**
 * @brief Post the task to the device adapter thread.
 * @param func The task to be executed.
 * @param delayTime Process the event after 'delayTime' milliseconds.
 */
void DoInDeviceAdapterThread(const ThreadUtilFunc &func, uint64_t delayTime = 0);

/**
 * @brief Post the task to the antenna thread.
 * @param func The task to be executed.
 * @param delayTime Process the event after 'delayTime' milliseconds.
 */
void DoInAntennaThread(const ThreadUtilFunc &func, uint64_t delayTime = 0);

/**
 * @brief Post a task to the private NearLink IP sharing worker.
 */
void DoInIpShareThread(const ThreadUtilFunc &func, uint64_t delayTime = 0);

class ThreadUtil {
public:
    void PostTask(
        int threadId, const ThreadUtilFunc &func, uint64_t delayTime = 0, const std::string &name = std::string());
    /**
     * Remove a task.
     *
     * @param name Name of the task.
     */
    void RemoveTask(int threadId, const std::string &name);
    void ClearThreadStateMap();
    void InitThreadStateMap();

    static ThreadUtil &GetInstance();

private:
    void CheckThreadStateReturn(int threadId, const ThreadUtilFunc &func);
    enum ThreadState : int {
        ENABLED = 0,  // The task function is switched normally.
        DISABLED,  // The task function is not executed.
        NOT_SWITCH_THREAD,  // The task functions is executed in the same thread.
    };
    // threadId <-> thread state
    NearlinkSafeMap<int, ThreadState> threadStateMap_ {};

    ThreadUtil();
    ~ThreadUtil();

    struct impl;
    std::unique_ptr<impl> pimpl;
};
}  // namespace Nearlink
}  // namespace OHOS

#endif  // SLE_THREAD_UTIL_H
