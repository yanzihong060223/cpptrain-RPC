#pragma once
#include "../../log_manager.h"
#include "service.h"
#include "thread_pool_single.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <future>
namespace yan_rpc {
class ServiceManager{
public:
static ServiceManager& GetInstance() {
    static ServiceManager instance_;
    return instance_;
}
ServiceManager(const ServiceManager&) = delete;
ServiceManager& operator= (const ServiceManager&) = delete;
ServiceManager(ServiceManager&&) = delete;
ServiceManager& operator= (ServiceManager&&) = delete;
bool RegisterService(std::shared_ptr<Service> service);
std::future<bool> HandleRpcRequestAsync(std::string service_name, std::string method_name, std::shared_ptr<std::string> result, std::string args);
bool HandleRpcRequest(std::string service_name, std::string method_name, std::string& result, std::string args);
std::shared_ptr<Service> GetService(const std::string& service_name);
ThreadPool::Stat GetThreadPoolStat();
private:
ServiceManager() = default;
mutable std::mutex mtx_;
bool HandleRequest(std::string service_name, std::string method_name, std::shared_ptr<std::string> result, std::string args);
std::unordered_map<std::string, std::shared_ptr<Service>> services_;

};
} //namespace yan_rpc