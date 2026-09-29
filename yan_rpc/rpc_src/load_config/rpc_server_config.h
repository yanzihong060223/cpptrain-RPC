#pragma once

#include "logger_config.h"
#include "service_register_config.h"
#include "threadpool_config.h"

#include <cstdint>
#include <string>

namespace yan_rpc {

class RpcServerConfig {
public:
    static RpcServerConfig& GetInstance()
    {
        static RpcServerConfig instance;
        return instance;
    }

    bool Init(const std::string& config_file);

    RpcServerConfig(const RpcServerConfig&) = delete;
    RpcServerConfig& operator=(const RpcServerConfig&) = delete;
    RpcServerConfig(RpcServerConfig&&) = delete;
    RpcServerConfig& operator=(RpcServerConfig&&) = delete;

    void SetServiceIp(const std::string& service_ip)
    {
        service_ip_ = service_ip;
    }

    void SetServicePort(std::uint32_t service_port)
    {
        service_port_ = service_port;
    }

    void SetRetryTime(int retry_time)
    {
        retry_time_ = retry_time;
    }

    void SetTimeoutMs(std::uint32_t timeout_ms)
    {
        timeout_ms_ = timeout_ms;
    }

    void SetMaxServiceConnections(int max_connections)
    {
        max_service_connections_ = max_connections;
    }

    const std::string& GetServiceIp() const
    {
        return service_ip_;
    }

    std::uint32_t GetServicePort() const
    {
        return service_port_;
    }

    int GetRetryTime() const
    {
        return retry_time_;
    }

    std::uint32_t GetTimeoutMs() const
    {
        return timeout_ms_;
    }

    int GetMaxServiceConnections() const
    {
        return max_service_connections_;
    }

    const std::string& GetLoadBalanceStrategy() const
    {
        return load_balance_strategy_;
    }

    const ThreadPoolConfigLoader& GetThreadPoolConfig() const
    {
        return ThreadPoolConfigLoader::GetInstance();
    }

    const RegisterConfig& GetRegisterConfig() const
    {
        return RegisterConfig::GetInstance();
    }

    const LoggerConfig& GetLoggerConfig() const
    {
        return LoggerConfig::GetInstance();
    }

private:
    RpcServerConfig() = default;
    ~RpcServerConfig() = default;

    std::string service_ip_{"0.0.0.0"};
    std::uint32_t service_port_{8989};
    int retry_time_{3};
    std::uint32_t timeout_ms_{3000};
    int max_service_connections_{1000};
    std::string load_balance_strategy_{"random"};
};

} // namespace yan_rpc
