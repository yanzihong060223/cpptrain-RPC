#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace cookrpc
{

    // RPC 错误码枚举
    enum class ErrorCode : std::uint32_t
    {
        // 成功
        SUCCESS = 0,

        // 系统级错误 (1-999)
        SYSTEM_ERROR = 1,
        UNKNOWN_ERROR = 2,
        OPERATION_TIMEOUT = 3,
        MEMORY_ALLOCATION_ERROR = 4,

        // 网络相关错误 (1000-1999)
        NETWORK_ERROR = 1000,
        CONNECTION_LOST = 1001,
        SOCKET_ERROR = 1002,
        BIND_FAILED = 1003,
        LISTEN_FAILED = 1004,
        CONNECT_FAILED = 1005,
        SEND_FAILED = 1006,
        RECEIVE_FAILED = 1007,

        // 序列化相关错误 (2000-2999)
        SERIALIZE_ERROR = 2000,
        DESERIALIZE_ERROR = 2001,
        INVALID_MESSAGE_FORMAT = 2002,
        PROTOCOL_ERROR = 2003,

        // 服务相关错误 (3000-3999)
        SERVICE_NOT_FOUND = 3000,
        METHOD_NOT_FOUND = 3001,
        SERVICE_ALREADY_EXISTS = 3002,
        INVALID_SERVICE_NAME = 3003,
        SERVICE_UNAVAILABLE = 3004,

        // 参数相关错误 (4000-4999)
        INVALID_ARGUMENT = 4000,
        ARGUMENT_OUT_OF_RANGE = 4001,
        INVALID_CONFIG = 4002,

        // 其他业务错误 (5000-5999)
        BUSINESS_ERROR = 5000,

        // 客户端错误 (6000-6999)
        INVALID_REQUEST = 6000,
        METHOD_ERROR = 6001,
        INTERNAL_ERROR = 6002,
    };
    class Error
    {
    public:
        static std::string getErrorMessage(ErrorCode code)
        {
            static const std::unordered_map<ErrorCode, std::string> error_messages = {
                {ErrorCode::SUCCESS, "Success"},
                {ErrorCode::SYSTEM_ERROR, "System error"},
                {ErrorCode::UNKNOWN_ERROR, "Unknown error"},
                {ErrorCode::OPERATION_TIMEOUT, "Operation timeout"},
                {ErrorCode::MEMORY_ALLOCATION_ERROR, "Memory allocation failed"},

                {ErrorCode::NETWORK_ERROR, "Network error"},
                {ErrorCode::CONNECTION_LOST, "Connection lost"},
                {ErrorCode::SOCKET_ERROR, "Socket error"},
                {ErrorCode::BIND_FAILED, "Bind failed"},
                {ErrorCode::LISTEN_FAILED, "Listen failed"},
                {ErrorCode::CONNECT_FAILED, "Connect failed"},
                {ErrorCode::SEND_FAILED, "Send failed"},
                {ErrorCode::RECEIVE_FAILED, "Receive failed"},

                {ErrorCode::SERIALIZE_ERROR, "Serialize error"},
                {ErrorCode::DESERIALIZE_ERROR, "Deserialize error"},
                {ErrorCode::INVALID_MESSAGE_FORMAT, "Invalid message format"},
                {ErrorCode::PROTOCOL_ERROR, "Protocol error"},

                {ErrorCode::SERVICE_NOT_FOUND, "Service not found"},
                {ErrorCode::METHOD_NOT_FOUND, "Method not found"},
                {ErrorCode::SERVICE_ALREADY_EXISTS, "Service already exists"},
                {ErrorCode::INVALID_SERVICE_NAME, "Invalid service name"},
                {ErrorCode::SERVICE_UNAVAILABLE, "Service unavailable"},

                {ErrorCode::INVALID_ARGUMENT, "Invalid argument"},
                {ErrorCode::ARGUMENT_OUT_OF_RANGE, "Argument out of range"},
                {ErrorCode::INVALID_CONFIG, "Invalid configuration"},

                {ErrorCode::BUSINESS_ERROR, "Business error"},

                {ErrorCode::INVALID_REQUEST, "Invalid request"},
                {ErrorCode::METHOD_ERROR, "Method error"},
                {ErrorCode::INTERNAL_ERROR, "Internal error"},
            };

            auto it = error_messages.find(code);
            return it != error_messages.end() ? it->second : "Unknown error code";
        }
    };

    // 错误结果类
    class ErrorResult
    {
    public:
        ErrorResult() : code_(ErrorCode::SUCCESS) {}
        explicit ErrorResult(ErrorCode code) : code_(code) {}
        ErrorResult(ErrorCode code, const std::string &message)
            : code_(code), message_(message) {}

        bool isOk() const { return code_ == ErrorCode::SUCCESS; }
        ErrorCode code() const { return code_; }
        const std::string &message() const { return message_; }

        std::string toString() const
        {
            if (message_.empty())
            {
                return Error::getErrorMessage(code_);
            }
            return Error::getErrorMessage(code_) + ": " + message_;
        }

    private:
        ErrorCode code_;
        std::string message_;
    };


}
