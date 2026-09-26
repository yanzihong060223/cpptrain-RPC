#include "rpc_server_config.h"

#include "../conn_balance/log_balancer.h"
#include "../conn_balance/zk_conn_handler.h"
#include "../../log_manager.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <string>

namespace yan_rpc {

bool RpcServerConfig::Init(const std::string& config_file)
{
    try {
        std::ifstream file(config_file);
        if (!file.is_open()) {
            LOGGER_ERROR("Failed to open server config: {}", config_file);
            return false;
        }

        const nlohmann::json config =
            nlohmann::json::parse(file);

        const std::string service_ip = config.value(
            "service_ip",
            config.value("servers_ip", std::string("0.0.0.0"))
        );
        const int service_port = config.value(
            "service_port",
            config.value("servers_port", 8989)
        );
        const int retry_time = config.value(
            "retry_time",
            config.value("retry_times", 3)
        );
        const int timeout_ms = config.value(
            "timeout_ms",
            config.value("timeout", 3000)
        );
        const int max_connections = config.value(
            "max_service_connections",
            config.value("max_connections", 1000)
        );
        const std::string load_balance_strategy = config.value(
            "load_balance_strategy",
            std::string("random")
        );

        if (service_ip.empty()) {
            LOGGER_ERROR("Service IP cannot be empty");
            return false;
        }
        if (service_port <= 0 || service_port > 65535) {
            LOGGER_ERROR("Invalid service port: {}", service_port);
            return false;
        }
        if (retry_time < 0) {
            LOGGER_ERROR("Retry time cannot be negative: {}", retry_time);
            return false;
        }
        if (timeout_ms <= 0) {
            LOGGER_ERROR("Timeout must be greater than zero: {}", timeout_ms);
            return false;
        }
        if (max_connections <= 0) {
            LOGGER_ERROR(
                "Max service connections must be greater than zero: {}",
                max_connections
            );
            return false;
        }

        SetServiceIp(service_ip);
        SetServicePort(static_cast<std::uint32_t>(service_port));
        SetRetryTime(retry_time);
        SetTimeoutMs(static_cast<std::uint32_t>(timeout_ms));
        SetMaxServiceConnections(max_connections);
        load_balance_strategy_ = load_balance_strategy;

        if (config.contains("logger")) {
            if (!config["logger"].is_object()) {
                LOGGER_ERROR("logger must be a JSON object");
                return false;
            }
            if (!LoggerConfig::GetInstance().Init(config["logger"])) {
                LOGGER_ERROR("Failed to initialize logger config");
                return false;
            }
        }

        if (config.contains("thread_pool")) {
            if (!config["thread_pool"].is_object()) {
                LOGGER_ERROR("thread_pool must be a JSON object");
                return false;
            }
            if (!ThreadPoolConfigLoader::GetInstance().Init(
                    config["thread_pool"])) {
                LOGGER_ERROR("Failed to initialize thread pool config");
                return false;
            }
        }

        if (config.contains("register_config")) {
            if (!config["register_config"].is_string()) {
                LOGGER_ERROR("register_config must be a file path");
                return false;
            }

            const std::string register_config_file =
                config["register_config"].get<std::string>();
            if (!RegisterConfig::GetInstance().Init(
                    register_config_file)) {
                LOGGER_ERROR("Failed to initialize register config");
                return false;
            }
        }

        if (config.contains("scheduler")) {
            if (!config["scheduler"].is_object()) {
                LOGGER_ERROR("scheduler must be a JSON object");
                return false;
            }
            if (!ZkHandler::GetInstance().Init(config["scheduler"])) {
                LOGGER_ERROR("Failed to initialize ZooKeeper handler");
                return false;
            }
        }

        if (!LogBalancer::Init(load_balance_strategy_)) {
            LOGGER_ERROR(
                "Invalid load balance strategy: {}",
                load_balance_strategy_
            );
            return false;
        }

        LOGGER_INFO(
            "Server config initialized: {}:{}, max connections: {}, "
            "retry time: {}, timeout: {} ms, load balance: {}",
            service_ip_,
            service_port_,
            max_service_connections_,
            retry_time_,
            timeout_ms_,
            load_balance_strategy_
        );
        return true;
    } catch (const nlohmann::json::exception& error) {
        LOGGER_ERROR("Invalid server config JSON: {}", error.what());
        return false;
    } catch (const std::exception& error) {
        LOGGER_ERROR("Initialize server config failed: {}", error.what());
        return false;
    }
}

} // namespace yan_rpc
