#include "logger_config.h"

#include <stdexcept>
#include <iostream>
namespace yan_rpc {
    bool LoggerConfig::Init (const nlohmann::json& logjson) {
        try{
            SetLogFile(logjson.value("path", "../log"));
            SetLogLevel(logjson.value("level", "info"));
            if (log_level_ != "info" && log_level_ != "debug" && log_level_ != "trace" && log_level_ != "warn" && log_level_ != "error") {
                std::cerr << "SET LOG LEVEL ERROR FAIL BACK TO INFO" << std::endl;
                log_level_ = "info";
                return false;
            }
            return true;
        } catch (const nlohmann::json::exception& e) {
            std::cerr << "INIT ERROR" << e.what() << std::endl;
            return false;
        }
    }
}
