#pragma once
#include "connection.h"
#include "../../log_manager.h"

#include <mutex>
#include <unordered_map>
#include <memory>
namespace yan_rpc {
class ConnectionManager {
public:
 static ConnectionManager& GetInstance() {
   static ConnectionManager instance;
   return instance; 
 }
ConnectionManager(const ConnectionManager&) = delete;
ConnectionManager& operator= (const ConnectionManager&) = delete;
ConnectionManager(ConnectionManager&&) = delete;
ConnectionManager& operator=(ConnectionManager&&) = delete;
void AddConnection(std::shared_ptr<Connection> conn);
std::shared_ptr<Connection> GetConnection(int fd);
void RemoveConnection(int fd);
int GetConnectionCount();
void CloseAll();
~ConnectionManager();
private:
ConnectionManager() = default;
std::mutex mtx_;
std::unordered_map<int, std::shared_ptr<Connection>> connection_map_;
};
}
