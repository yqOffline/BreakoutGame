#ifndef GAME_H
#define GAME_H

#include "raylib.h"
#include "Ball.h"
#include "Paddle.h"
#include "Brick.h"
#include <vector>
#include <fstream>
#include "json.hpp"

using json = nlohmann::json;

// ======================== 【状态机核心】4种状态 ========================
enum class GameState {  /*定义强类型枚举,预防枚举重名的情况*/
    MENU,       // 主菜单
    PLAYING,    // 游戏进行
    PAUSED,     // 暂停
    GAME_OVER   // 游戏结束
};

enum class PauseCause{
    MANUAL_PAUSE,
    LIFE_LOSS_PAUSE
};

class Game {
private:
    json config;

    // 游戏对象
    Ball ball;
    Paddle paddle;
    std::vector<Brick> bricks;

    // 游戏数据
    int score;
    int hearts;
    int paddleMoveSpeed;

    // 砖块配置
    int brickRows;
    int brickCols;
    float brickWidth;
    float brickHeight;
    float brickSpacing;
    float brickStartY;
    std::vector<Color> brickRowColors;

    // UI
    Rectangle redLine;
    Rectangle startBtn;
    Rectangle continueBtn;
    Rectangle restartBtn;
    Rectangle gameOverRestartBtn;

    // 纹理
    Texture2D backgroundTex;
    Texture2D paddleTex;
    bool bgLoaded;
    bool paddleLoaded;

    // 状态机当前状态
    GameState currentState;
    PauseCause pauseCause;

    // 私有方法
    void ResetBricks();
    void CheckBallHitRedLine();
    void CheckGameVictory();

public:
    Game(int screenWidth, int screenHeight);
    ~Game();
    void ResetGame();  // 重置游戏（状态机专用）

    // 状态机三大核心方法
    void HandleInput(Vector2 mousePos);  // 输入处理
    void Update(float dt);               // 逻辑更新
    void Draw();                         // 画面绘制

    bool IsGameRunning() const;
    GameState GetState() const { return currentState; }
    int GetHearts() const {return hearts;}
    void SimulateBallDrop(){CheckBallHitRedLine();}
};

#endif