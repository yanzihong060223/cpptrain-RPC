#pragma once
#include "../../log_manager.h"

#include <vector>
#include <cstdint>
#include <string>

#include <nlohmann/json.hpp>
namespace yan_rpc {
    enum class BalancerStrateStrategy {
        Vote, //轮询
        Random, //随机
        Weight //带权
    };
struct RegisterInfo {
    std::string address;
    uint16_t port;
};
class RegisterConfig {
public:
static RegisterConfig& GetInstance () {
        static RegisterConfig instance;
        return instance;
    }
bool Init(const std::string& config_file);
RegisterConfig(const RegisterConfig&) = delete;
RegisterConfig& operator=(const RegisterConfig&) = delete;
RegisterConfig(RegisterConfig&&) = delete;
RegisterConfig& operator= (RegisterConfig&&) = delete;
void SetServiceName(const std::string& service_name) {service_name_ = service_name;};
void SetServiceVersion(const std::string& service_version) {service_version_ = service_version;};
std::string GetServiceName() const {return service_name_;};
std::string GetServiceVersion() const {return service_version_;};
size_t GetNodesSize() const {return register_nodes_.size();};
std::vector<RegisterInfo> GetRegisterNodes () const {return register_nodes_;};
private:
RegisterConfig() = default;
~RegisterConfig() = default;
std::string service_name_;
std::string service_version_;
std::vector<RegisterInfo> register_nodes_;

};
} //namespace yan_rpc