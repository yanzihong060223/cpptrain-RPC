#pragma once

#include "../../log_manager.h"

#include <nlohmann/json.hpp>

#include <string>

namespace yan_rpc {

class JsonSerialization {
public:
    template<typename T>
    static std::string Serialization(const T& data);

    template<typename T>
    static bool Deserialization(T& data, const std::string& buf);
};

template<typename T>
std::string JsonSerialization::Serialization(const T& data) {
    try {
        nlohmann::json json_data = data;
        return json_data.dump(4);
    } catch (const std::exception& e) {
        LOGGER_ERROR("Json Serialization Failed {}", e.what());
        return "";
    }
}

template<typename T>
bool JsonSerialization::Deserialization(
    T& data,
    const std::string& buf
) {
    try {
        nlohmann::json j = nlohmann::json::parse(buf);
        T new_data = j.get<T>();
        data = new_data;
        return true;
    } catch (const std::exception& e) {
        LOGGER_ERROR("Json Deserialization Failed {}", e.what());
        return false;
    }
}

} // namespace yan_rpc

