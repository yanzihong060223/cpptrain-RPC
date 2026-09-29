cmake_minimum_required(VERSION 3.16)

# 未指定工具链时，默认使用 ~/vcpkg 的工具链（需在 project() 之前设置）
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE AND EXISTS "$ENV{HOME}/vcpkg/scripts/buildsystems/vcpkg.cmake")
    set(CMAKE_TOOLCHAIN_FILE "$ENV{HOME}/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

project(yan_rpc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

find_package(Threads REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(protobuf CONFIG REQUIRED)
find_package(unofficial-zookeeper CONFIG REQUIRED)
find_package(zstd CONFIG REQUIRED)

# 框架库：除两个 main 之外的全部源文件
file(GLOB_RECURSE RPC_SOURCES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/rpc_src/*.cpp")
list(REMOVE_ITEM RPC_SOURCES
    "${PROJECT_SOURCE_DIR}/rpc_src/core/servermain.cpp"
    "${PROJECT_SOURCE_DIR}/rpc_src/core/clientmain.cpp"
)

add_library(yan_rpc STATIC ${RPC_SOURCES})

target_include_directories(yan_rpc PUBLIC
    ${PROJECT_SOURCE_DIR}                   # log_manager.h
    ${PROJECT_SOURCE_DIR}/rpc_src
    ${PROJECT_SOURCE_DIR}/rpc_src/thread_pool # service_manager.h 直接 include "thread_pool_single.h"
)

# vcpkg 的 zookeeper 是多线程版，但没导出 THREADED，
# 不定义的话头文件里没有 zoo_get / zoo_get_children 等同步接口
target_compile_definitions(yan_rpc PUBLIC THREADED)

target_compile_options(yan_rpc PRIVATE -Wall -Wextra)

target_link_libraries(yan_rpc PUBLIC
    Threads::Threads
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    protobuf::libprotobuf
    unofficial::zookeeper::zookeeper
    $<IF:$<TARGET_EXISTS:zstd::libzstd_shared>,zstd::libzstd_shared,zstd::libzstd_static>
)

add_executable(rpc_server rpc_src/core/servermain.cpp)
target_link_libraries(rpc_server PRIVATE yan_rpc)

add_executable(rpc_client rpc_src/core/clientmain.cpp)
target_link_libraries(rpc_client PRIVATE yan_rpc)

add_executable(stress_client stress_test/stress_client.cpp)
target_link_libraries(stress_client PRIVATE yan_rpc)
