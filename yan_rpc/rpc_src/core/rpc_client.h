#pragma once

#include "../compress_data/compress_data.h"
#include "../encrypt/aes_encrypt.h"
#include "../network/connection.h"
#include "../protocol/rpc_protocol.h"
#include "../serialization/Serialization.h"
#include "../../log_manager.h"
#include "error_code.h"

#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <future>
#include <memory>
#include <mutex>
#include <netinet/in.h>
#include <stdexcept>
#include <string>
#include <sys/select.h>
#include <sys/socket.h>

namespace yan_rpc {

class RpcClient {
public:
    explicit RpcClient(const std::string& client_config_file);
    ~RpcClient();

    RpcClient(const RpcClient&) = delete;
    RpcClient& operator=(const RpcClient&) = delete;
    RpcClient(RpcClient&&) = delete;
    RpcClient& operator=(RpcClient&&) = delete;

    bool LoadConfig(const std::string& config_file);
    bool InitFd(int& fd);
    bool Connect();
    const std::string& GetServerAddress() const { return zk_namespace_; }
    bool ValidServiceInfo(const std::string& ip, int port);
    bool TryConnect(
        int fd,
        const struct sockaddr_in& addr,
        int retry_times
    );
    void DisConnect();
    bool IsConnected() const { return is_connected_.load(); }
    bool ReConnect();

    template<typename Request>
    bool PackageRequest(
        const Request& request,
        const std::string& service_name,
        const std::string& method_name,
        SerType type,
        std::string& encrypted_data)
    {
        std::string user_serialized_data =
            Serialization::Serialize(request, type);
        if (user_serialized_data.empty()) {
            LOGGER_ERROR("Get User Data Failed");
            return false;
        }

        RpcRequest rpc_request;
        rpc_request.SetterSequenceId(++sequence_id_);
        rpc_request.SetPayload(user_serialized_data);
        rpc_request.SetServiceName(service_name);
        rpc_request.SetMethodName(method_name);

        std::string rpc_data;
        if (!rpc_request.Serialization(rpc_data)) {
            LOGGER_ERROR("Request Serialization Failed");
            return false;
        }

        std::string compressed_data;
        if (!Compress::GetInstance().CompressString(
                rpc_data,
                compressed_data)) {
            LOGGER_ERROR("Compress Failed");
            return false;
        }

        auto& aes = AesEncrypt::GetInstance();
        if (!aes.Init(AesEncryptConfig)) {
            LOGGER_ERROR("AES Init Failed");
            return false;
        }
        if (!aes.Encrypt(compressed_data, encrypted_data)) {
            LOGGER_ERROR("Encrypt Failed");
            return false;
        }

        return true;
    }

    bool SendRequest(const std::string& encrypted_data)
    {
        if (!connection_ || !connection_->Write(encrypted_data)) {
            is_connected_.store(false);
            return false;
        }
        return true;
    }

    template<typename Response>
    bool ParseResponse(Response& response, SerType type)
    {
        if (!connection_) {
            LOGGER_ERROR("No Connection");
            return false;
        }

        // fd 是非阻塞的，发完请求立刻 recv 只会拿到 EAGAIN，先等响应可读
        const int fd = connection_->GetFd();
        struct timeval timeout;
        timeout.tv_sec = timeout_ms_ / 1000;
        timeout.tv_usec = (timeout_ms_ % 1000) * 1000;

        fd_set read_fds;
        int ret = 0;
        do {
            // select 返回后会修改 fd_set，重试前需要重新设置
            FD_ZERO(&read_fds);
            FD_SET(fd, &read_fds);
            ret = ::select(fd + 1, &read_fds, nullptr, nullptr, &timeout);
        } while (ret < 0 && errno == EINTR);

        if (ret == 0) {
            LOGGER_ERROR("Wait Response Time Out");
            return false;
        }
        if (ret < 0) {
            LOGGER_ERROR("Select Failed: {}", std::strerror(errno));
            return false;
        }

        if (!connection_->Read()) {
            LOGGER_ERROR("Read Response Failed");
            is_connected_.store(false);
            return false;
        }

        std::string read_buffer_data = connection_->TakeReadData();
        if (read_buffer_data.empty()) {
            LOGGER_ERROR("Read Error No Data");
            return false;
        }

        auto& aes = AesEncrypt::GetInstance();
        if (!aes.Init(AesEncryptConfig)) {
            LOGGER_ERROR("AES Init Failed");
            return false;
        }

        std::string decrypted_data;
        if (!aes.Decrypt(read_buffer_data, decrypted_data)) {
            LOGGER_ERROR("Decryption Failed");
            return false;
        }

        std::string decompressed_data;
        if (!Compress::GetInstance().DepressString(
                decrypted_data,
                decompressed_data)) {
            LOGGER_ERROR("Depress Failed");
            return false;
        }

        RpcReponse rpc_response;
        if (!rpc_response.Deserialization(decompressed_data)) {
            LOGGER_ERROR("Response Deserialization Failed");
            return false;
        }

        if (rpc_response.GetErrorCode() !=
            static_cast<std::uint32_t>(cookrpc::ErrorCode::SUCCESS)) {
            LOGGER_ERROR(
                "RPC Response Error: {}",
                rpc_response.GetErrorMessage()
            );
            return false;
        }

        const std::string result_data = rpc_response.GetResultData();
        return Serialization::Deserialization(response, result_data, type);
    }

    template<typename Response, typename Request>
    bool Call(
        const std::string& service_name,
        const std::string& method_name,
        const Request& request,
        Response& response,
        SerType type)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!IsConnected()) {
            LOGGER_ERROR("No Connection");
            return false;
        }

        std::string encrypted_data;
        if (!PackageRequest(
                request,
                service_name,
                method_name,
                type,
                encrypted_data)) {
            LOGGER_ERROR("Package Request Error");
            return false;
        }

        if (!SendRequest(encrypted_data)) {
            LOGGER_ERROR("Send Request Failed");
            return false;
        }

        return ParseResponse(response, type);
    }

    // 在独立线程里调用同步 Call，失败时抛异常，由调用方 future.get() 捕获
    template<typename Response, typename Request>
    std::future<Response> AsyncCall(
        const std::string& service_name,
        const std::string& method_name,
        const Request& request,
        SerType type)
    {
        return std::async(
            std::launch::async,
            [this, service_name, method_name, request, type]() {
                Response response;
                if (this->Call<Response, Request>(
                        service_name,
                        method_name,
                        request,
                        response,
                        type)) {
                    return response;
                }
                throw std::runtime_error("RPC call failed");
            }
        );
    }

private:
    std::shared_ptr<Connection> connection_;
    std::atomic<bool> is_connected_{false};
    std::uint32_t sequence_id_{0};
    int timeout_ms_{3000};
    int retry_times_{3};
    std::string zk_namespace_;
    std::mutex mtx_;
};

} // namespace yan_rpc
