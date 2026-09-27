#include "rpc_client.h"
#include "../../log_manager.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <exception>
#include <future>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using json = nlohmann::json;
using namespace yan_rpc;

void TestSync(RpcClient& client)
{
    json request;
    request["message"] = "hi i am rpc client";

    json response;
    const bool success = client.Call<json, json>(
        "DefaultService",
        "Echo",
        request,
        response,
        SerType::Json
    );

    if (success) {
        std::cout << response.dump(4) << "\n";
        return;
    }

    LOGGER_ERROR("Call Failed");
}

void TestAsync(RpcClient& client)
{
    std::vector<std::future<json>> futures;
    futures.reserve(300);

    for (int i = 0; i < 300; ++i) {
        json request;
        request["message"] = "Hello from async client " + std::to_string(i);

        futures.push_back(
            client.AsyncCall<json, json>(
                "DefaultService",
                "Echo",
                request,
                SerType::Json
            )
        );

        std::cout << "I am doing work " << i << "\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    for (std::size_t i = 0; i < futures.size(); ++i) {
        try {
            json response = futures[i].get();
            std::cout
                << "Async call " << i
                << " succeeded, response: "
                << response.dump()
                << std::endl;
        } catch (const std::exception& e) {
            std::cout
                << "Async call " << i
                << " failed: "
                << e.what()
                << std::endl;
        }
    }
}

int main(int argc, char* argv[])
{
    const std::string config_file =
        argc > 1 ? argv[1] : "../config/rpc_client.json";

    try {
        RpcClient client(config_file);
        std::cout << "Client Connect\n";
        TestSync(client);
        TestAsync(client);
    } catch (const std::exception& e) {
        std::cout << e.what() << "\n";
        return 1;
    }

    return 0;
}
