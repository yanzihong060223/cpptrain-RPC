#pragma once

#include "../service/service.h"
#include "../serialization/Serialization.h"
#include "../../log_manager.h"

#include <nlohmann/json.hpp>

#include <string>

namespace yan_rpc {

class RpcService : public Service {
public:
    std::string GetService() const override {
        return "DefaultService";
    }

    bool HandleService(std::string method_name, std::string& result, std::string args) override {
        if (method_name == "Echo") {
            try {
                nlohmann::json js;
                if (!Serialization::Deserialization(js, args, SerType::Json)) {
                    LOGGER_ERROR("Deserialization Failed");
                    return false;
                }
                nlohmann::json reponse;
                reponse["echo"] = "hahah i am rpc server, welcome to C++ training camp, come on";
                reponse["received_message"] = js.value("message", "");
                result = reponse.dump(4);
                return true;
            } catch (const std::exception& e) {
                LOGGER_ERROR("Echo Failed {}", e.what());
                return false;
            }
        }

        LOGGER_ERROR("Unknown Method {}", method_name);
        return false;
    }
};

} // namespace yan_rpc
