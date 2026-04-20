#include "raylib.h"
#include "game.h"

int main() {
    const int screenWidth = 800;
    const int screenHeight = 600;
    InitWindow(screenWidth, screenHeight, "2DBREAKOUT!!!");
    InitAudioDevice();
    SetTargetFPS(60);

    // 创建游戏核心对象
    Game game(screenWidth, screenHeight);

    // 游戏主循环
    while (game.IsGameRunning()) {
        Vector2 mousePos = GetMousePosition();
        game.HandleInput(mousePos);
        game.Update(GetFrameTime());
        
        BeginDrawing();
        ClearBackground(RAYWHITE);
        game.Draw();
        EndDrawing();
    }

    CloseAudioDevice();
    CloseWindow();
    return 0;
}