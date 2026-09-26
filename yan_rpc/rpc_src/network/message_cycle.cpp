#include "message_cycle.h"

#include "../compress_data/compress_data.h"
#include "../encrypt/aes_encrypt.h"
#include "../service/service_manager.h"
#include "../thread_pool/thread_pool_single.h"
#include "../../log_manager.h"

#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace yan_rpc {

static std::atomic<bool> stop_flag{false};

static void SignalHandler(int)
{
    stop_flag = true;
}

MessageCycle::MessageCycle(ConnectionManager* manager)
    : running_(true), connect_manager_(manager)
{
    if (!connect_manager_) {
        throw std::invalid_argument("Connection manager is null");
    }

    stop_flag.store(false);

#ifdef __APPLE__
    kqueue_fd_ = kqueue();
    if (kqueue_fd_ == -1) {
        LOGGER_ERROR("Kqueue fd create failed");
        return;
    }
#else
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ == -1) {
        LOGGER_ERROR("Epoll fd create failed");
        return;
    }
#endif

    if (::signal(SIGINT, SignalHandler) == SIG_ERR) {
        LOGGER_ERROR("Signal init failed");
        throw std::runtime_error("Signal init failed");
    }
}

#ifdef __APPLE__
void MessageCycle::HandleKqueueEvents(struct kevent* events, int nfds)
{
    for (int i = 0; i < nfds; ++i) {
        int fd = static_cast<int>(events[i].ident);

        if (fd < 0) {
            continue;
        }

        if (!IsValidFd(fd)) {
            RemoveFromLoop(fd);
            continue;
        }

        if (events[i].flags & EV_ERROR) {
            LOGGER_ERROR("Invalid fd: {}", fd);

            bool is_listen_fd = false;
            {
                std::lock_guard<std::mutex> lock(mtx_);
                is_listen_fd =
                    listen_fds_.find(fd) != listen_fds_.end();
            }

            if (is_listen_fd) {
                RemoveListenFd(fd);
                ::close(fd);
            } else {
                RemoveConnection(fd);
            }
            continue;
        }

        bool is_listen_fd = false;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            is_listen_fd =
                listen_fds_.find(fd) != listen_fds_.end();
        }
        if (is_listen_fd) {
            HandleNewConnection(fd);
            continue;
        }

        auto connection = connect_manager_->GetConnection(fd);
        if (!connection) {
            LOGGER_ERROR("No connection");
            RemoveFromLoop(fd);
            continue;
        }

        if (events[i].filter == EVFILT_READ) {
            HandleNewClients(fd);
        }
    }
}
#else
void MessageCycle::HandleEpollEvents(struct epoll_event* events, int nfds)
{
    for (int i = 0; i < nfds; ++i) {
        int fd = events[i].data.fd;

        if (fd < 0) {
            continue;
        }


        if (!IsValidFd(fd)) {
            RemoveFromLoop(fd);
            continue;
        }

        if (events[i].events & (EPOLLERR | EPOLLHUP)) {
            LOGGER_ERROR("Event error on fd: {}", fd);

            bool is_listen_fd = false;
            {
                std::lock_guard<std::mutex> lock(mtx_);
                is_listen_fd =
                    listen_fds_.find(fd) != listen_fds_.end();
            }

            if (is_listen_fd) {
                RemoveListenFd(fd);
                ::close(fd);
            } else {
                RemoveConnection(fd);
            }
            continue;
        }

        bool is_listen_fd = false;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            is_listen_fd =
                listen_fds_.find(fd) != listen_fds_.end();
        }
        if (is_listen_fd) {
            HandleNewConnection(fd);
            continue;
        }

        auto connection = connect_manager_->GetConnection(fd);
        if (!connection) {
            LOGGER_ERROR("No connection");
            RemoveFromLoop(fd);
            continue;
        }

        if (events[i].events & EPOLLIN) {
            HandleNewClients(fd);
        }
    }
}
#endif

bool MessageCycle::AddListenFd(int fd)
{
    if (fd < 0) {
        LOGGER_ERROR("Illegal fd");
        return false;
    }

    std::lock_guard<std::mutex> lock(mtx_);

#ifdef __APPLE__
    struct kevent event;
    EV_SET(
        &event,
        fd,
        EVFILT_READ,
        EV_ADD | EV_ENABLE,
        0,
        0,
        nullptr
    );
    if (kevent(kqueue_fd_, &event, 1, nullptr, 0, nullptr) == -1) {
        LOGGER_ERROR("Add listen fd failed");
        return false;
    }
#else
    struct epoll_event event{};
    event.events = EPOLLIN;
    event.data.fd = fd;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &event) == -1) {
        LOGGER_ERROR("Add listen fd failed");
        return false;
    }
#endif

    listen_fds_.insert(fd);
    return true;
}

bool MessageCycle::RemoveListenFd(int fd)
{
    std::lock_guard<std::mutex> lock(mtx_);

    if (listen_fds_.find(fd) == listen_fds_.end()) {
        LOGGER_ERROR("Listen fd not found");
        return false;
    }

#ifdef __APPLE__
    struct kevent event;
    EV_SET(&event, fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
    if (kevent(kqueue_fd_, &event, 1, nullptr, 0, nullptr) == -1) {
        LOGGER_ERROR("Remove listen fd failed");
        return false;
    }
#else
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr) == -1) {
        LOGGER_ERROR(
            "Failed to remove fd {} from epoll: {}",
            fd,
            std::strerror(errno)
        );
        return false;
    }
#endif

    listen_fds_.erase(fd);
    return true;
}

void MessageCycle::HandleNewConnection(int fd)
{
    struct sockaddr_in client_address{};
    socklen_t address_length = sizeof(client_address);
    int client_fd = ::accept(
        fd,
        reinterpret_cast<struct sockaddr*>(&client_address),
        &address_length
    );

    if (client_fd < 0) {
        LOGGER_ERROR("New connection error");
        return;
    }

    bool connection_added = false;

    try {
        int flags = fcntl(client_fd, F_GETFL, 0);
        if (flags < 0 ||
            fcntl(client_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
            throw std::runtime_error("Set nonblock failed");
        }

        int option_value = 1;
        if (setsockopt(
                client_fd,
                IPPROTO_TCP,
                TCP_NODELAY,
                &option_value,
                sizeof(option_value)) < 0) {
            throw std::runtime_error("Set TCP_NODELAY failed");
        }

        auto connection = std::make_shared<Connection>(client_fd);
        connection->SetMessageCallback(
            [this](
                const std::shared_ptr<Connection>& current_connection,
                const RpcRequest& request) {
                HandleRequest(current_connection, request);
            }
        );
        connection->SetCloseCallback(
            [this](const std::shared_ptr<Connection>& current_connection) {
                RemoveConnection(current_connection->GetFd());
            }
        );

        connect_manager_->AddConnection(connection);
        connection_added = true;

#ifdef __APPLE__
        struct kevent event;
        EV_SET(
            &event,
            client_fd,
            EVFILT_READ,
            EV_ADD | EV_ENABLE,
            0,
            0,
            nullptr
        );
        if (kevent(kqueue_fd_, &event, 1, nullptr, 0, nullptr) == -1) {
            throw std::runtime_error("Add client fd failed");
        }
#else
        struct epoll_event event{};
        event.events = EPOLLIN | EPOLLET;
        event.data.fd = client_fd;
        if (epoll_ctl(
                epoll_fd_,
                EPOLL_CTL_ADD,
                client_fd,
                &event) == -1) {
            throw std::runtime_error("Add client fd failed");
        }
#endif
    } catch (const std::exception& error) {
        LOGGER_ERROR("Add client fd failed: {}", error.what());
        RemoveFromLoop(client_fd);
        if (connection_added) {
            connect_manager_->RemoveConnection(client_fd);
        } else {
            ::close(client_fd);
        }
        return;
    }
}

void MessageCycle::RemoveFromLoop(int fd)
{
    if (fd < 0) {
        LOGGER_ERROR("Illegal fd");
        return;
    }

    std::lock_guard<std::mutex> lock(mtx_);

#ifdef __APPLE__
    struct kevent event;
    EV_SET(&event, fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
    kevent(kqueue_fd_, &event, 1, nullptr, 0, nullptr);
#else
    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
#endif
}

void MessageCycle::HandleRpcRequestSync(
    const std::shared_ptr<Connection>& connection,
    const RpcRequest& request)
{
    try {
        RpcReponse response;
        response.SetterSequenceId(request.GetSequenceid());

        if (!IsValidRpcRequest(request)) {
            LOGGER_WARN("Invalid request");
            SendErrorResponse(
                connection,
                response.GetSequenceid(),
                cookrpc::ErrorCode::INVALID_REQUEST,
                "Invalid request"
            );
            return;
        }

        std::string result;
        bool completed = ServiceManager::GetInstance().HandleRpcRequest(
            request.GetServiceName(),
            request.GetMethodName(),
            result,
            request.GetPayload()
        );

        if (!completed) {
            LOGGER_ERROR("Handle request error");
            SendErrorResponse(
                connection,
                response.GetSequenceid(),
                cookrpc::ErrorCode::SERVICE_NOT_FOUND,
                "Service or method not found"
            );
            return;
        }

        SendSuccessResponse(
            connection,
            response.GetSequenceid(),
            result
        );
    } catch (const std::exception& error) {
        LOGGER_ERROR("Handle RPC request failed: {}", error.what());
    }
}

void MessageCycle::SendSuccessResponse(
    const std::shared_ptr<Connection>& connection,
    std::uint32_t sequence_id,
    const std::string& result)
{
    RpcReponse response;
    response.SetterSequenceId(sequence_id);
    response.SetResultData(result);
    response.SetErrorCode(
        static_cast<std::uint32_t>(cookrpc::ErrorCode::SUCCESS)
    );
    response.SetErrorMessage("Success");
    SendResponse(connection, response);
}

void MessageCycle::SendErrorResponse(
    const std::shared_ptr<Connection>& connection,
    std::uint32_t sequence_id,
    cookrpc::ErrorCode error_code,
    const std::string& error_message)
{
    RpcReponse response;
    response.SetterSequenceId(sequence_id);
    response.SetResultData("");
    response.SetErrorCode(static_cast<std::uint32_t>(error_code));
    response.SetErrorMessage(error_message);
    SendResponse(connection, response);
}

void MessageCycle::SendResponse(
    const std::shared_ptr<Connection>& connection,
    RpcReponse& response)
{
    std::string data;
    if (!response.Serialization(data)) {
        LOGGER_ERROR("Serialize response failed");
        return;
    }

    std::string compressed_data;
    if (!Compress::GetInstance().CompressString(data, compressed_data)) {
        LOGGER_ERROR("Compress response failed");
        return;
    }

    auto& encryptor = AesEncrypt::GetInstance();
    if (!encryptor.Init(AesEncryptConfig)) {
        LOGGER_ERROR("Initialize encryptor failed");
        return;
    }

    std::string encrypted_data;
    if (!encryptor.Encrypt(compressed_data, encrypted_data)) {
        LOGGER_ERROR("Encrypt response failed");
        return;
    }

    if (!connection->Write(encrypted_data)) {
        LOGGER_ERROR("Send response failed");
    }
}

void MessageCycle::HandleRequestAsync(
    const std::shared_ptr<Connection>& connection,
    const RpcRequest& request)
{
    if (!ThreadPoolSingle::IsInitialized()) {
        ThreadPoolSingle::Init();
        LOGGER_INFO("Init thread pool");
    }

    auto future = ThreadPoolSingle::Enqueue(
        TaskPriority::HIGH,
        [this, connection, request]() {
            HandleRpcRequestSync(connection, request);
        }
    );

    if (!future.valid()) {
        LOGGER_ERROR("Enqueue failed");
        SendErrorResponse(
            connection,
            request.GetSequenceid(),
            cookrpc::ErrorCode::INTERNAL_ERROR,
            "Service temporarily unavailable"
        );
    }
}

void MessageCycle::HandleRequest(
    const std::shared_ptr<Connection>& connection,
    const RpcRequest& request)
{
    HandleRequestAsync(connection, request);
}

void MessageCycle::HandleNewClients(int fd)
{
    auto connection = connect_manager_->GetConnection(fd);
    if (!connection) {
        LOGGER_ERROR("No connection");
        RemoveFromLoop(fd);
        return;
    }

    if (!connection->IsValid()) {
        LOGGER_ERROR("Connection is not valid");
        RemoveConnection(fd);
        return;
    }

    if (!connection->Read()) {
        LOGGER_ERROR("Connection cannot read");
        RemoveConnection(fd);
        return;
    }

    if (!connection->ProcessMessage()) {
        LOGGER_ERROR("Connection cannot process message");
        RemoveConnection(fd);
    }
}

void MessageCycle::Loop()
{
    LOGGER_INFO("Message cycle started");
    running_.store(true);

    constexpr int kMaxEvents = 1024;
    while (running_.load() && !stop_flag.load()) {
#ifdef __APPLE__
        struct kevent events[kMaxEvents];
        struct timespec timeout{1, 0};
        int event_count = kevent(
            kqueue_fd_,
            nullptr,
            0,
            events,
            kMaxEvents,
            &timeout
        );

        if (event_count < 0) {
            LOGGER_ERROR("kevent error");
            break;
        } else if (event_count == 0) {
            LOGGER_WARN("Time out");
            continue;
        } else if (
            event_count > 0 &&
            !stop_flag.load() &&
            running_.load()) {
            HandleKqueueEvents(events, event_count);
        }
#else
        struct epoll_event events[kMaxEvents];
        int event_count = epoll_wait(
            epoll_fd_,
            events,
            kMaxEvents,
            1000
        );

        if (event_count < 0) {
            LOGGER_ERROR("epoll_wait error");
            break;
        } else if (event_count == 0) {
            LOGGER_WARN("Time out");
            continue;
        }

        if (
            event_count > 0 &&
            !stop_flag.load() &&
            running_.load()) {
            HandleEpollEvents(events, event_count);
        }
#endif

        if (!running_.load() || stop_flag.load()) {
            LOGGER_INFO("Message cycle stopped");
            break;
        }
    }
}

void MessageCycle::RemoveConnection(int fd)
{
    RemoveFromLoop(fd);
    connect_manager_->RemoveConnection(fd);
}

MessageCycle::~MessageCycle()
{
    if (connect_manager_) {
        connect_manager_->CloseAll();
    }

    std::lock_guard<std::mutex> lock(mtx_);
    running_.store(false);
    stop_flag.store(true);

    for (int fd : listen_fds_) {
#ifdef __APPLE__
        struct kevent event;
        EV_SET(&event, fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
#else
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
#endif
        ::close(fd);
    }
    listen_fds_.clear();

#ifdef __APPLE__
    if (kqueue_fd_ >= 0) {
        ::close(kqueue_fd_);
        kqueue_fd_ = -1;
    }
#else
    if (epoll_fd_ >= 0) {
        ::close(epoll_fd_);
        epoll_fd_ = -1;
    }
#endif
}

void MessageCycle::Stop()
{
    running_.store(false);
    stop_flag.store(true);
}

bool MessageCycle::IsValidFd(int fd)
{
    if (fd < 0) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mtx_);
    if (listen_fds_.find(fd) != listen_fds_.end()) {
        return true;
    }

    if (!connect_manager_->GetConnection(fd)) {
        return false;
    }

    return true;
}

bool MessageCycle::IsValidRpcRequest(const RpcRequest& request)
{
    return request.GetSequenceid() > 0 &&
           !request.GetServiceName().empty() &&
           !request.GetMethodName().empty();
}

} // namespace yan_rpc
