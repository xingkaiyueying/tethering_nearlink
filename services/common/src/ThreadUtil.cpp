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
#include "ThreadUtil.h"
#include "datetime_ex.h"
#include "log.h"
#include "ffrt_inner.h"

const uint64_t DELAY_TIME_MS_MAX = 0xFFFFFFFF;  // Max delay time is 8 years
const uint64_t MILLISEC_TO_MICROSEC = 1000; // convert ms

int GetFfrtQueueId(void)
{
    return ffrt::get_queue_id();
}

namespace OHOS {
namespace Nearlink {

static void PostTaskToThread(int threadId, const ThreadUtilFunc &func, uint64_t delayTime)
{
    ThreadUtil::GetInstance().PostTask(threadId, func, delayTime);
}

void DoInDisThread(const ThreadUtilFunc &func, uint64_t delayTime)
{
    PostTaskToThread(THREAD_ID_DIS, func, delayTime);
}

void DoInLisThread(const ThreadUtilFunc &func, uint64_t delayTime)
{
    PostTaskToThread(THREAD_ID_LIS, func, delayTime);
}

void DoInIcceThread(const ThreadUtilFunc &func, uint64_t delayTime)
{
    PostTaskToThread(THREAD_ID_ICCE, func, delayTime);
}

void DoInDataTransferThread(const ThreadUtilFunc &func, uint64_t delayTime)
{
    PostTaskToThread(THREAD_ID_DATATRANSFER, func, delayTime);
}

void DoInScanThread(const ThreadUtilFunc &func, uint64_t delayTime)
{
    PostTaskToThread(THREAD_ID_SCAN, func, delayTime);
}

void DoInPortThread(const ThreadUtilFunc &func, uint64_t delayTime)
{
    HILOGD("enter");
    PostTaskToThread(THREAD_ID_PORT, func, delayTime);
}

void DoInAdvThread(const ThreadUtilFunc &func, uint64_t delayTime)
{
    PostTaskToThread(THREAD_ID_ADV, func, delayTime);
}

void DoInServiceManagerThread(const ThreadUtilFunc &func, uint64_t delayTime)
{
    HILOGD("enter");
    PostTaskToThread(THREAD_ID_SVC_MGR, func, delayTime);
}

void DoInHadmThread(const ThreadUtilFunc &func, uint64_t delayTime)
{
    PostTaskToThread(THREAD_ID_HADM, func, delayTime);
}

void DoInHidThread(const ThreadUtilFunc &func, uint64_t delayTime)
{
    PostTaskToThread(THREAD_ID_HID, func, delayTime);
}

void DoInSsapThread(const ThreadUtilFunc &func, uint64_t delayTime)
{
    PostTaskToThread(THREAD_ID_SSAP, func, delayTime);
}

void DoInCdsmThread(const ThreadUtilFunc &func, uint64_t delayTime)
{
    PostTaskToThread(THREAD_ID_CDSM, func, delayTime);
}

void DoInMcpThread(const ThreadUtilFunc &func, uint64_t delayTime)
{
    PostTaskToThread(THREAD_ID_MCP, func, delayTime);
}

void DoInAdapterThread(const ThreadUtilFunc &func, uint64_t delayTime)
{
    PostTaskToThread(THREAD_ID_ADAPTER, func, delayTime);
}

void DoInAscThread(const ThreadUtilFunc &func, uint64_t delayTime)
{
    PostTaskToThread(THREAD_ID_ASC, func, delayTime);
}

void DoInTwsThread(const ThreadUtilFunc &func, uint64_t delayTime)
{
    PostTaskToThread(THREAD_ID_TWS, func, delayTime);
}

void DoInVcpThread(const ThreadUtilFunc &func, uint64_t delayTime)
{
    PostTaskToThread(THREAD_ID_VCP, func, delayTime);
}

void DoInCcpThread(const ThreadUtilFunc &func, uint64_t delayTime)
{
    PostTaskToThread(THREAD_ID_CCP, func, delayTime);
}

void DoInVasThread(const ThreadUtilFunc &func, uint64_t delayTime)
{
    PostTaskToThread(THREAD_ID_VAS, func, delayTime);
}

void DoInCollaborationThread(const ThreadUtilFunc &func, uint64_t delayTime)
{
    PostTaskToThread(THREAD_ID_COLLABORATION, func, delayTime);
}

void DoInBasThread(const ThreadUtilFunc &func, uint64_t delayTime)
{
    PostTaskToThread(THREAD_ID_BAS, func, delayTime);
}

void DoInMicThread(const ThreadUtilFunc &func, uint64_t delayTime)
{
    PostTaskToThread(THREAD_ID_MIC, func, delayTime);
}

void DoInAudioFwAdapterThread(const ThreadUtilFunc &func, uint64_t delayTime)
{
    PostTaskToThread(THREAD_ID_AUDIO_FW_ADAPTER, func, delayTime);
}

void DoInDeviceAdapterThread(const ThreadUtilFunc &func, uint64_t delayTime)
{
    PostTaskToThread(THREAD_ID_DEVICE_ADAPTER, func, delayTime);
}

void DoInFindClientThread(const ThreadUtilFunc &func, uint64_t delayTime)
{
    PostTaskToThread(THREAD_ID_FIND_CLIENT, func, delayTime);
}

void DoInFindServerThread(const ThreadUtilFunc &func, uint64_t delayTime)
{
    PostTaskToThread(THREAD_ID_FIND_SERVER, func, delayTime);
}

void DoInFindPdrThread(const ThreadUtilFunc &func, uint64_t delayTime)
{
    PostTaskToThread(THREAD_ID_FIND_PDR, func, delayTime);
}

void DoInAntennaThread(const ThreadUtilFunc &func, uint64_t delayTime)
{
    PostTaskToThread(THREAD_ID_ANTENNA, func, delayTime);
}

void DoInIpShareThread(const ThreadUtilFunc &func, uint64_t delayTime)
{
    PostTaskToThread(THREAD_ID_IPSHARE, func, delayTime);
}

// Only for test.
void ThreadUtil::ClearThreadStateMap()
{
    HILOGI("Enter");
    threadStateMap_.Clear();
}

// Only for test.
void ThreadUtil::InitThreadStateMap()
{
    for (int i = 0; i < THREAD_ID_BUTT; i++) {
        threadStateMap_.EnsureInsert(i, ThreadState::ENABLED);
    }
}

static std::string GetThreadName(int threadId)
{
    const std::map<int, std::string> threadNameMap {
        { THREAD_ID_DIS,                "sle_dis" },
        { THREAD_ID_LIS,                "sle_lis" },
        { THREAD_ID_ICCE,               "sle_icce" },
        { THREAD_ID_DATATRANSFER,       "sle_datatransfer" },
        { THREAD_ID_SCAN,               "sle_scan" },
        { THREAD_ID_PORT,               "sle_port" },
        { THREAD_ID_ADV,                "sle_adv" },
        { THREAD_ID_SVC_MGR,            "sle_service_manager" },
        { THREAD_ID_HADM,               "sle_hadm" },
        { THREAD_ID_HID,                "sle_hid" },
        { THREAD_ID_SSAP,               "sle_ssap" },
        { THREAD_ID_CDSM,               "sle_cdsm" },
        { THREAD_ID_MCP,                "sle_mcp" },
        { THREAD_ID_ADAPTER,            "sle_adapter" },
        { THREAD_ID_ASC,                "sle_asc" },
        { THREAD_ID_TWS,                "sle_tws" },
        { THREAD_ID_VCP,                "sle_vcp" },
        { THREAD_ID_CCP,                "sle_ccp" },
        { THREAD_ID_VAS,                "sle_vas" },
        { THREAD_ID_COLLABORATION,      "sle_collaboration" },
        { THREAD_ID_BAS,                "sle_bas" },
        { THREAD_ID_MIC,                "sle_mic" },
        { THREAD_ID_DEVICE_ADAPTER,     "sle_device_adapter" },
        { THREAD_ID_FIND_CLIENT,        "sle_find_client" },
        { THREAD_ID_FIND_SERVER,        "sle_find_server" },
        { THREAD_ID_FIND_PDR,           "sle_find_pdr" },
        { THREAD_ID_ANTENNA,            "sle_antenna" },
        { THREAD_ID_IPSHARE,            "sle_ipshare" },
    };

    auto it = threadNameMap.find(threadId);
    if (it == threadNameMap.end()) {
        HILOGE("Not find threadId: %{public}d", threadId);
        return "Unknown";
    }

    return it->second;
}

struct ThreadUtil::impl {
    using DelayTaskKey = std::pair<int, std::string>;
    struct DelayTask {
        std::string name = "";
        std::atomic_bool trigger = false;
        ffrt::task_handle taskHandle = nullptr;
    };
    class TaskQueue {
    public:
        explicit TaskQueue(const char *name) : queue_(name, ffrt::queue_attr().qos(ffrt::qos_user_interactive)) {}
        ~TaskQueue() = default;

        void PostTask(const ThreadUtilFunc &func, uint64_t delayTime, const std::string &name);
        void PostDelayTask(const ThreadUtilFunc &func, uint64_t delayTime, const std::string &name);
        void RemoveTask(const std::string &name);
        int GetQueueId(void);

    private:
        ffrt::queue queue_;
        ffrt::mutex delayTaskVecMutex_ {};
        std::vector<std::shared_ptr<DelayTask>> delayTaskVec_ {};
    };

    impl();
    ~impl() = default;
    std::shared_ptr<TaskQueue> CreateTaskQueue(int threadId);

    NearlinkSafeMap<int, std::shared_ptr<TaskQueue>> taskQueueMap_ {};
    ffrt::mutex mutex_{};
};

ThreadUtil::impl::impl()
{}

ThreadUtil::ThreadUtil() : pimpl(std::make_unique<impl>())
{
    for (int i = 0; i < THREAD_ID_BUTT; i++) {
        threadStateMap_.EnsureInsert(i, ThreadState::ENABLED);
    }
}

ThreadUtil::~ThreadUtil()
{
    threadStateMap_.Clear();
}

void ThreadUtil::impl::TaskQueue::PostDelayTask(const ThreadUtilFunc &func, uint64_t delayTime, const std::string &name)
{
    NL_CHECK_RETURN(delayTime < DELAY_TIME_MS_MAX, "Invalid delaytime(%{public}lu), taskName(%{public}s)",
        delayTime, name.c_str());

    {
        std::lock_guard<ffrt::mutex> lock(delayTaskVecMutex_);
        // Remove the delayed task if it's triggered, or it's a same task.
        for (auto it = delayTaskVec_.begin(); it != delayTaskVec_.end();) {
            if ((*it)->name == name || (*it)->trigger.load()) {
                queue_.cancel((*it)->taskHandle);
                it = delayTaskVec_.erase(it);
            } else {
                it++;
            }
        }
    }

    // Push a new delayed task
    std::shared_ptr<DelayTask> delayTask = std::make_shared<DelayTask>();
    NL_CHECK_RETURN(delayTask, "delayTask is nullptr");

    ffrt::task_attr taskAttr;
    taskAttr.name(name.c_str()).delay(delayTime * MILLISEC_TO_MICROSEC);
    auto taskFunc = [delayTask, func]() {
        delayTask->trigger = true;
        func();
    };

    auto taskHandle = queue_.submit_h(taskFunc, taskAttr);
    NL_CHECK_RETURN(taskHandle, "ffrt submit task failed");

    delayTask->name = name;
    delayTask->taskHandle = std::move(taskHandle);

    std::lock_guard<ffrt::mutex> lock(delayTaskVecMutex_);
    delayTaskVec_.push_back(delayTask);
}

void ThreadUtil::impl::TaskQueue::PostTask(const ThreadUtilFunc &func, uint64_t delayTime, const std::string &name)
{
    if (delayTime > 0) {
        PostDelayTask(func, delayTime, name);
        return;
    }

    queue_.submit(func);
}

void ThreadUtil::impl::TaskQueue::RemoveTask(const std::string &name)
{
    std::lock_guard<ffrt::mutex> lock(delayTaskVecMutex_);
    auto it = std::find_if(
        delayTaskVec_.begin(), delayTaskVec_.end(), [&name](auto &task) { return task->name == name; });
    if (it == delayTaskVec_.end()) {
        return;
    }

    queue_.cancel((*it)->taskHandle);
    delayTaskVec_.erase(it);
}

int ThreadUtil::impl::TaskQueue::GetQueueId(void)
{
    // Get the ffrt queue id, for debugging
    int id = -1;
    auto handle = queue_.submit_h([&id]() {
        id = ffrt::get_queue_id();
    });
    queue_.wait(handle);
    return id;
}

// Check thread state for unit test.
#define CHECK_THREAD_STATE_RETURN(threadId, func) \
do { \
    auto state = ThreadState::DISABLED; \
    threadStateMap_.GetValue((threadId), state); \
    if (state == ThreadState::NOT_SWITCH_THREAD) { \
        func(); \
    } \
    NL_CHECK_RETURN(state == ThreadState::ENABLED, \
        "threadId %{public}s is no enabled", GetThreadName(threadId).c_str()); \
} while (0)

void ThreadUtil::PostTask(int threadId, const ThreadUtilFunc &func, uint64_t delayTime, const std::string &name)
{
    // Check thread state for unit test.
    CHECK_THREAD_STATE_RETURN(threadId, func);

    std::shared_ptr<impl::TaskQueue> taskQueue = nullptr;
    {
        std::lock_guard<ffrt::mutex> lock(pimpl->mutex_);
        if (pimpl->taskQueueMap_.GetValue(threadId, taskQueue) && taskQueue != nullptr) {
            taskQueue->PostTask(func, delayTime, name);
            return;
        }
        // If the thread not found, create it.
        taskQueue = pimpl->CreateTaskQueue(threadId);
    }
    // Execute the first task.
    if (taskQueue) {
        taskQueue->PostTask(func, delayTime, name);
    }
}

void ThreadUtil::RemoveTask(int threadId, const std::string &name)
{
    std::shared_ptr<impl::TaskQueue> taskQueue = nullptr;
    if (pimpl->taskQueueMap_.GetValue(threadId, taskQueue) && taskQueue != nullptr) {
        taskQueue->RemoveTask(name);
        return;
    }
}

std::shared_ptr<ThreadUtil::impl::TaskQueue> ThreadUtil::impl::CreateTaskQueue(int threadId)
{
    std::string threadName = GetThreadName(threadId);
    auto taskQueue = std::make_shared<TaskQueue>(threadName.c_str());
    NL_CHECK_RETURN_RET(taskQueue, nullptr, "Create %{public}s TaskQueue failed", threadName.c_str());

    int queueId = taskQueue->GetQueueId();
    HILOGI("sle_ffrt_queue: queueId(%{public}d),  name(%{public}s)", queueId, threadName.c_str());

    taskQueueMap_.EnsureInsert(threadId, taskQueue);
    return taskQueue;
}

ThreadUtil &ThreadUtil::GetInstance()
{
    static ThreadUtil instance;
    return instance;
}
}  // namespace Nearlink
}  // namespace OHOS
