// 压测客户端：单进程单连接，顺序发送 Echo 请求，把每次请求的耗时写入结果文件
// 用法: stress_client <config> <client_id> <requests> <duration_sec> <result_file>
//   requests > 0     按次数发送
//   duration_sec > 0 在限定时间内持续发送（requests 为 0 时生效）
#include "core/rpc_client.h"
#include "log_manager.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdio>
#include <exception>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using json = nlohmann::json;
using namespace yan_rpc;
using Clock = std::chrono::steady_clock;

int main(int argc, char* argv[])
{
    if (argc < 6) {
        std::cerr << "usage: " << argv[0]
                  << " <config> <client_id> <requests> <duration_sec> <result_file>\n";
        return 2;
    }

    const std::string config_file = argv[1];
    const int client_id = std::stoi(argv[2]);
    const long requests = std::stol(argv[3]);
    const long duration_sec = std::stol(argv[4]);
    const std::string result_file = argv[5];

    long success = 0;
    long failed = 0;
    long reconnects = 0;
    std::vector<long> latencies_us;
    latencies_us.reserve(requests > 0 ? requests : 100000);

    const auto start = Clock::now();
    const auto deadline = start + std::chrono::seconds(duration_sec);

    try {
        RpcClient client(config_file);

        for (long i = 0;; ++i) {
            if (requests > 0 ? i >= requests : Clock::now() >= deadline) {
                break;
            }

            json request;
            request["message"] = "stress client " + std::to_string(client_id) +
                                 " req " + std::to_string(i);
            json response;

            const auto t0 = Clock::now();
            const bool ok = client.Call<json, json>(
                "DefaultService", "Echo", request, response, SerType::Json);
            const auto t1 = Clock::now();

            // 校验回显内容，防止把错位的响应算作成功
            if (ok && response.value("received_message", "") == request["message"]) {
                ++success;
                latencies_us.push_back(
                    std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count());
            } else {
                ++failed;
                // 连接断开后重连，失败则放弃剩余请求
                if (!client.IsConnected()) {
                    ++reconnects;
                    if (!client.ReConnect()) {
                        LOGGER_ERROR("client {} reconnect failed", client_id);
                        if (requests > 0) {
                            failed += requests - i - 1;
                        }
                        break;
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "client " << client_id << " init failed: " << e.what() << "\n";
        failed += requests > 0 ? requests - success - failed : 1;
    }

    const double elapsed_sec =
        std::chrono::duration<double>(Clock::now() - start).count();

    // 结果文件格式：首行汇总，其后每行一个成功请求的耗时(us)
    std::ofstream out(result_file);
    out << "SUMMARY " << success << " " << failed << " " << reconnects
        << " " << elapsed_sec << "\n";
    for (const long us : latencies_us) {
        out << us << "\n";
    }

    return failed == 0 ? 0 : 1;
}
