#include "create_socket.h"
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <atomic>

#include "../../log_manager.h"
// macOS 兼容性处理
#ifdef __APPLE__
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#ifndef SOL_TCP
#define SOL_TCP IPPROTO_TCP
#endif
#endif
namespace yan_rpc {
CreateSocket::CreateSocket(const std::string& service_ip, uint32_t service_port,
        int retry_times, int max_service_connections) : service_ip_(service_ip), service_port_(service_port),
        retry_times_(retry_times), fd_(-1), max_service_connections_(max_service_connections) {
            is_init_.store(false);
        }
CreateSocket::CreateSocket(const RpcServerConfig& Config) : service_ip_(Config.GetServiceIp()), service_port_(Config.GetServicePort()),
retry_times_(Config.GetRetryTime()), fd_(-1), max_service_connections_(Config.GetMaxServiceConnections()) {
    is_init_.store(false);
}
bool CreateSocket::Init() {
    if (service_port_ == 0 || service_port_ > 65535) {
        LOGGER_ERROR("Invalid service port: {}", service_port_);
        return false;
    }

    if (max_service_connections_ <= 0) {
        LOGGER_ERROR(
            "Invalid max service connections: {}",
            max_service_connections_
        );
        return false;
    }

    if (retry_times_ < 0) {
        LOGGER_ERROR("Invalid retry times: {}", retry_times_);
        return false;
    }

    fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ < 0) {
        LOGGER_ERROR("Create Socket Failed");
        return false;
    }
    LOGGER_INFO("Create Socket Success");
    if (!SetSocketOpt()) {
        LOGGER_ERROR("Set Socket Options ");
        ::close(fd_);
        fd_ = -1;
        return false;
    }
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(service_port_);
    if (service_ip_  == "0.0.0.0" || service_ip_.empty()) {
        server_addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (!inet_pton(AF_INET, service_ip_.c_str(), &server_addr.sin_addr)) {
            LOGGER_ERROR("Invalid IP");
            ::close(fd_);
             fd_ = -1;
            return false;
        }
    }
    //struct sockaddr
    socklen_t addrlen = sizeof(server_addr);
    if (::bind(fd_, (sockaddr*)& server_addr, addrlen) < 0) {
        LOGGER_ERROR ("bind error");
        ::close(fd_);
         fd_ = -1;
        return false;
    }
    if (::listen(fd_, max_service_connections_) < 0) {
         LOGGER_ERROR ("listen error");
        ::close(fd_);
         fd_ = -1;
        return false;
    }
    LOGGER_INFO("Socket Init Success");
    is_init_.store(true);
    return true;
}
std::shared_ptr<CreateSocket> CreateSocket::Create(const std::string& service_ip, uint32_t service_port,
        int retry_times, int max_service_connections) {
            std::shared_ptr<CreateSocket> ptr(new CreateSocket(service_ip, service_port, retry_times, max_service_connections));
            bool completed = ptr->Init();
            if (!completed) {
                LOGGER_ERROR("CreateSocket Error");
                return nullptr;
            }
            return ptr;
        }
std::shared_ptr<CreateSocket> CreateSocket::Create(const RpcServerConfig& Config) {
    std::shared_ptr<CreateSocket> ptr(new CreateSocket(Config));
     bool completed = ptr->Init();
    if (!completed) {
        LOGGER_ERROR("CreateSocket Error");
        return nullptr;
        }
            return ptr;
}
bool CreateSocket::SetSocketOpt() {
     int reuse = 1;

    // 允许服务器重启后快速重新绑定端口
    if (::setsockopt(
            fd_,
            SOL_SOCKET,
            SO_REUSEADDR,
            &reuse,
            sizeof(reuse)) < 0) {
        LOGGER_ERROR("Set Socket Option REUSEADDR Error");
        return false;
    }

    // 系统支持SO_REUSEPORT时，允许多个socket共同监听同一端口
#ifdef SO_REUSEPORT
    if (::setsockopt(
            fd_,
            SOL_SOCKET,
            SO_REUSEPORT,
            &reuse,
            sizeof(reuse)) < 0) {
        LOGGER_ERROR("Set Socket Option REUSEPORT Error");
        return false;
    }
#endif

    // epoll/kqueue需要非阻塞socket
    int flags = ::fcntl(fd_, F_GETFL, 0);
    if (flags < 0) {
        LOGGER_ERROR("Set Socket Option Error");
        return false;
    }

    if (::fcntl(fd_, F_SETFL, flags | O_NONBLOCK) < 0) {
        LOGGER_ERROR("Set Socket Option Error");
        return false;
    }

    return true;
}
} //namespace yan_rpc
