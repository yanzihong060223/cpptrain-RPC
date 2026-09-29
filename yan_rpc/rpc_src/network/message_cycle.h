#pragma once

#include <atomic>
#include <future>
#include <mutex>
#include <pthread.h>
#include <set>

#include "connection_manager.h"
#include "connection.h"
#include "../protocol/rpc_protocol.h"
#include "../core/error_code.h"

#ifdef __APPLE__
#include <sys/event.h>
#include <sys/time.h>
#include <sys/types.h>
#else
#include <sys/epoll.h>
#endif

namespace yan_rpc {

class MessageCycle {
public:
    explicit MessageCycle(ConnectionManager* manager);
    bool AddListenFd(int fd);
    bool RemoveListenFd(int fd);
    void HandleRequest(
        const std::shared_ptr<Connection>& connection,
        const RpcRequest& request
    );
    void HandleRequestAsync(
        const std::shared_ptr<Connection>& connection,
        const RpcRequest& request
    );
    void Loop();
    void Stop();
    ~MessageCycle();

private:
#ifdef __APPLE__
    int kqueue_fd_{-1};
#else
    int epoll_fd_{-1};
#endif

    pthread_t thread_id_{};
    std::atomic<bool> running_{false};
    std::set<int> listen_fds_;

#ifdef __APPLE__
    void HandleKqueueEvents(struct kevent* events, int nfds);
#else
    void HandleEpollEvents(struct epoll_event* events, int nfds);
#endif

    void HandleNewConnection(int fd);
    void HandleNewClients(int fd);
    void RemoveConnection(int fd);
    void SendSuccessResponse(
        const std::shared_ptr<Connection>& connection,
        std::uint32_t sequence_id,
        const std::string& result
    );
    void SendErrorResponse(
        const std::shared_ptr<Connection>& connection,
        std::uint32_t sequence_id,
        cookrpc::ErrorCode error_code,
        const std::string& error_message
    );
    void SendResponse(
        const std::shared_ptr<Connection>& connection,
        RpcReponse& response
    );
    void HandleRpcRequestSync(
        const std::shared_ptr<Connection>& connection,
        const RpcRequest& request
    );

    ConnectionManager* connect_manager_{nullptr};
    void RemoveFromLoop(int fd);
    bool IsValidFd(int fd);
    bool IsValidRpcRequest(const RpcRequest& request);
    std::mutex mtx_;
};

} // namespace yan_rpc
