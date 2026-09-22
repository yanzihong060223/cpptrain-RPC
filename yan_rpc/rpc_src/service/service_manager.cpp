#include "service_manager.h"
#include "../../log_manager.h"

#include <string>
#include <mutex>
namespace yan_rpc {
bool ServiceManager::RegisterService(std::shared_ptr<Service> service) {
    if (! service) {
        LOGGER_ERROR("No Service");
        return false;
    }
    std::string service_name = service->GetService();
    std::lock_guard<std::mutex> im(mtx_);
    if (services_.find(service_name) != services_.end()) {
        LOGGER_WARN("Service Exists");
        return false;
    }
    services_[service_name] = service;
    return true;
}
std::shared_ptr<Service> ServiceManager:: GetService(const std::string& service_name) {
    std::lock_guard<std::mutex> lm(mtx_);
    if (services_.find(service_name) == services_.end()) {
        LOGGER_ERROR("No Service");
        return nullptr;
    } 
    return services_[service_name];
}
bool ServiceManager::HandleRequest(std::string service_name, std::string method_name, std::shared_ptr<std::string> result, std::string args) {
        auto ptr = GetService(service_name);
        if (ptr == nullptr) {
            LOGGER_ERROR("No Service");
            return false;
        }
        if (!ptr->HandleService(method_name, *result, args)) {
            LOGGER_ERROR("Handle Request Error");
            return false;
        }
        LOGGER_INFO("Handle Request Success Service {}, method {}", service_name, method_name);
        return true;
}
bool ServiceManager:: HandleRpcRequest(std::string service_name, std::string method_name, std::string& result, std::string args)
{
    auto result_ptr = std::make_shared<std::string>(result);
    bool ok = HandleRequest(service_name, method_name, result_ptr, args);
    result = *result_ptr;
    return ok;
}
std::future<bool> ServiceManager:: HandleRpcRequestAsync(std::string service_name, std::string method_name, std::shared_ptr<std::string> result, std::string args) {
    if (!ThreadPoolSingle::IsInitialized()) {
        ThreadPoolSingle::Init();
        LOGGER_INFO("Init ThreadPool");
    }
    return ThreadPoolSingle::Enqueue(TaskPriority::HIGH, [this, service_name, method_name, result, args](){return this->HandleRequest(service_name, method_name, result, args);});
}
ThreadPool::Stat ServiceManager::GetThreadPoolStat() {
    return ThreadPoolSingle::GetStat();
}
}// namespace yan_rpc