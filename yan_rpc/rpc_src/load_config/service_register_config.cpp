#include "service_register_config.h"
#include "../../log_manager.h"

#include <fstream>
#include <stdexcept>


#include <nlohmann/json.hpp>
using json = nlohmann::json;
namespace yan_rpc {
bool RegisterConfig::Init(const std::string& config_file) {
    std::ifstream file(config_file);
    if (!file.is_open()) {
        LOGGER_ERROR("FAILED TO OPEN FILE");
        return false;
    }
     json config;
    try{
    config = json::parse(file);
    } catch(const std::exception& e) {
         LOGGER_ERROR("JSON ERROR : {}", e.what());
         return false;
    }
    std::string service_name = config.value("service_name", "");
    if (service_name == "") {
           LOGGER_ERROR("NO SERVICE NAME");
           return false;
    }
     std::string service_version = config.value("service_version", "");
     if (service_version == "") {
           LOGGER_ERROR("NO SERVICE version");
           return false;
     }
    
     if (!config.contains("register_nodes") || !config["register_nodes"].is_array() ||
    config["register_nodes"].empty()) {
           LOGGER_ERROR("NO REGISTER NODES");
           return false;
     }
     std::vector<RegisterInfo> arry;
     for (auto p : config["register_nodes"]) {
         RegisterInfo temp;
         if (!p.contains("address") || !p.contains("port") || !p["address"].is_string() ||
        !p["port"].is_number_unsigned()) {
            LOGGER_ERROR("ILLEGAL REGISTERNODES");
            return false;
         }
         temp.address = p["address"];
         temp.port = p["port"];
         arry.push_back(temp);
     }
     SetServiceName(service_name);
     SetServiceVersion(service_version);
     register_nodes_ = std::move(arry);
     return true;
}


} //namespace yan_rpc