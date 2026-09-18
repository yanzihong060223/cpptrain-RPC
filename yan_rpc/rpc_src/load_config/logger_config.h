#pragma once
#include "../../log_manager.h"

#include <string>

#include <nlohmann/json.hpp>
 
//logger的配置
namespace yan_rpc {
class LoggerConfig {
public:
static LoggerConfig& GetInstance() {
    static LoggerConfig instance;
    return instance;
}
bool Init(const nlohmann::json& logjson);
void SetLogFile(const std::string& logfile) {log_file_ = logfile; };
void SetLogLevel(const std::string& loglevel) {log_level_ = loglevel;};
const std::string& GetLogFile() const {return log_file_;};
const std::string& GetLogPattern() const {return log_level_;};
LoggerConfig(const LoggerConfig&) = delete;
LoggerConfig& operator= (const LoggerConfig&) = delete;
LoggerConfig(LoggerConfig && l) = delete;
LoggerConfig& operator=(LoggerConfig && l) =delete;


private:
    std::string log_file_;
    std::string log_level_;
private:
    LoggerConfig() = default;
    ~LoggerConfig() = default;
};
} //namespace yan_rpc