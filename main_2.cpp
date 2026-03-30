#include "raylib.h"
#include "game.h"

int main() {
    const int screenWidth = 800;
    const int screenHeight = 600;
    InitWindow(screenWidth, screenHeight, "2DBREAKOUT！！！");
    SetTargetFPS(60);

    // 创建游戏核心对象
    Game game(screenWidth, screenHeight);

    // 游戏主循环
    while (game.IsGameRunning()) {
        // 获取鼠标位置
        Vector2 mousePos = GetMousePosition();
        
        // 1. 处理输入
        game.HandleInput(mousePos);
        
        // 2. 更新游戏逻辑（传入deltaTime，当前未使用，预留扩展）
        game.Update(GetFrameTime());
        
        // 3. 绘制游戏内容
        BeginDrawing();
        ClearBackground(RAYWHITE); // 兜底背景（背景纹理会覆盖）
        game.Draw();
        EndDrawing();
    }

    // 窗口关闭，自动调用Game析构函数释放资源
    CloseWindow();
    return 0;
}