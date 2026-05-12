cmake_minimum_required(VERSION 3.10)
project(game)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

find_package(PkgConfig REQUIRED)
pkg_check_modules(raylib REQUIRED raylib)

# 查找 ENet
pkg_check_modules(ENET REQUIRED libenet)

include_directories(${raylib_INCLUDE_DIRS} ${ENET_INCLUDE_DIRS})
link_directories(${raylib_LIBRARY_DIRS} ${ENET_LIBRARY_DIRS})

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
    RacePlayer.cpp       # 新增
    RaceManager.cpp      # 新增
    VersusGame.cpp 
    VersusManager.cpp 
    TextureCache.cpp
    NetworkThread.cpp
    ThreadPool.cpp
    Grid.cpp
)

add_executable(game ${SOURCES})
target_link_libraries(game ${raylib_LIBRARIES} ${ENET_LIBRARIES})