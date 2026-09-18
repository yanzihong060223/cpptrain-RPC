#pragma once
#include "log_balancer.h"
#include "../../log_manager.h"

#include <random>
#include <atomic>
#include <mutex>
namespace yan_rpc {
class Random : public LogBalancer{
public:
    Random() : rd_(), gen_(rd_()) {}
    ~Random() = default;
    const std::string Select(const std::vector<std::string>& servers) override {
        if (servers.empty()) {
            LOGGER_WARN("NO SERVER");
            return "";
        } else {
            std::uniform_int_distribution<std::size_t> dist(0, servers.size() - 1);
            std::lock_guard<std::mutex> lm(mtx_);
            return servers[dist(gen_)];
        }
    }

private:
std::random_device rd_; //初始种子
std::mt19937 gen_;//初始引擎
std::mutex mtx_;
};
} // namespace yan_rpc