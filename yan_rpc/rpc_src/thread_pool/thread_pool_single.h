#pragma once

#include "thread_pool.h"

#include <chrono>
#include <memory>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>
#include <atomic>

namespace yan_rpc {

class ThreadPoolSingle {
public:
    // 第一次初始化的配置生效，之后再次 Init 返回 false//。
    static bool Init(
        size_t thread_size = std::thread::hardware_concurrency()
    );
    static bool Init(const ThreadPoolConfig& config);

    // 未初始化时使用默认线程数创建线程池。
    static ThreadPool& GetInstance();
    static ThreadPool& GetInstance(const ThreadPoolConfig& config);

    static bool Pause();
    static bool Resume();

    static bool ShutDown(
        std::chrono::milliseconds timeout =
            std::chrono::milliseconds::max()
    );

    // 仅在程序退出流程、确认没有其他线程使用线程池时调用//
    static void Destroy();

    static size_t WorkerSize();
    static size_t TaskSize();
    static bool IsInitialized();
    static ThreadPool::Stat GetStat();

    template<typename F, typename... Args>
    static auto Enqueue(TaskPriority priority, F&& f, Args&&... args)
        -> std::future<typename std::invoke_result<F, Args...>::type> {
        return GetInstance().Enquene(
            priority,
            std::forward<F>(f),
            std::forward<Args>(args)...
        );
    }
private:
    ThreadPoolSingle() = delete;
    ~ThreadPoolSingle() = delete;

    static std::mutex mtx_;
    static std::unique_ptr<ThreadPool> instance_;
};

} // namespace yan_rpc
