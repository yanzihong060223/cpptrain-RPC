#pragma once

#include "json_serialization.h"
#include "proto_serialization.h"
#include "../../log_manager.h"

#include <google/protobuf/message.h>
#include <nlohmann/json.hpp>

#include <string>
#include <type_traits>

namespace yan_rpc {

enum class SerType {
    Json,
    Protobuf
};

class Serialization {
public:
    template<typename T>
    static std::string Serialize(const T& data, SerType type);

    template<typename T>
    static bool Deserialization(
        T& data,
        const std::string& buf,
        SerType type
    );

private:
    template<typename T>
    static std::string SerializationProto(const T& data);

    template<typename T>
    static bool DeserializationProto(T& data, const std::string& buf);

    template<typename T>
    static std::string SerializationJson(const T& data);

    template<typename T>
    static bool DeserializationJson(T& data, const std::string& buf);
};

template<typename T>
std::string Serialization::Serialize(const T& data, SerType type) {
    switch (type) {
        case SerType::Json:
            if constexpr (
                std::is_same_v<std::decay_t<T>, nlohmann::json>
            ) {
                return SerializationJson(data);
            } else {
                LOGGER_ERROR("No Current Type");
                return "";
            }

        case SerType::Protobuf:
            if constexpr (
                std::is_base_of_v<
                    google::protobuf::Message,
                    std::decay_t<T>
                >
            ) {
                return SerializationProto(data);
            } else {
                LOGGER_ERROR("No Current Type");
                return "";
            }
    }

    LOGGER_ERROR("Unknown Type");
    return "";
}

template<typename T>
bool Serialization::Deserialization(
    T& data,
    const std::string& buf,
    SerType type
) {
    switch (type) {
        case SerType::Json:
            if constexpr (
                std::is_same_v<std::decay_t<T>, nlohmann::json>
            ) {
                return DeserializationJson(data, buf);
            } else {
                LOGGER_ERROR("No Current Type");
                return false;
            }

        case SerType::Protobuf:
            if constexpr (
                std::is_base_of_v<
                    google::protobuf::Message,
                    std::decay_t<T>
                >
            ) {
                return DeserializationProto(data, buf);
            } else {
                LOGGER_ERROR("No Current Type");
                return false;
            }
    }

    LOGGER_ERROR("Unknown Type");
    return false;
}

template<typename T>
std::string Serialization::SerializationProto(const T& data) {
    return ProtoSerialization::Serialization(data);
}

template<typename T>
bool Serialization::DeserializationProto(
    T& data,
    const std::string& buf
) {
    return ProtoSerialization::Deserialization(data, buf);
}

template<typename T>
std::string Serialization::SerializationJson(const T& data) {
    return JsonSerialization::Serialization(data);
}

template<typename T>
bool Serialization::DeserializationJson(
    T& data,
    const std::string& buf
) {
    return JsonSerialization::Deserialization(data, buf);
}

} // namespace yan_rpc

