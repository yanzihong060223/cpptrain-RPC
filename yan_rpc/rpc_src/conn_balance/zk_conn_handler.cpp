#include "zk_conn_handler.h"
#include "../../log_manager.h"
#include "log_balancer.h"

#include <mutex>
#include <chrono>
#include <stdexcept>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <zookeeper/zookeeper.h>
#include <iostream>
#include <vector>
#include <utility>
namespace yan_rpc {
std::string ZkHandler:: GetConnectString() {
    std::lock_guard<std::mutex> lm(mtx_);
    return zoo_host_ + ":" + std::to_string(zoo_port_);
}
//必须已经拿到connect_mtx
void ZkHandler::CleanUpLocked() {
     zhandle_t* handle = std::exchange(zoo_client_, nullptr);

    if (handle == nullptr)
        return;

    int rc = zookeeper_close(handle);

    if (rc != ZOK)
    {
        LOGGER_ERROR("Failed to close ZooKeeper handle: {}", zerror(rc));
    }
}
bool ZkHandler:: EnsureConnectLocked() {
    if (session_failed_.load()) {
        LOGGER_ERROR("SESSION EXPIRED, NOT RECONNECTING");
        return false;
    }
    if(! zoo_client_) {
     std::string conn =
            zoo_host_ + ":" + std::to_string(zoo_port_);
    zoo_client_ = zookeeper_init(
    conn.c_str(),
    Watcher,
    30000,
    nullptr,
    this,
    0
    );
    if (zoo_client_ == nullptr) {
        LOGGER_ERROR("CONNECT ERROR");
        CleanUpLocked();
        return false;
    }
    int max_times = 50; //100ms 一次 共等 5 秒 session timeout 是 30 秒
    for (int retry_count = 0; retry_count < max_times; retry_count++) {
        int state = zoo_state(zoo_client_);
        if (state == ZOO_CONNECTED_STATE) {
            return true;
        }
        if (state == ZOO_EXPIRED_SESSION_STATE || state == ZOO_AUTH_FAILED_STATE) {
            LOGGER_ERROR("FAILED TO CONNECT {}", state);
            //已持 connect_mtx_ 不能调 clean_up 就地关
            zhandle_t* h = std::exchange(zoo_client_, nullptr);
            if (h) zookeeper_close(h);
            return false;
        } else {
            LOGGER_INFO("CONNECTING");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    LOGGER_ERROR("CONNECTION OUT");
    //已持 connect_mtx_ 不能调 clean_up 就地关
    CleanUpLocked();
    return false;

}
return true;
}
struct StringVectorGuard
{
    String_vector value{};

    ~StringVectorGuard()
    {
        deallocate_String_vector(&value);
    }
};
bool ZkHandler:: EnsureConnect() {
    std::lock_guard<std::mutex> lm(connect_mtx_);
    return EnsureConnectLocked();
}
bool ZkHandler:: Init(const nlohmann:: json& config) {
    inited_.store(false);
    try{
        if (config.empty()) {
            LOGGER_ERROR("CONFIG ERROR");
            return false;
        }
    try {
     std::string zoo_host = config.value("host", "localhost");// 获取失败取默认值
    int zoo_port = config.value("port", 2181);
    std::string zoo_namespace = config.value("zoo_namespace", "/yan_rpc");
    {
        std::lock_guard<std::mutex> lm (mtx_);
        zoo_host_ = zoo_host;
        zoo_port_ = zoo_port;
        zk_namespace_ = zoo_namespace;

    }
    } catch(const std::exception& e) {
        LOGGER_ERROR("JSON PARSE ERROR {}", e.what());
        clean_up(); //将配置的资源全部清理
        return false;
    }
    bool is_connected = EnsureConnect();
    if (!zoo_client_) {
        LOGGER_ERROR(" ZOO_CLIENT INIT ERROR");
        clean_up();
        return false;
    }
    if (! is_connected) {
        LOGGER_ERROR("FAILED TO CONNECT");
        clean_up();
        return false;
    }
}catch(const std::exception& e) {
    LOGGER_ERROR("INIT  ZkHandler ERROR {}", e.what()) ;
    clean_up();
    return false;
}
inited_.store(true);
return true;
}
bool ZkHandler:: CreateServiceRegistryIfNeeded() {
    try{
        std::string zoo_address = GetConnectString();
        std::lock_guard<std::mutex> lm (server_register_mtx_);
        if(service_register_) {
            return true;
        }
        service_register_ = std::make_unique<ServiceRegister>(zoo_address);
        //等待连接 服务是异步的
        std::this_thread::sleep_for(std::chrono::seconds(2));
        if (!service_register_->IsConnect()) {
            LOGGER_ERROR("ServiceRegister Connect Error");
            service_register_.reset(); //清除资源
            return false;
        }   
        return true;

    } catch(const std::exception& e) {
        LOGGER_ERROR("ServiceRegister Create Error{}", e.what());
        std::lock_guard<std::mutex>lm(server_register_mtx_);
        service_register_.reset(); //清除资源
        return false;
    }
}
ServiceRegister* ZkHandler:: GetServiceRegister() {
    if (!CreateServiceRegistryIfNeeded()) {
        LOGGER_ERROR("Failed To Get ServiceRegister");
        return nullptr;
    }
    std::lock_guard<std::mutex> lm(server_register_mtx_);
    return service_register_.get();
}
bool ZkHandler:: RegisterService(const std::string&path, const std::string& address) {
    try {

         ServiceRegister* registry = GetServiceRegister();
         if (!registry) {
            LOGGER_ERROR("Failed To Create Service");
            return false;
        }
        if (registry-> RegisterService(path, address)) {
            return true;
        } else {
            LOGGER_ERROR("FAILED TO REGISTER SERVICE");
            return false;
        }
    } catch(const std::exception& e) {
        LOGGER_ERROR("FAILED TO RegisterService {}", e.what());
        return false;
    }
}

bool ZkHandler:: RegisterServiceConfig(const RegisterConfig& service_config) {
    if (service_config.GetNodesSize() == 0) {
        LOGGER_WARN("NO SERVICE CONFIG");
        return false;
    }
    try {
        ServiceRegister* registry = GetServiceRegister();
        if (!registry) {
            LOGGER_ERROR("Failed To Service Register");
            return false;
        }
        bool all_success {true};
        std::string service_name = service_config.GetServiceName();
        auto configs = service_config.GetRegisterNodes();
        for (auto info : configs) {
           std::string address = info.address + ":" + std::to_string(info.port);
            if (registry->RegisterService(service_name, address))
                {
                    // // LOG_INFO("Successfully registered service: {} at {}", service_name, service_address);
                }

           else {
            all_success = false;
           }
        }
        return all_success;
    } catch(const std::exception& e) {
        LOGGER_ERROR("FAILED TO RegisterServiceConfig {}", e.what());
        return false;
    }
}
std::vector<std::string> ZkHandler:: GetAllServers(const std::string& zk_namespace) {
    std::lock_guard<std::mutex> lm(connect_mtx_);
    if (!EnsureConnectLocked()){
        LOGGER_ERROR("FAILED TO CONNECT");
        return {};
    }
     StringVectorGuard child_node;
    try {
        //zoo_client_ 归 connect_mtx_ 管
        int rc = zoo_get_children(zoo_client_, zk_namespace.c_str(), 0, &child_node.value); //获取孩子节点
        if (rc != ZOK) {
            LOGGER_ERROR("GET CHILDREN NODE ERROR {}", zerror(rc));
            return {};
        }
        std::vector<std::string> server;
        for (int i = 0; i < child_node.value.count; i++) {
            std::string zk_address = zk_namespace + "/" + child_node.value.data[i];
            char buf[1024];
            int buf_len = sizeof(buf);
            int rc = zoo_get(zoo_client_, zk_address.c_str(), 0, buf, &buf_len, nullptr);
            if (rc == ZOK && buf_len > 0) {
                std::string server_ip = std::string(buf, buf_len);//防止没找到\0 一直往下
                server.push_back(server_ip);
            } else {
                LOGGER_ERROR("FAILED TO GET NDOE DATA {}", zerror(rc));
                //一个发生错误不影响
            }
        }
        
        return server;
    } catch(const std::exception& e) {
        LOGGER_ERROR("FAILED TO GetAllServers {}", e.what());
        return {};
    }
}
void ZkHandler::UpdateServer(const std::string& zk_namespace) {
    std::vector<std::string> server = GetAllServers(zk_namespace);
       {    
        std::lock_guard<std::mutex> lock(server_mtx_);
        if (server.empty()) {
            return;
        }
        servers_ = std::move(server); //移动赋值
    }
}
std::string ZkHandler:: GetServer(const std::string& zk_namespace) {
    
    UpdateServer(zk_namespace);
    std::lock_guard<std::mutex> lock(server_mtx_);
    if (servers_.empty()) {
        LOGGER_ERROR("NO AVAILABLE SERVER");
        return "";
    } else {
        std::string server_data = LogBalancer::SelectServer(servers_); //按照负载方法
        return server_data;
    }
}
void ZkHandler:: Watcher(  zhandle_t* zh,
    int type,
    int state,
    const char* path,
    void* watcherCtx) {
        auto* handler =
        static_cast<ZkHandler*>(watcherCtx);
        if (handler == nullptr) {
            return; //没有对象直接退出
        }
        if (type != ZOO_SESSION_EVENT) {
            return; //只观察会话变化
        }
        //ZOO_*_STATE 是 extern const int，不是编译期常量，不能用 switch
        if (state == ZOO_CONNECTED_STATE) {
            LOGGER_INFO("ZooKeeper connected");
        } else if (state == ZOO_CONNECTING_STATE) {
            LOGGER_INFO("ZooKeeper connecting");
        } else if (state == ZOO_ASSOCIATING_STATE) {
            LOGGER_INFO("ZooKeeper associating");
        } else if (state == ZOO_EXPIRED_SESSION_STATE) {
            LOGGER_ERROR("ZooKeeper session expired");
            handler->session_failed_.store(true);
        } else if (state == ZOO_AUTH_FAILED_STATE) {
            LOGGER_ERROR("ZooKeeper authentication failed");

            handler->session_failed_.store(true);
        } else {
            LOGGER_WARN("Unknown ZooKeeper state: {}", state);
        }
        
    }
void ZkHandler:: SetZooHost(const std::string host) {
    std::lock_guard<std::mutex> lm(mtx_);
    try {
    if (host == "") {
        LOGGER_WARN("NO HOST NAME BACK  TO LOCALHOST");
        zoo_host_ = "localhost";
        return ;
    } 
    zoo_host_ = host;
    } catch(const std::exception& e) {
        LOGGER_ERROR("SetZooHost Error {}", e.what());
    }
}
void ZkHandler:: SetZooPort(int port) {
     std::lock_guard<std::mutex> lock(mtx_);
     zoo_port_ = (port > 0 && port < 65536) ? port : 2181;

}
void ZkHandler:: SetZooNameSpace(std::string zk_namespace) {
     std::lock_guard<std::mutex> lock(mtx_);
     zk_namespace_ = zk_namespace;
}

void ZkHandler::clean_up()
{
    std::lock_guard<std::mutex> lock(connect_mtx_);
    CleanUpLocked();
  
}

ZkHandler::~ZkHandler() {
    clean_up();
}

}// namespace yan_rpc