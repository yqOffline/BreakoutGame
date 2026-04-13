#ifndef GAME_H
#define GAME_H

#include "Ball.h"
#include "Paddle.h"
#include "Brick.h"
#include <vector>
#include "raylib.h"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

// ===================== 强类型枚举：游戏状态 =====================
enum class GameState {
    MENU,
    LEVEL_SELECT,
    PLAYING,
    PAUSED,
    GAME_OVER,
    LEVEL_VICTORY,
    GAME_VICTORY
};

// ===================== 强类型枚举：暂停原因（保留你的修复） =====================
enum class PauseCause {
    MANUAL_PAUSE,
    LIFE_LOSS_PAUSE
};

class Game {
public:
    Game();
    ~Game();
    void Init(const json& config);
    void HandleInput();
    void Update();
    void Draw();
    void ResetGame();

private:
    void LoadLevel(int level);
    void CheckLevelComplete();
    void ResetCurrentLevel();

    void DrawPauseMenu();
    void DrawGameOver();
    void DrawUI();
    void DrawLevelSelect();
    void DrawLevelVictory();
    void DrawAllVictory();

    // 成员变量完全不变
    GameState state;
    PauseCause pauseCause;
    Ball ball;
    Paddle paddle;
    std::vector<Brick> bricks;

    int currentLevel;
    int unlockedLevel;
    json levelConfigs[3];
    int totalBricks;

    int score;
    int lives;
    int screenWidth, screenHeight;
    float bottomLineY;
    Texture2D backgroundTex;
    bool isTextureLoaded;
};

#endif