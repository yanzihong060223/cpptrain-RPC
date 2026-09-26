#include "connection_manager.h"
#include "../../log_manager.h"

#include <mutex>
#include <utility>
namespace yan_rpc {
void ConnectionManager::AddConnection(std::shared_ptr<Connection> conn) {       
    if (!conn) {
        LOGGER_ERROR("Connection is null");
        return;
    }
    int fd = conn->GetFd();
    if (fd < 0) {
        LOGGER_ERROR("Connection fd is invalid");
        return;
    }
    {
        std::lock_guard<std::mutex> lm(mtx_);
        if (connection_map_.find(fd) == connection_map_.end()) {
            connection_map_[fd] = conn;
        } else {
            LOGGER_WARN("Connection Exists");
            return;
        }
    }
}
std::shared_ptr<Connection> ConnectionManager:: GetConnection(int fd) {
    std::lock_guard<std::mutex> lm(mtx_);
    auto it = connection_map_.find(fd);
    if (it == connection_map_.end()) {
        LOGGER_ERROR("No Connection");
        return nullptr;
    }
    return it->second;
}
void ConnectionManager::RemoveConnection(int fd) {
    std::shared_ptr<Connection> connection;
    {
        std::lock_guard<std::mutex> lm(mtx_);
        auto it = connection_map_.find(fd);
        if (it == connection_map_.end()) {
            LOGGER_WARN("Connection does not exist, fd: {}", fd);
            return;
        }

        connection = std::move(it->second);
        connection_map_.erase(it);
    }

    if (connection) {
        connection->Close();
    }

    LOGGER_INFO("Remove connection success, fd: {}", fd);
}
int ConnectionManager::GetConnectionCount() {
    std::lock_guard<std::mutex> lm(mtx_);
    return connection_map_.size();
}
void ConnectionManager::CloseAll() {
    std::unordered_map<int, std::shared_ptr<Connection>> connections;
    {
        std::lock_guard<std::mutex> lm(mtx_);
        connections.swap(connection_map_);
    }

    for (auto& pair : connections) {
        if (pair.second) {
            pair.second->Close();
        }
    }
}
 ConnectionManager::~ConnectionManager() {
    CloseAll();
 }
}// namespace yan_rpc
