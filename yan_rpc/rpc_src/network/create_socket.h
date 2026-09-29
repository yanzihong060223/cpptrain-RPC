#pragma once
#include "../../log_manager.h"
#include "../load_config/rpc_server_config.h"
#include <sys/socket.h>
#include <netinet/tcp.h>

#include <string>
#include <cstdint>
#include <atomic>
#include <memory>
namespace yan_rpc {
class CreateSocket {
public:
    static std::shared_ptr<CreateSocket> Create(const std::string& service_ip, uint32_t service_port,
        int retry_times, int max_service_connections);
    static std::shared_ptr<CreateSocket> Create(const RpcServerConfig& Config);
    int GetFd() const { return fd_; }
    std::string GetServiceIp() const { return service_ip_; }
    uint32_t GetServicePort() const { return service_port_; }
    ~CreateSocket() = default;
private:
    bool Init(); //初始化fd bind listen
    bool SetSocketOpt(); //设置 服务端fd
    CreateSocket(const std::string& service_ip, uint32_t service_port,
        int retry_times, int max_service_connections);
    CreateSocket(const RpcServerConfig& Config);




private:
std::string service_ip_ ;//IP
uint32_t service_port_;// 端口
int retry_times_; //重连时间
int fd_;// socket 句柄
int max_service_connections_;// 最大连接次数
std::atomic<bool> is_init_{false};
};
} // namespace yan_rpc
