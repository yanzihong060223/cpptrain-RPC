#pragma once

#include "../protocol/rpc_protocol.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace yan_rpc {

class Connection : public std::enable_shared_from_this<Connection> {
public:
    enum class ConnectState {
        Connected,
        Connecting,
        Disconnected
    };

    using MessageCallback = std::function<void(
        const std::shared_ptr<Connection>&,
        const RpcRequest&
    )>;

    using CloseCallback = std::function<void(
        const std::shared_ptr<Connection>&
    )>;

    explicit Connection(int fd);
    ~Connection();

    bool IsValid() const;
    bool Read();
    bool Write(const std::string& message);
    bool ProcessMessage();
    void Close();

    int GetFd() const { return fd_; }
    const std::string& GetPeerIp() const { return peer_ip_; }
    std::uint16_t GetPeerPort() const { return peer_port_; }

    void SetMessageCallback(MessageCallback callback) {
        message_callback_ = std::move(callback);
    }

    void SetCloseCallback(CloseCallback callback) {
        close_callback_ = std::move(callback);
    }

private:
    void HandleMessage(const RpcRequest& request);
    void HandleClose();
    bool SendBuffer();

private:
    int fd_{-1};
    std::vector<char> read_buffer_;
    std::vector<char> write_buffer_;

    static constexpr std::size_t kMaxBufferSize = 4096;

    ConnectState state_{ConnectState::Disconnected};
    std::string peer_ip_;
    std::uint16_t peer_port_{0};

    std::mutex write_mutex_;

    MessageCallback message_callback_;
    CloseCallback close_callback_;
};

} // namespace yan_rpc
