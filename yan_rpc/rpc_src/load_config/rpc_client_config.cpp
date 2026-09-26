#include "rpc_client_config.h"
#include "../../log_manager.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <string>
#include <cstdint>
#include <stdexcept>
namespace yan_rpc {
bool RpcServiceConfig ::Init (const std::string& filename) {
    try {
    std::ifstream file(filename);
    if (!file.is_open()) {
        LOGGER_ERROR("File Open Failed");
        return false;
    }
    nlohmann::json j = nlohmann::json::parse(file);
    SetterZkHost(j.value("zk_host", "local_host"));
    SetterZkNameSpace(j.value("zk_namespace", "/yan_rpc"));
    SetterZkPort(j.value("zk_port", 2181));
    SetterServerPort(j.value("server_port", 8989));
    SetterTimeoutMs(j.value("timeout_ms", 3000));
    SetterRetryTimes(j.value("retry_times", 3));
    if (zk_namespace_.empty()) {
        LOGGER_ERROR(" No current NameSpace");
        return false;
    }
    if (zk_host_.empty()) {
        LOGGER_ERROR("No Current Zkhost");
        return false;
    }
    if (zk_port_ < 0 || zk_port_ > 65535) {
        LOGGER_ERROR("No Current ZkPort");
        return false;
    }
    if (server_port_ < 0 || server_port_ > 65535) {
        LOGGER_ERROR("No Current Server Port");
        return false;
    }
    if (timeout_ms_ == 0) {
        LOGGER_ERROR("No Current Timeoutms");
        return false; 
    }
    if (retry_times_ == 0) {
        LOGGER_ERROR("No Current Retry Time");
        return false;
    }

    return true;
    } catch(const std::exception& e) {
        LOGGER_ERROR("Init Error {}", e.what());
        return false;
    }
}





}