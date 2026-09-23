#include "threadpool_config.h"
#include "../../log_manager.h"

#include <thread>

namespace yan_rpc {
bool ThreadPoolConfigLoader::Init(const nlohmann::json& config) {
    try {
        SetterCoreThreads(config.value("core_threads", static_cast<size_t>(std::thread::hardware_concurrency())));
        SetterMaxCoreThreads(config.value("max_core_threads", static_cast<size_t>(std::thread::hardware_concurrency() * 2)));
        if (core_threads_ > max_core_threads_) {
            LOGGER_ERROR("CORE THREADS CAN NOT Greater than MAX CORE THREADS");
            return false;
        }
        if (max_core_threads_ == 0) {
             LOGGER_ERROR("MAX CORE THREADS CAN NOT BE ZERO");
             return false;
        }
        SetterMaxQueueSize(config.value("max_queue_size", static_cast<size_t>(0)));
        SetterAliveTime(std::chrono::milliseconds(config.value("alive_time", 8000)));
        if (keep_alive_time_.count() == 0) {
            LOGGER_ERROR("ALIVE TIME CAN NOT BE ZERO");
            return false;
        }
        LOGGER_INFO("THREAD POOL CONFIG SUCCESS CORE THREADS {} // MAX CORE THREADS {} // MAXQUEUESIZE {}//ALIVE TIME{}", core_threads_, max_core_threads_,  max_queue_size_, keep_alive_time_.count());
        return true;
    }catch(const nlohmann::json::exception& e) {
        LOGGER_ERROR("FAILED TO CONFIG THREADPOOL {}", e.what());
        return false;
    }
}

}//namespace yan_rpc
