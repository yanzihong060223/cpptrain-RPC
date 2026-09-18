//轮询模块的实现
#pragma once
#include "../../log_manager.h"
#include "log_balancer.h"

#include <atomic>
#include <string>
#include <vector>
namespace yan_rpc {
class Vote : public LogBalancer {
public:
Vote() : idex_(0) {}
~Vote() = default;
const std::string Select(const std::vector<std::string>& servers) override {
    if (servers.empty()) {
        LOGGER_WARN("NO SERVER");
        return "";
    } else {
        const std::string temp = servers [(idex_.fetch_add(1)) % servers.size()];
        return temp;
    }
}
private:
std::atomic<int> idex_;
};
} // namespace yan_rpc