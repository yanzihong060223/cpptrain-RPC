#pragma once
#ifdef __cplusplus
extern "C" {
#endif
#include <zookeeper/zookeeper.h>
#include <zookeeper/zookeeper_version.h>
#include <zookeeper/proto.h>
#ifdef __cplusplus
}
#endif

#include "../../log_manager.h"
#include "../register/service_register.h"
#include "../load_config/service_register_config.h"

#include <vector>
#include <mutex>
#include <atomic>
#include <nlohmann/json.hpp>
#include <string>
#include <memory>
namespace yan_rpc {
class ZkHandler {
public:
static ZkHandler& GetInstance() {
    static ZkHandler zkhandler;
    return zkhandler;
}
//初始化 按配置启动zookeeper集群
bool Init(const nlohmann:: json& config);
std::vector<std::string> GetAllServers(const std::string& zk_namespace); //获取所有活实例
std::string GetServer(const std::string& zk_namespace); //获取一个实例
bool RegisterService(const std::string&path, const std::string& address);
bool RegisterServiceConfig(const RegisterConfig& service_config); //按照配置模块注册
bool EnsureConnect(); //确保连接
void UpdateServer(const std::string& zk_namespace); //更新server数组
ServiceRegister* GetServiceRegisterPtr() {
    return service_register_.get();
}
const ServiceRegister* GetServiceRegisterPtr() const {
    return service_register_.get();
}
void SetZooHost(const std::string host);
void SetZooPort(int port) ;
void SetZooNameSpace(std::string zk_namespace);
void clean_up(); //清空
~ZkHandler();
ZkHandler (const ZkHandler&) = delete;
ZkHandler& operator= (const ZkHandler&) = delete;
ZkHandler(ZkHandler&&) = delete;
ZkHandler& operator= (ZkHandler&&) = delete;
private:
ServiceRegister* GetServiceRegister();
ZkHandler() : zoo_client_(nullptr), service_register_(nullptr) {}
static void Watcher(  zhandle_t* zh,
    int type,
    int state,
    const char* path,
    void* watcherCtx); //全局观察函数
bool CreateServiceRegistryIfNeeded();
std::string GetConnectString();
void CleanUpLocked(); //调用 locked对象 必须已经拿到connect_mutex_;
bool EnsureConnectLocked();
private:
// 三把锁各管一段。实际加锁顺序 server_mtx_ -> mtx_ -> connect_mtx_，禁止反向嵌套
std::mutex mtx_;//成员锁：zk_namespace_ / zoo_host_ / zoo_port_ / service_register_
std::mutex server_mtx_;//server_锁
std::mutex connect_mtx_;//zoo_client_ 专用：建连 读取 关闭都走这把
std::mutex server_register_mtx_; //服务注册锁
zhandle_t* zoo_client_;//客户端句柄
std::unique_ptr<ServiceRegister> service_register_; //服务注册
std::string zk_namespace_; //作用域
std::vector<std::string> servers_;//节点数组
std::string zoo_host_;
int zoo_port_;
std::atomic<bool> session_failed_{false};//会话失效标记
std::atomic<bool> inited_{false};

};
} //namespace yan_rpc
