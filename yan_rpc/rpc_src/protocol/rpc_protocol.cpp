#include "rpc_protocol.h"

#include <arpa/inet.h>

#include <cstring>
#include <exception>
#include <string>

namespace yan_rpc {
namespace {

constexpr size_t RpcHeadSize = 3 * sizeof(uint32_t);

void WriteUint32(std::string& output, size_t& pos, uint32_t value) {
    const uint32_t network_value = htonl(value);
    std::memcpy(output.data() + pos, &network_value, sizeof(network_value));
    pos += sizeof(network_value);
}

uint32_t ReadUint32(const std::string& input, size_t& pos) {
    uint32_t network_value = 0;
    std::memcpy(&network_value, input.data() + pos, sizeof(network_value));
    pos += sizeof(network_value);
    return ntohl(network_value);
}

} // namespace

bool RpcRequest::Serialization(std::string& output) {
    try {
        const size_t body_size = service_name_.size() + method_name_.size() +
                                 payload_.size() + 3 * sizeof(uint32_t);
        output.resize(RpcHeadSize + body_size);

        size_t pos = 0;
        WriteUint32(output, pos, RpcHead::MagicNumber);
        WriteUint32(output, pos, static_cast<uint32_t>(body_size));
        WriteUint32(output, pos, sequence_id_);

        WriteUint32(output, pos, static_cast<uint32_t>(service_name_.size()));
        std::memcpy(output.data() + pos, service_name_.data(), service_name_.size());
        pos += service_name_.size();

        WriteUint32(output, pos, static_cast<uint32_t>(method_name_.size()));
        std::memcpy(output.data() + pos, method_name_.data(), method_name_.size());
        pos += method_name_.size();

        WriteUint32(output, pos, static_cast<uint32_t>(payload_.size()));
        std::memcpy(output.data() + pos, payload_.data(), payload_.size());
        return true;
    } catch (const std::exception& e) {
        LOGGER_ERROR("Request Serialization Failed {}", e.what());
        return false;
    }
}

bool RpcRequest::Deserialization(std::string& input) {
    try {
        if (input.size() < RpcHeadSize) {
            LOGGER_ERROR("Input Too Small");
            return false;
        }

        size_t pos = 0;
        RpcHead head{};
        head.magic_number = ReadUint32(input, pos);
        head.message_size = ReadUint32(input, pos);
        head.sequence_id = ReadUint32(input, pos);

        if (head.magic_number != RpcHead::MagicNumber) {
            LOGGER_ERROR("No Current Magic Number");
            return false;
        }
        sequence_id_ = head.sequence_id;

        uint32_t len = 0;
        if (pos + sizeof(uint32_t) > input.size()) {
            LOGGER_ERROR("Can Not Read Service Name");
            return false;
        }
        len = ReadUint32(input, pos);
        if (len > input.size() - pos) {
            LOGGER_ERROR("Service Name Too Long Length : {}", len);
            return false;
        }
        service_name_ = input.substr(pos, len);
        pos += len;

        if (pos + sizeof(uint32_t) > input.size()) {
            LOGGER_ERROR("Can Not Read Method Name");
            return false;
        }
        len = ReadUint32(input, pos);
        if (len > input.size() - pos) {
            LOGGER_ERROR("Method Name Too Long Length ,{}", len);
            return false;
        }
        method_name_ = input.substr(pos, len);
        pos += len;

        if (sizeof(uint32_t) > input.size() - pos) {
            LOGGER_ERROR("Can Not Read PayLoad");
            return false;
        }
        len = ReadUint32(input, pos);
        if (len > input.size() - pos) {
            LOGGER_ERROR("PayLoad Too Long Length ,{}", len);
            return false;
        }
        payload_ = input.substr(pos, len);
        return true;
    } catch (const std::exception& e) {
        LOGGER_ERROR("Request Deserialization Failed {}", e.what());
        return false;
    }
}

bool RpcReponse::Serialization(std::string& output) {
    try {
        const size_t body_size = result_data_.size() + error_message_.size() +
                                 sizeof(error_code_) + 2 * sizeof(uint32_t);
        output.resize(RpcHeadSize + body_size);

        size_t pos = 0;
        WriteUint32(output, pos, RpcHead::MagicNumber);
        WriteUint32(output, pos, static_cast<uint32_t>(body_size));
        WriteUint32(output, pos, sequence_id_);

        WriteUint32(output, pos, static_cast<uint32_t>(result_data_.size()));
        std::memcpy(output.data() + pos, result_data_.data(), result_data_.size());
        pos += result_data_.size();

        WriteUint32(output, pos, static_cast<uint32_t>(error_message_.size()));
        std::memcpy(output.data() + pos, error_message_.data(), error_message_.size());
        pos += error_message_.size();

        WriteUint32(output, pos, error_code_);
        return true;
    } catch (const std::exception& e) {
        LOGGER_ERROR("RpcReponse Serialization Error {}", e.what());
        return false;
    }
}

bool RpcReponse::Deserialization(std::string& input) {
    try {
        if (input.size() < RpcHeadSize) {
            LOGGER_ERROR("No Current Input Message");
            return false;
        }

        size_t pos = 0;
        RpcHead head{};
        head.magic_number = ReadUint32(input, pos);
        head.message_size = ReadUint32(input, pos);
        head.sequence_id = ReadUint32(input, pos);

        if (head.magic_number != RpcHead::MagicNumber) {
            LOGGER_ERROR("No Current Magic Number");
            return false;
        }
        if(head.message_size  != input.size() - pos) {
            LOGGER_ERROR("No Current Message");
            return false;
        }
        sequence_id_ = head.sequence_id;

        if (pos + sizeof(uint32_t) > input.size()) {
            LOGGER_ERROR("Can Not Read Result Data");
            return false;
        }
        uint32_t len = ReadUint32(input, pos);
        if (len > input.size() - pos) {
            LOGGER_ERROR("Result Data Too Long Length {}", len);
            return false;
        }
        result_data_ = input.substr(pos, len);
        pos += len;
        if (pos + sizeof(uint32_t) > input.size()) {
            LOGGER_ERROR("Can Not Read Error Message");
            return false;
        }
        len = ReadUint32(input, pos);
        if (len > input.size() - pos) {
            LOGGER_ERROR("Error Message Too Long Length {}", len);
            return false;
        }
        error_message_ = input.substr(pos, len);
        pos += len;

        if (sizeof(uint32_t) != input.size() - pos) {
            LOGGER_ERROR("No Current Error_Code");
            return false;
        }
        error_code_ = ReadUint32(input, pos);
        return true;
    } catch (const std::exception& e) {
        LOGGER_ERROR("RpcReponse Deserialization Failed {}", e.what());
        return false;
    }
}

} // namespace yan_rpc
