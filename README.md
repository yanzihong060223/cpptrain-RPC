# yan_rpc
基于 C++17 的 RPC 学习项目，使用非阻塞 TCP、事件循环和线程池处理远程调用，借助 ZooKeeper 完成服务注册与发现。
## 核心功能
- **网络通信**：非阻塞 Socket、连接管理，Linux 使用 epoll，代码包含 macOS kqueue 适配。
- **业务处理**：线程池执行请求，支持任务优先级与线程动态扩展。
- **RPC 调用**：自定义二进制协议，同步调用与返回 `std::future` 的异步接口。
- **服务发现**：ZooKeeper 临时节点注册、实例发现，以及随机和轮询策略实现。
- **数据处理**：JSON / Protobuf 序列化封装、Zstd 压缩、spdlog 日志；当前示例使用 JSON。
调用流程：服务发现 → 请求封装 → 压缩与编码 → TCP 传输 → 服务分发 → 响应返回。
## 编译运行
环境要求：Linux、C++17 编译器、CMake ≥ 3.16。以下假设 vcpkg 已安装在 `$HOME/vcpkg`，使用 x86-64 Linux。
bash
git clone https://github.com/yanzihong060223/cpptrain-RPC.git
cd cpptrain-RPC
"$HOME/vcpkg/vcpkg" install nlohmann-json spdlog protobuf zookeeper zstd --triplet x64-linux
cmake -S yan_rpc -B yan_rpc/build \   
-DCMAKE_TOOLCHAIN_FILE="$HOME/vcpkg/scripts/buildsystems/vcpkg.cmake" \   
-DVCPKG_TARGET_TRIPLET=x64-linux \   
-DCMAKE_BUILD_TYPE=Release
cmake --build yan_rpc/build --parallel
￼
先启动 ZooKeeper，确保 `127.0.0.1:2181` 可访问。然后从仓库根目录分别打开两个终端：
bash
# 终端一：启动服务端，默认监听 8989
cd yan_rpc/build
./rpc_server ../config/rpc_server.json
￼
bash
# 终端二：待服务端输出 REGISTER SUCCESS 后运行客户端
cd yan_rpc/build
./rpc_client ../config/rpc_client.json
￼
客户端演示 `DefaultService.Echo` 的同步和异步调用，成功时响应包含发送的 `received_message`。示例约运行 30 秒或更久。
配置文件位于 `yan_rpc/config/`。当前发现路径为 `/yanrpc/UserService`，业务调用目标为 `DefaultService.Echo`；两者分别用于定位服务器和分发请求。
## 压力测试
ZooKeeper 可用且编译完成后，从仓库根目录执行：
bash
cd yan_rpc
bash stress_test.sh -c 10 -r 100 -n
￼
脚本依赖 Bash、Python 3 和 `ss`，通过 10 个客户端进程各发送 100 次请求，汇总成功率、QPS 和 P50/P90/P99 延迟。QPS 包含客户端初始化时间，延迟统计仅包含成功调用
