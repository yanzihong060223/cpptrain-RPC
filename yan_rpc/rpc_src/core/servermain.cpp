#include "../../log_manager.h"
#include "../conn_balance/zk_conn_handler.h"
#include "../load_config/rpc_server_config.h"
#include "../network/connection_manager.h"
#include "../network/create_socket.h"
#include "../network/message_cycle.h"
#include "../service/service_manager.h"
#include "../thread_pool/thread_pool_single.h"
#include "rpc_service.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <memory>
#include <string>

using namespace yan_rpc;

// 全局变量，用于优雅关闭
static std::atomic<bool> g_shutdown_requested{false};
static std::atomic<bool> g_shutdown_done{false};
static std::unique_ptr<MessageCycle> g_message_cycle;
static std::shared_ptr<CreateSocket> g_socket_server;

// 信号处理函数里只做原子操作，日志等非异步信号安全的操作放到主流程
void SignalHandler(int sig)
{
    (void)sig;
    g_shutdown_requested.store(true);

    // Stop 只修改原子标志，可以在信号处理函数中调用
    if (g_message_cycle) {
        g_message_cycle->Stop();
    }
}

// 优雅关闭函数，可重复调用，只执行一次
void GracefulShutdown()
{
    if (g_shutdown_done.exchange(true)) {
        return;
    }
    LOGGER_INFO("Starting graceful shutdown...");

    // 停止消息循环，不再处理新事件
    if (g_message_cycle) {
        g_message_cycle->Stop();
    }

    // 先关闭线程池：工作线程中的任务持有 MessageCycle 指针和连接，
    // 必须在销毁 MessageCycle 之前执行完
    LOGGER_INFO("Shutting down ThreadPool...");
    if (ThreadPoolSingle::IsInitialized()) {
        if (ThreadPoolSingle::ShutDown(std::chrono::seconds(10))) {
            LOGGER_INFO("ThreadPool shut down successfully");
        } else {
            LOGGER_WARN("ThreadPool shutdown timeout, forcing stop");
        }
        ThreadPoolSingle::Destroy();
    }

    // MessageCycle 析构时会关闭监听 fd 和所有连接
    LOGGER_INFO("Closing server socket and all connections...");
    g_message_cycle.reset();
    ConnectionManager::GetInstance().CloseAll();
    g_socket_server.reset();

    // 清理 ZooKeeper 连接 - 在其他资源清理完成后
    LOGGER_INFO("Cleaning up ZooKeeper connections...");
    try {
        ZkHandler::GetInstance().clean_up();
    } catch (const std::exception& e) {
        LOGGER_ERROR("Exception during ZooKeeper cleanup: {}", e.what());
    } catch (...) {
        LOGGER_ERROR("Unknown exception during ZooKeeper cleanup");
    }

    LOGGER_INFO("Graceful shutdown completed");
}

// 初始化并运行服务端，返回进程退出码
static int RunServer(const std::string& config_filename)
{
    // 初始化配置（同时初始化 logger / thread_pool / register / ZooKeeper 配置）
    auto& rpc_server_config = RpcServerConfig::GetInstance();
    if (!rpc_server_config.Init(config_filename)) {
        LOGGER_ERROR("load config failed: {}", config_filename);
        return 1;
    }

    // 按配置重新初始化日志
    const auto& logger_config = LoggerConfig::GetInstance();
    const std::string log_file = logger_config.GetLogFile().empty()
        ? std::string("../log") : logger_config.GetLogFile();
    const std::string log_level = logger_config.GetLogPattern().empty()
        ? std::string("info") : logger_config.GetLogPattern();
    if (!Logger::GetInstance().Init(log_file, log_level)) {
        std::cerr << "Failed to initialize logger" << std::endl;
        return 1;
    }

    // 初始化线程池 - 在服务器启动时初始化
    LOGGER_INFO("Initializing ThreadPool...");
    const auto& thread_pool_config = rpc_server_config.GetThreadPoolConfig();
    bool pool_inited = false;
    if (thread_pool_config.GetMaxCoreThreads() > 0) {
        ThreadPoolConfig config;
        config.core_threads = thread_pool_config.GetCoreThreads();
        config.max_core_threads = thread_pool_config.GetMaxCoreThreads();
        config.max_queue_size = thread_pool_config.GetMaxQueueSize();
        config.keep_alive_time = thread_pool_config.GetAliveTime();
        pool_inited = ThreadPoolSingle::Init(config);
    } else {
        // 配置文件中没有 thread_pool 字段，使用默认线程数
        pool_inited = ThreadPoolSingle::Init();
    }
    if (!pool_inited) {
        LOGGER_WARN("ThreadPool already initialized or failed to initialize");
    } else {
        LOGGER_INFO("ThreadPool initialized successfully with {} threads", ThreadPoolSingle::WorkerSize());
    }

    // 创建 socket 服务器（bind + listen）
    g_socket_server = CreateSocket::Create(rpc_server_config);
    if (!g_socket_server) {
        LOGGER_ERROR("create socket server failed");
        return 1;
    }

    // 创建消息循环
    g_message_cycle = std::make_unique<MessageCycle>(&ConnectionManager::GetInstance());

    // MessageCycle 构造时会注册自己的 SIGINT 处理，这里在它之后注册以覆盖
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    // 添加监听套接字
    if (!g_message_cycle->AddListenFd(g_socket_server->GetFd())) {
        LOGGER_ERROR("add server socket to message cycle failed");
        return 1;
    }

    // 在服务器启动时注册服务
    if (!ServiceManager::GetInstance().RegisterService(std::make_shared<RpcService>())) {
        LOGGER_ERROR("Failed to register rpc service");
        return 1;
    }

    // 向 ZooKeeper 注册服务实例
    if (!ZkHandler::GetInstance().RegisterServiceConfig(rpc_server_config.GetRegisterConfig())) {
        LOGGER_ERROR("Failed to register services to ZooKeeper");
        return 1;
    }

    // 初始化期间收到信号，直接退出
    if (g_shutdown_requested.load()) {
        return 0;
    }

    LOGGER_INFO("Server starting, listening on {}:{}",
        rpc_server_config.GetServiceIp(), rpc_server_config.GetServicePort());

    // 运行消息循环，收到信号后 Stop 使其退出
    g_message_cycle->Loop();
    LOGGER_INFO("Message loop ended, starting shutdown process");
    return 0;
}

int main(int argc, char* argv[])
{
    // 先用默认参数初始化日志，保证加载配置阶段的日志能输出
    if (!Logger::GetInstance().Init()) {
        std::cerr << "Failed to initialize logger" << std::endl;
        return 1;
    }

    // 可通过命令行指定配置文件，默认从 build 目录启动
    const std::string config_filename = argc > 1 ? argv[1] : "../config/rpc_server.json";

    int ret = 0;
    try {
        ret = RunServer(config_filename);
    } catch (const std::exception& e) {
        LOGGER_ERROR("Exception in main: {}", e.what());
        ret = 1;
    } catch (...) {
        LOGGER_ERROR("Unknown exception in main");
        ret = 1;
    }

    // 所有退出路径统一清理
    g_shutdown_requested.store(true);
    GracefulShutdown();
    return ret;
}
