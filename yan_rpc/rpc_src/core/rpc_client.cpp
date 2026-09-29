#include "rpc_client.h"

#include "../conn_balance/zk_conn_handler.h"
#include "../load_config/rpc_client_config.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

namespace yan_rpc {

bool RpcClient::LoadConfig(const std::string& config_file)
{
    auto& config = RpcServiceConfig::GetInstance();
    if (!config.Init(config_file)) {
        LOGGER_ERROR("Config Load Failed");
        return false;
    }

    timeout_ms_ = config.GetTimeoutMs();
    retry_times_ = config.GetRetryTimes();
    zk_namespace_ = config.GetZkNameSpace();

    auto& zk_handler = ZkHandler::GetInstance();
    nlohmann::json zk_config;
    zk_config["host"] = config.GetZkHost();
    zk_config["port"] = config.GetZkPort();
    zk_config["zoo_namespace"] = config.GetZkNameSpace();

    if (!zk_handler.Init(zk_config)) {
        LOGGER_ERROR("ZkHandler Init Failed");
        return false;
    }

    return true;
}

bool RpcClient::InitFd(int& fd)
{
    fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        LOGGER_ERROR("Init Fd Failed");
        return false;
    }

    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0 ||
        ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        LOGGER_ERROR("Set Nonblock Failed");
        ::close(fd);
        fd = -1;
        return false;
    }

    return true;
}

bool RpcClient::TryConnect(
    int fd,
    const struct sockaddr_in& addr,
    int retry_times)
{
    (void)retry_times;

    // 失败时不关闭 fd，由调用方统一关闭
    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0 ||
        ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        LOGGER_ERROR("Set Nonblock Failed");
        return false;
    }

    const socklen_t addr_len = sizeof(addr);
    int ret = ::connect(
        fd,
        reinterpret_cast<const struct sockaddr*>(&addr),
        addr_len
    );

    if (ret == 0) {
        ::fcntl(fd, F_SETFL, flags);
        return true;
    }

    if (errno != EINPROGRESS && errno != EALREADY) {
        LOGGER_ERROR("Connection Failed");
        return false;
    }

    struct timeval timeout;
    timeout.tv_sec = timeout_ms_ / 1000;
    timeout.tv_usec = (timeout_ms_ % 1000) * 1000;

    fd_set write_fds;
    do {
        // select 返回后会修改 fd_set，重试前需要重新设置
        FD_ZERO(&write_fds);
        FD_SET(fd, &write_fds);
        ret = ::select(fd + 1, nullptr, &write_fds, nullptr, &timeout);
    } while (ret < 0 && errno == EINTR);

    if (ret == 0) {
        LOGGER_ERROR("Connection Time Out");
        return false;
    }
    if (ret < 0) {
        LOGGER_ERROR("Select Failed: {}", std::strerror(errno));
        return false;
    }

    int error = 0;
    socklen_t len = sizeof(error);
    if (::getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0 ||
        error != 0) {
        LOGGER_ERROR("Connection Failed After Select");
        return false;
    }

    ::fcntl(fd, F_SETFL, flags);
    return true;
}

bool RpcClient::Connect()
{
    std::lock_guard<std::mutex> lock(mtx_);
    if (is_connected_.load()) {
        return true;
    }

    const std::string server_ip_port =
        ZkHandler::GetInstance().GetServer(zk_namespace_);
    if (server_ip_port.empty()) {
        LOGGER_ERROR("Can Not Get Server");
        return false;
    }

    const std::size_t separator = server_ip_port.find(':');
    const std::string server_ip = server_ip_port.substr(0, separator);
    const int server_port =
        std::stoi(server_ip_port.substr(separator + 1));

    // ip/port 每次重试都不变，只需校验一次
    if (!ValidServiceInfo(server_ip, server_port)) {
        LOGGER_ERROR("Service Info Is Not Valid");
        return false;
    }

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = ::inet_addr(server_ip.c_str());
    addr.sin_port = htons(static_cast<std::uint16_t>(server_port));

    int retry_times = 0;
    while (retry_times < retry_times_) {
        int fd = -1;
        if (!InitFd(fd)) {
            LOGGER_WARN("Init Fd Failed");
            ++retry_times;
            continue;
        }

        if (!TryConnect(fd, addr, retry_times_)) {
            ::close(fd);
            LOGGER_WARN("Try Connect Failed, Retry: {}", retry_times + 1);
            ++retry_times;
            continue;
        }

        // Connection 析构时会关闭 fd，这里失败无需再 close
        connection_ = std::make_shared<Connection>(fd);
        if (!connection_->IsValid()) {
            connection_.reset();
            LOGGER_WARN("Create Connection Failed, Retry: {}", retry_times + 1);
            ++retry_times;
            continue;
        }

        is_connected_.store(true);
        LOGGER_INFO("Connect Success");
        return true;
    }

    LOGGER_ERROR("Connection Failed");
    return false;
}

void RpcClient::DisConnect()
{
    std::lock_guard<std::mutex> lock(mtx_);
    // 不以 is_connected_ 提前返回：发送/读取失败后标志已为 false，
    // 但 connection_ 仍需释放
    if (connection_) {
        connection_->Close();
        connection_.reset();
    }
    is_connected_.store(false);
}

bool RpcClient::ReConnect()
{
    DisConnect();
    return Connect();
}

bool RpcClient::ValidServiceInfo(
    const std::string& ip,
    int port)
{
    if (ip.empty()) {
        LOGGER_ERROR("Empty Service Ip");
        return false;
    }

    struct sockaddr_in addr{};
    if (::inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) != 1) {
        LOGGER_ERROR("Illegal Server Ip");
        return false;
    }

    if (port <= 0 || port > 65535) {
        LOGGER_ERROR("Illegal Port");
        return false;
    }

    return true;
}

RpcClient::RpcClient(const std::string& client_config_file)
{
    try {
        if (!LoadConfig(client_config_file)) {
            throw std::runtime_error("Failed To Load Client Config");
        }
        if (!Connect()) {
            throw std::runtime_error("Connect Failed");
        }
    } catch (const std::exception& error) {
        LOGGER_ERROR(
            "Rpc Client Initialization Failed: {}",
            error.what()
        );
        DisConnect();
        throw;
    }
}

RpcClient::~RpcClient()
{
    DisConnect();
}

} // namespace yan_rpc
