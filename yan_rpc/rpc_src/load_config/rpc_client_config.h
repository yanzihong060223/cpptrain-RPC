#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <cstdint>
/* {
    "zk_host": "localhost",               // ZooKeeper主机
    "zk_port": 2181,                      // ZooKeeper端口
    "zk_namespace": "/cookrpc/cookrpc_service",  // 服务命名空间
    "server_port": 8989,                  // 服务器端口
    "timeout_ms": 3000,                   // 超时时间(毫秒)
    "retry_times": 3                      // 重试次数
    */
namespace yan_rpc {
class RpcServiceConfig {
public:
static RpcServiceConfig& GetInstance() {
    static RpcServiceConfig instance;
    return instance;
}
bool Init (const std::string& filename);
void SetterZkHost(const std::string& zk_host) {
    zk_host_ = zk_host;
}
void SetterZkNameSpace(const std::string& zk_namespace) {
    zk_namespace_ = zk_namespace;
}
void SetterZkPort(uint32_t zk_port) {
    zk_port_ = zk_port;
}
void SetterServerPort(uint32_t server_port) {
    server_port_ = server_port;
}
void SetterTimeoutMs(int timeout_ms) {
    timeout_ms_ = timeout_ms;
}
void SetterRetryTimes(int retry_times) {
    retry_times_ = retry_times;
}
std::string GetZkHost() const {
    return zk_host_;
}
std::string GetZkNameSpace() const {
    return zk_namespace_;
}
uint32_t GetZkPort() const {
    return zk_port_;
}
uint32_t GetServerPort() const {
    return server_port_;
}
int GetTimeoutMs() const {
    return timeout_ms_;
}
int GetRetryTimes() const {
    return retry_times_;
}
private:
~RpcServiceConfig() = default;
RpcServiceConfig() = default;
private:
std::string zk_host_; //zooKeeper主机
std::string zk_namespace_; //服务命名空间
uint32_t zk_port_; //zookeepr端口
uint32_t server_port_;
int timeout_ms_ = 3000; //默认3000
int retry_times_ =3; //重试次数默认三次
};
} //namespace yan_rpc