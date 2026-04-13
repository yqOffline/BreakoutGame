#include "raylib.h"
#include "game.h"
#include "nlohmann/json.hpp"
#include <fstream>

using json = nlohmann::json;

int main() {
    // 读取配置文件
    std::ifstream config_file("config.json");
    json config;
    config_file >> config;

    // 初始化窗口
    const int screenWidth = config["screen"]["width"];
    const int screenHeight = config["screen"]["height"];
    InitWindow(screenWidth, screenHeight, "Brick Breaker - 关卡版");
    SetTargetFPS(60);
    InitAudioDevice();

    // 适配新Game类（无参构造 + Init初始化）
    Game game;
    game.Init(config);

    // 主循环
    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(BLACK);

        // 调用新接口（无参）
        game.HandleInput();
        game.Update();
        game.Draw();

        EndDrawing();
    }

    CloseAudioDevice();
    CloseWindow();
    return 0;
}