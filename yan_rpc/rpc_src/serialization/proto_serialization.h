#pragma once

#include "../../log_manager.h"

#include <google/protobuf/message.h>

#include <string>

namespace yan_rpc {

class ProtoSerialization {
public:
    template<typename T>
    static std::string Serialization(const T& data);

    template<typename T>
    static bool Deserialization(T& data, const std::string& buf);
};

template<typename T>
std::string ProtoSerialization::Serialization(const T& data) {
    try {
        std::string result;
        if (!data.SerializeToString(&result)) {
            LOGGER_ERROR("SerializeToString Failed");
            return "";
        }
        return result;
    } catch (const std::exception& e) {
        LOGGER_ERROR("SerializeToString Failed {}", e.what());
        return "";
    }
}

template<typename T>
bool ProtoSerialization::Deserialization(
    T& data,
    const std::string& buf
) {
    try {
        if (!data.ParseFromString(buf)) {
            LOGGER_ERROR("Deserialization Failed");
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        LOGGER_ERROR("Deserialization Failed {}", e.what());
        return false;
    }
}

} // namespace yan_rpc

