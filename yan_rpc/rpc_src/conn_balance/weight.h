#pragma once
#include "log_balancer.h"
#include "../../log_manager.h"

#include <unordered_map>
#include <vector>
#include <atomic>
#include <string>
namespace yan_rpc {
class Weight : public LogBalancer {
public:
Weight() : idex_(0) {}
~Weight() = default;
const std::string Select(const std::vector<std::string>& servers) override {
    if (servers.empty()) {
         LOGGER_WARN("NO SERVER");
        return "";
    } else {
        std::vector<int> weights(servers.size(), 1); //临时存储权重 应该由配置模块配置 这里简略
        int sum_weight = 0;
        for (auto p : weights) {
            sum_weight += p;
        }
         idex_ = (idex_ + 1) % sum_weight;
         int current_weight = 0;
        for (int i = 0; i < servers.size(); i++) {
            current_weight += weights[i];
            if (idex_ < current_weight) {
                return servers[i];
            }
        }
        LOGGER_WARN("WeightedRoundRobin: no instance matched the ticket");
        return servers[0];
    }
}

private:
std::unordered_map<std::string, int> weight_map_;
void SetWeight(const std::string& s, int weight) {
    weight_map_[s] = weight;
}
std::atomic<int> idex_;
};
} // namespace yan_rpc