#include "connection.h"

#include "../compress_data/compress_data.h"
#include "../encrypt/aes_encrypt.h"
#include "../../log_manager.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace yan_rpc {

Connection::Connection(int fd) : fd_(fd) {
    if (fd < 0) {
        LOGGER_ERROR("Invalid connection fd");
        return;
    }

    int socket_type = 0;
    socklen_t socket_type_length = sizeof(socket_type);
    if (::getsockopt(
            fd,
            SOL_SOCKET,
            SO_TYPE,
            &socket_type,
            &socket_type_length) < 0) {
        LOGGER_ERROR(
            "Get socket type failed, errno: {}, error: {}",
            errno,
            std::strerror(errno)
        );
        Close();
        return;
    }

    sockaddr_in client_address{};
    socklen_t address_length = sizeof(client_address);
    if (::getpeername(
            fd,
            reinterpret_cast<sockaddr*>(&client_address),
            &address_length) < 0) {
        LOGGER_ERROR(
            "Get peer address failed, errno: {}, error: {}",
            errno,
            std::strerror(errno)
        );
        Close();
        return;
    }

    char ip[INET_ADDRSTRLEN]{};
    if (::inet_ntop(
            AF_INET,
            &client_address.sin_addr,
            ip,
            sizeof(ip)) == nullptr) {
        LOGGER_ERROR(
            "Convert peer IP failed, errno: {}, error: {}",
            errno,
            std::strerror(errno)
        );
        Close();
        return;
    }

    peer_ip_ = ip;
    peer_port_ = ntohs(client_address.sin_port);
    state_ = ConnectState::Connected;

    LOGGER_INFO(
        "Connection established, peer: {}:{}",
        peer_ip_,
        peer_port_
    );
}

Connection::~Connection() {
    Close();
}

bool Connection::IsValid() const {
    return fd_ >= 0 &&
           state_ == ConnectState::Connected;
}

bool Connection::Read() {
    if (!IsValid()) {
        LOGGER_ERROR("Connection is not valid");
        return false;
    }

    char buffer[4096];

    while (true) {
        const int fd = fd_;
        if (fd < 0) {
            return false;
        }

        const ssize_t received =
            ::recv(fd, buffer, sizeof(buffer), 0);

        if (received > 0) {
            read_buffer_.insert(
                read_buffer_.end(),
                buffer,
                buffer + received
            );

            if (read_buffer_.size() > kMaxBufferSize) {
                LOGGER_ERROR(
                    "Read buffer overflow, size: {}",
                    read_buffer_.size()
                );
                HandleClose();
                return false;
            }

            continue;
        }

        if (received == 0) {
            LOGGER_INFO(
                "Peer closed connection, peer: {}:{}",
                peer_ip_,
                peer_port_
            );
            HandleClose();
            return false;
        }

        if (errno == EINTR) {
            continue;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return true;
        }

        LOGGER_ERROR(
            "Read failed, errno: {}, error: {}",
            errno,
            std::strerror(errno)
        );
        HandleClose();
        return false;
    }
}

bool Connection::Write(const std::string& message) {
    if (message.empty()) {
        return true;
    }

    bool send_ok = false;

    {
        std::lock_guard<std::mutex> lock(write_mutex_);

        if (!IsValid()) {
            LOGGER_ERROR("Connection is not valid");
            return false;
        }

        write_buffer_.insert(
            write_buffer_.end(),
            message.begin(),
            message.end()
        );

        send_ok = SendBuffer();
    }

    if (!send_ok) {
        HandleClose();
    }

    return send_ok;
}

bool Connection::SendBuffer() {
    while (!write_buffer_.empty()) {
        const int fd = fd_;
        if (fd < 0) {
            return false;
        }

        int send_flags = 0;
#ifdef MSG_NOSIGNAL
        send_flags = MSG_NOSIGNAL;
#endif

        const ssize_t sent = ::send(
            fd,
            write_buffer_.data(),
            write_buffer_.size(),
            send_flags
        );

        if (sent > 0) {
            write_buffer_.erase(
                write_buffer_.begin(),
                write_buffer_.begin() + sent
            );
            continue;
        }

        if (sent < 0 && errno == EINTR) {
            continue;
        }

        if (sent < 0 &&
            (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return true;
        }

        LOGGER_ERROR(
            "Write failed, errno: {}, error: {}",
            errno,
            std::strerror(errno)
        );
        return false;
    }

    return true;
}

bool Connection::ProcessMessage() {
    if (read_buffer_.empty()) {
        return true;
    }

    std::string encrypted_data(
        read_buffer_.begin(),
        read_buffer_.end()
    );

    auto& encryptor = AesEncrypt::GetInstance();
    if (!encryptor.Init(AesEncryptConfig)) {
        LOGGER_ERROR("Initialize encryptor failed");
        return false;
    }

    std::string plaintext;
    if (!encryptor.Decrypt(encrypted_data, plaintext)) {
        LOGGER_ERROR("Decrypt request failed");
        return false;
    }

    std::string decompressed_data;
    if (!Compress::GetInstance().DepressString(
            plaintext,
            decompressed_data)) {
        LOGGER_ERROR("Decompress request failed");
        return false;
    }

    RpcRequest request;
    if (!request.Deserialization(decompressed_data)) {
        LOGGER_ERROR("Deserialize request failed");
        return false;
    }

    read_buffer_.clear();
    HandleMessage(request);
    return true;
}

void Connection::HandleMessage(const RpcRequest& request) {
    if (!message_callback_) {
        LOGGER_ERROR(
            "Message callback is not set, fd: {}",
            fd_
        );
        HandleClose();
        return;
    }

    try {
        message_callback_(shared_from_this(), request);
    } catch (const std::exception& error) {
        LOGGER_ERROR(
            "Message callback failed: {}",
            error.what()
        );
        HandleClose();
    } catch (...) {
        LOGGER_ERROR("Message callback failed with unknown exception");
        HandleClose();
    }
}

void Connection::HandleClose() {
    if (state_ == ConnectState::Disconnected) {
        return;
    }

    auto self = weak_from_this().lock();

    if (close_callback_ && self) {
        try {
            close_callback_(self);
        } catch (const std::exception& error) {
            LOGGER_ERROR(
                "Close callback failed: {}",
                error.what()
            );
        } catch (...) {
            LOGGER_ERROR("Close callback failed with unknown exception");
        }
    }

    Close();
}

void Connection::Close() {
    std::lock_guard<std::mutex> lock(write_mutex_);

    if (state_ == ConnectState::Disconnected && fd_ < 0) {
        return;
    }

    if (fd_ >= 0) {
        LOGGER_INFO(
            "Close connection, peer: {}:{}",
            peer_ip_,
            peer_port_
        );
        ::close(fd_);
        fd_ = -1;
    }

    state_ = ConnectState::Disconnected;
    read_buffer_.clear();
    write_buffer_.clear();
}

} // namespace yan_rpc
