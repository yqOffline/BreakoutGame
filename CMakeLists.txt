cmake_minimum_required(VERSION 3.10)
project(BreakoutGame)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加 raylib 配置 (根据你的环境调整)
find_package(raylib REQUIRED)

# 1. 包含 nlohmann/json 头文件 (通过 FetchContent 自动下载)
# 这种方式最推荐，不需要你手动下载文件，CMake 会自动处理
include(FetchContent)
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/archive/refs/tags/v3.11.3.tar.gz
)
FetchContent_MakeAvailable(json)

# 添加可执行文件
add_executable(raylib 
    main_2.cpp 
    game.cpp 
    Ball.cpp 
    Brick.cpp 
    Paddle.cpp
)

# 2. 链接库
# 链接 raylib 和 nlohmann_json
target_link_libraries(raylib PRIVATE raylib nlohmann_json::nlohmann_json)