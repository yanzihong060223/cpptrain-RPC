#include "service_register.h"

#include <string>
#include <stdexcept>
#include <iostream>
#include <chrono>
#include <thread>

const std::string ServiceRegister:: Root_Path_ = "/yanrpc";
ServiceRegister::ServiceRegister(const std::string& zoo_host) : is_connected_(false), zoo_handle_(nullptr) {

        zoo_handle_ = zookeeper_init(zoo_host.c_str(), Watcher, 30000, 0, this, 0);
        if (zoo_handle_ == nullptr) {
            std::cerr <<  "ZOOKEEPER INIT ERROR" << std::endl;
            throw std::runtime_error("INTI ERROR");
        }
        int temp = 0;
        int max_time = 10;
        while (temp < max_time && !is_connected_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            temp++;
        }
        if (!is_connected_) {
              zookeeper_close(zoo_handle_);
            throw std::runtime_error("TIME OUT");
    
    }
}
void ServiceRegister:: Watcher (  zhandle_t* zh,
    int type,
    int state,
    const char* path,
    void* watcherCtx) {
          if (type == ZOO_SESSION_EVENT) {
        auto res = static_cast<ServiceRegister*>(watcherCtx);
        if (state == ZOO_CONNECTED_STATE) {
            res->is_connected_ = true;
        } else {
             res->is_connected_ = false;
        }
    }

}
ServiceRegister::  ~ServiceRegister() {
    try {
        if (zoo_handle_) {
            zookeeper_close(zoo_handle_);
            zoo_handle_ = nullptr;
        }
    } catch(const std::exception& e) {
        std::cout << e.what() << std::endl;
        zoo_handle_ = nullptr;
    }
}
bool ServiceRegister::EnsurePath(const std::string& path) {
    if (! zoo_handle_ || !is_connected_) {
        std::cout << " NOT CONNECT" << std::endl;
        return false;
    }
    if (! path.empty()) {
       size_t res = path.find_last_of('/');
       if (res != std::string ::npos && res > 0) {
            std::string parent = path.substr(0, res);
            if (!parent.empty() && !EnsurePath(parent)) {
                LOGGER_ERROR("FAILED TO CREATE");
                return false;
            } //递归确保父路径存在
       }
       struct Stat stat{};
       int rc =  zoo_exists(zoo_handle_, path.c_str(), 0, nullptr);
       if (rc == ZOK) {
          LOGGER_WARN("NODE EXISTS");
        return true;
       } else if (rc == ZNONODE) {
            size_t res = zoo_create(zoo_handle_, path.c_str(), "", 0, &ZOO_OPEN_ACL_UNSAFE, 0, nullptr, 0);//父目录创建=固定节点
            if (res == ZOK || res == ZNODEEXISTS) {
                return true;
            }
       }
       return false;
    }
    return false;
} //确保路径存在
bool ServiceRegister:: CreateNode(const std::string& address, const std::string& data) {
    if (! zoo_handle_ || !is_connected_) {
        LOGGER_ERROR("NO ZOOKEEPER SERVICE");
        return false;
    }
     struct Stat stat;
       int rc =  zoo_exists(zoo_handle_, address.c_str(), 0, &stat);
       if (rc == ZOK) {
          LOGGER_WARN("NODE EXISTS");
        return true;
       } else if(rc == ZNONODE) {
            int res = zoo_create(zoo_handle_, address.c_str(), data.c_str(), data.length(), &ZOO_OPEN_ACL_UNSAFE, ZOO_EPHEMERAL, nullptr, 0 );
            if (res == ZOK) {
                LOGGER_INFO("CREATE SUCCESS");
                return true;
            } else if( res == ZNODEEXISTS) {
                LOGGER_WARN("NODE EXISTS");
                return true;
            }
       }
       LOGGER_ERROR("CREATE FAIL");
    return false;
}
  bool ServiceRegister:: RegisterService(const std::string& service_path, const std::string& service_address) {
    if (! zoo_handle_ || !is_connected_) {
        LOGGER_ERROR("NO ZOOKEEPER SERVICE");
        return false;
    }
    std::string path = Root_Path_ + "/" + service_path;
    if (EnsurePath(path)) {
        LOGGER_INFO("PARENT PATH EXISTS");
        std::string instace_path = path + "/" + service_address;
        if (CreateNode(instace_path, service_address)) {
              LOGGER_INFO("REGISTER SUCCESS");
              return true;
        }
        LOGGER_ERROR("REGISTER FAIL");
        return false;
    }
      LOGGER_ERROR("NO PARENT PATH");
    return false;
   
}    //服务注册
bool ServiceRegister:: IsConnect() {
    return is_connected_ && zoo_handle_ != nullptr;
}