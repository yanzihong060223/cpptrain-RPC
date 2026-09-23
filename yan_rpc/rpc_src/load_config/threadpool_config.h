#pragma once
#include "../thread_pool/thread_pool.h"

#include <nlohmann/json.hpp>
#include <chrono>
#include <cstddef>

namespace yan_rpc {
class ThreadPoolConfigLoader {
public:
static ThreadPoolConfigLoader& GetInstance() {
    static ThreadPoolConfigLoader instance;
    return instance;
}
ThreadPoolConfigLoader (const ThreadPoolConfigLoader&) = delete;
ThreadPoolConfigLoader& operator= (const ThreadPoolConfigLoader&) = delete;
ThreadPoolConfigLoader (ThreadPoolConfigLoader&&) = delete;
ThreadPoolConfigLoader& operator= (ThreadPoolConfigLoader&&) = delete;
bool Init(const nlohmann::json& config);
void SetterCoreThreads(size_t core_threads){core_threads_ = core_threads;}
void SetterMaxCoreThreads(size_t max_core_threads){max_core_threads_ = max_core_threads;}
void SetterMaxQueueSize(size_t max_queue_size) {max_queue_size_ = max_queue_size;}
void SetterAliveTime(std::chrono::milliseconds alive_time){keep_alive_time_ = alive_time;}
size_t GetCoreThreads() const {return core_threads_;}
size_t GetMaxCoreThreads() const {return max_core_threads_;}
size_t GetMaxQueueSize() const {return max_queue_size_;}
std::chrono::milliseconds GetAliveTime() const {return keep_alive_time_;}
private:
size_t core_threads_ = 0;
size_t max_core_threads_ = 0;
size_t max_queue_size_ = 0;
std::chrono::milliseconds keep_alive_time_{0};
ThreadPoolConfigLoader() = default;
~ThreadPoolConfigLoader() = default;
};
} //namespace yan_rpc
