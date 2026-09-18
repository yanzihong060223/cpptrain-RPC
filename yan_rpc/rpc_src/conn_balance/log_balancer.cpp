#include "log_balancer.h"
#include "../../log_manager.h"
#include "vote.h"
#include "weight.h"
#include "random.h"

#include <functional>
#include <memory>
#include <unordered_map>
#include <string>
#include <mutex>
#include <stdexcept>
namespace yan_rpc {
std::shared_ptr<LogBalancer> LogBalancer:: instance_ = nullptr;
std::mutex LogBalancer::mtx_;
class LogBalancerFactor {
public:
    LogBalancerFactor() = default;
    ~LogBalancerFactor() = default;
    static std::shared_ptr<LogBalancer> CreateVote() {
        return std::make_shared<Vote> ();
    }
    static std::shared_ptr<LogBalancer> CreateWeight() {
        return std::make_shared<Weight> ();
    }
    static std::shared_ptr<LogBalancer> CreateRandom() {
        return std::make_shared<Random> ();
    }
    static const std::unordered_map<std::string, std::function<std::shared_ptr<LogBalancer>()>>& GetMap() {
        static const std::unordered_map<std::string, std::function<std::shared_ptr<LogBalancer>()>> m {{"vote", CreateVote },
    {"weight",CreateWeight}, {"random", CreateRandom}};
       
        return m;
    }
}; //工厂模式
std::shared_ptr<LogBalancer> LogBalancer:: GetInstance() {
    std::lock_guard<std::mutex> m(mtx_);
    if (instance_ == nullptr) {
        instance_ = LogBalancerFactor::CreateRandom(); //默认
    }
    return instance_; // 锁内拷贝，引用计数 +1
}
std::string LogBalancer:: SelectServer(const std::vector<std::string>& servergroups) {
    if (servergroups.empty()) {
         LOGGER_ERROR("NO SERVER");
         return "";
    }
    try {
        return GetInstance()->Select(servergroups);
    } catch(const std::exception& e) {
         LOGGER_ERROR("SELECT ERROR {}", e.what());
         return "";
    }
}
bool LogBalancer::Init(const std::string type) {
   std::lock_guard<std::mutex> lm(mtx_);
   const std::unordered_map<std::string, std::function<std::shared_ptr<LogBalancer>()>> m = LogBalancerFactor::GetMap();
   auto it = m.find(type);
   if (it != m.end()) {
    try {
        instance_ = it->second();
        return true;
    } catch(const std::exception& e) {
         LOGGER_ERROR("FAILED TO INITIAILZE: {}", e.what());
         LOGGER_WARN("BACK TO RAND");
         instance_ = LogBalancerFactor::CreateRandom();
         return false;
    }
   }
    LOGGER_ERROR("NO TYPE");
    LOGGER_WARN("BACK TO RAND");
    instance_ = LogBalancerFactor::CreateRandom();
    return false;
}
} // namespace yan_rpc