#pragma once
#include "../../log_manager.h"

#include <string>
#include <atomic>

#include <zookeeper/zookeeper.h>
class ServiceRegister {
public:
    explicit ServiceRegister(const std::string& zoo_host);//zookeeper的注册
    ~ServiceRegister();
    bool RegisterService(const std::string& service_path, const std::string& service_address);//服务注册
    bool IsConnect();
    ServiceRegister(const ServiceRegister&) = delete;
    ServiceRegister& operator=(const ServiceRegister&) = delete;
    ServiceRegister( ServiceRegister&&) = delete;
    ServiceRegister& operator=(ServiceRegister&&) = delete;
  
private:
static void Watcher(  zhandle_t* zh,
    int type,
    int state,
    const char* path,
    void* watcherCtx);//全局观察函数
bool EnsurePath(const std::string& path); //确保父路径
bool CreateNode(const std::string& address, const std::string& data);
private:
static const std::string Root_Path_;
std::atomic<bool> is_connected_;
zhandle_t* zoo_handle_;//zookeeper句柄

};
