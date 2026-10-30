#pragma once
#include <deque>
#include <functional>
#include <cstdint>
#include <algorithm>
#include <string>
namespace OHOS::Nearlink {
inline std::deque<std::function<void()>> tasks;
struct DelayedTask {
    std::string name;
    std::function<void()> callback;
    void operator()() const { callback(); }
};
inline std::deque<DelayedTask> delayedTasks;
enum { THREAD_ID_IPSHARE };
class ThreadUtil {
public:
    static ThreadUtil &GetInstance() { static ThreadUtil instance; return instance; }
    void PostTask(int, const std::function<void()> &fn, uint64_t delayTime = 0, const std::string &name = "")
    {
        if (delayTime == 0) { tasks.push_back(fn); return; }
        // Match production replacement-by-name behavior, including the empty default name.
        delayedTasks.erase(std::remove_if(delayedTasks.begin(), delayedTasks.end(),
            [&](const auto &task) { return task.name == name; }), delayedTasks.end());
        delayedTasks.push_back({name, fn});
    }
};
inline void DoInIpShareThread(std::function<void()> fn, uint64_t delayTime = 0)
{
    ThreadUtil::GetInstance().PostTask(THREAD_ID_IPSHARE, fn, delayTime);
}
inline void DrainTasks() { while (!tasks.empty()) { auto fn=tasks.front(); tasks.pop_front(); fn(); } }
}
