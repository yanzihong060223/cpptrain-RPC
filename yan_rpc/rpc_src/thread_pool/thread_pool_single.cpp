#include "thread_pool_single.h"

namespace yan_rpc {

std::mutex ThreadPoolSingle::mtx_;
std::unique_ptr<ThreadPool> ThreadPoolSingle::instance_;

bool ThreadPoolSingle::Init(size_t thread_size) {
    std::lock_guard<std::mutex> lock(mtx_);

    if (instance_) {
        return false;
    }

    instance_ = std::make_unique<ThreadPool>(thread_size);
    return true;
}

bool ThreadPoolSingle::Init(const ThreadPoolConfig& config) {
    std::lock_guard<std::mutex> lock(mtx_);

    if (instance_) {
        return false;
    }

    instance_ = std::make_unique<ThreadPool>(config);
    return true;
}

ThreadPool& ThreadPoolSingle::GetInstance() {
    std::lock_guard<std::mutex> lock(mtx_);

    if (!instance_) {
        instance_ = std::make_unique<ThreadPool>(std::thread::hardware_concurrency());
    }

    return *instance_;
}

ThreadPool& ThreadPoolSingle::GetInstance(
    const ThreadPoolConfig& config
) {
    std::lock_guard<std::mutex> lock(mtx_);

    if (!instance_) {
        instance_ = std::make_unique<ThreadPool>(config);
    }

    return *instance_;
}
//
bool ThreadPoolSingle::Pause() {
    ThreadPool* pool = nullptr;

    {
        std::lock_guard<std::mutex> lock(mtx_);
        pool = instance_.get();
    }

    if (!pool) {
        return false;
    }

    pool->Pause();
    return true;
}

bool ThreadPoolSingle::Resume() {
    ThreadPool* pool = nullptr;

    {
        std::lock_guard<std::mutex> lock(mtx_);
        pool = instance_.get();
    }

    if (!pool) {
        return false;
    }

    pool->Resume();
    return true;
}

bool ThreadPoolSingle::ShutDown(std::chrono::milliseconds timeout) {
    ThreadPool* pool = nullptr;

    {
        std::lock_guard<std::mutex> lock(mtx_);
        pool = instance_.get();
    }

    if (!pool) {
        return false;
    }

    return pool->ShutDown(timeout);
}

void ThreadPoolSingle::Destroy() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!instance_) {
        return;
    }
    instance_->Stop();
    instance_.reset();
}

size_t ThreadPoolSingle::WorkerSize() {
    ThreadPool* pool = nullptr;

    {
        std::lock_guard<std::mutex> lock(mtx_);
        pool = instance_.get();
    }

    return pool ? pool->WorkerSize() : 0;
}

size_t ThreadPoolSingle::TaskSize() {
    ThreadPool* pool = nullptr;

    {
        std::lock_guard<std::mutex> lock(mtx_);
        pool = instance_.get();
    }

    return pool ? pool->TasksSize() : 0;
}

bool ThreadPoolSingle::IsInitialized() {
    std::lock_guard<std::mutex> lock(mtx_);
    return static_cast<bool>(instance_);
}

ThreadPool::Stat ThreadPoolSingle::GetStat() {
    ThreadPool* pool = nullptr;

    {
        std::lock_guard<std::mutex> lock(mtx_);
        pool = instance_.get();
    }

    return pool ? pool->GetStat() : ThreadPool::Stat{};
}

} // namespace yan_rpc
