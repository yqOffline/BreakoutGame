cmake_minimum_required(VERSION 3.10)
project(game)

# 设置 C++ 标准（你的代码使用了 C++11/14 特性）
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置 C 标准（Raylib 内部使用 C11）
set(CMAKE_C_STANDARD 11)

# 使用 pkg-config 查找 raylib
find_package(PkgConfig REQUIRED)
pkg_check_modules(raylib REQUIRED raylib)

# 添加头文件搜索路径和库搜索路径
include_directories(${raylib_INCLUDE_DIRS})
link_directories(${raylib_LIBRARY_DIRS})

# 定义源文件列表（请根据你的实际情况核对文件名）
set(SOURCES
    main_2.cpp
    Ball.cpp
    Paddle.cpp
    Brick.cpp
    Game.cpp
    LevelManager.cpp
    Particle.cpp
    SkillBall.cpp
    Effect.cpp
    EffectFactory.cpp
    SoundManager.cpp
)

# 生成可执行文件
add_executable(game ${SOURCES})

# 链接 raylib 库
target_link_libraries(game ${raylib_LIBRARIES})