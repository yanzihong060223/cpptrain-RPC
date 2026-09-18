#pragma once
#include <memory>
#include <string>
#include <thread>
#include <iostream>
#include <sstream>
#include <filesystem>
#include <vector>
#include <stdexcept>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h> //彩色控制台
#include <fmt/format.h>
//日志级别的宏
#define LOGGER_TRACE(...)  SPDLOG_TRACE(__VA_ARGS__)
#define LOGGER_DEBUG(...)  SPDLOG_DEBUG(__VA_ARGS__)
#define LOGGER_INFO(...)  SPDLOG_INFO(__VA_ARGS__)
#define LOGGER_WARN(...)  SPDLOG_WARN(__VA_ARGS__)
#define LOGGER_ERROR(...)  SPDLOG_ERROR(__VA_ARGS__)
//客户端日志的宏
#define CLIENT_LOGGER_TRACE(...)  SPDLOG_TRACE("[CLIENT] " __VA_ARGS__)
#define CLIENT_LOGGER_DEBUG(...)  SPDLOG_DEBUG("[CLIENT] " __VA_ARGS__)
#define CLIENT_LOGGER_INFO(...)  SPDLOG_INFO("[CLIENT] " __VA_ARGS__)
#define CLIENT_LOGGER_WARN(...)  SPDLOG_WARN("[CLIENT] " __VA_ARGS__)
#define CLIENT_LOGGER_ERROR(...)  SPDLOG_ERROR("[CLIENT] " __VA_ARGS__)

template <> //全特化
struct fmt::formatter<std::thread::id> : fmt::formatter<std::string> {
    template <typename FormatContext>
    auto format(const std::thread::id& id, FormatContext& ctx) const {
        std::ostringstream os;
        os << id;
        return fmt::formatter<std::string>::format(os.str(), ctx);
    }
};

//单例模式
class Logger {
public:
    static Logger& GetInstance() {
        static Logger instance;
        return instance;
    }
    //初始化
    //创建日志目录
    //设置控制台输出
    //设置每日日志文件
    //设置日志格式和级别
    bool Init(const std::string& log_file = "../log", const std::string& log_level = "info") {
        try {
            const std::filesystem::path log_dir(log_file);
            if (!std::filesystem::exists(log_dir)) {
                if (!std::filesystem::create_directories(log_dir)) {
                    std::cerr << "FAILED TO CREATE DIRECTORY" << std::endl;
                    return false;
                }
            }
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            std::filesystem::path rpc_log_file = log_dir / "yan_rpc_log";
            auto daily_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(
                rpc_log_file.string(), 0, 0, false, 0);
            std::vector<std::shared_ptr<spdlog::sinks::sink>> sinks{console_sink, daily_sink};
            logger_ = std::make_shared<spdlog::logger>("yan_rpc", sinks.begin(), sinks.end());
            logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] [%t] [%s:%#] %v"); //设置输出模式
            logger_->set_level(spdlog::level::from_str(log_level));
            logger_->flush_on(spdlog::level::err);
            spdlog::set_default_logger(logger_); //  设成默认
            return true;
        } catch (const std::exception& e) {
            std::cerr << e.what() << std::endl;
            return false;
        }
    }

    Logger(const Logger& l) = delete;
    Logger& operator=(const Logger& l) = delete;

private:
    Logger() = default;
    ~Logger() = default;
    std::shared_ptr<spdlog::logger> logger_; //日志实例
};
