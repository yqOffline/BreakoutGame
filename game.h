#ifndef GAME_H
#define GAME_H

#include "raylib.h"
#include "Ball.h"
#include "Paddle.h"
#include "Brick.h"
#include "SkillBall.h"
#include "Particle.h"
#include "LevelManager.h"
#include <deque>
#include <vector>
#include <fstream>
#include <iostream>

#include "json.hpp"
using json = nlohmann::json;

enum class GameState {
    MENU,
    PLAYING,
    PAUSED,
    GAME_OVER,
    LEVEL_CLEAR
};

enum class PauseCause {
    MANUAL_PAUSE,
    LIFE_LOSS_PAUSE
};

class Game {
private:
    json config;

    // 游戏对象
    std::vector<Ball> balls;          // 改为多球
    Paddle paddle;
    std::vector<Brick> bricks;

    // 游戏数据
    int score;
    int hearts;
    float paddleMoveSpeed;

    // UI
    Rectangle redLine;
    Rectangle startBtn;
    Rectangle continueBtn;
    Rectangle restartBtn;
    Rectangle gameOverRestartBtn;
    Rectangle replayBtn;              // 新增
    Rectangle goAheadBtn;             // 新增

    // 纹理
    Texture2D backgroundTex;
    Texture2D paddleTex;
    bool bgLoaded;
    bool paddleLoaded;

    // 状态机
    GameState currentState;
    PauseCause pauseCause;

    // 系统
    std::vector<std::deque<Vector2>> ballTrails;  // 每个球单独拖尾
    ParticleSystem particleSystem;
    std::vector<SkillBall> skillBalls;
    LevelManager levelManager;

    // Buff 系统
    float buffTimer = 0.0f;
    SkillType activeBuffType = SkillType::PADDLE_EXTEND; // 初始值，实际会被覆盖
    float originalPaddleWidth;
    float originalBallRadius;
    bool buffActive = false;

    // 技能球配置
    float skillDropChance;
    float skillBallSpeedY;
    float skillBallRadius;
    float buffDuration;
    float paddleExtendFactor;
    float ballEnlargeFactor;
    float ballShrinkFactor;

    // 粒子配置
    int particlesPerBrick;
    float particleGravity;

    // 拖尾配置
    int maxTrailLength;

    // 关卡
    int currentLevel = 0;
    int totalLevels = 0;

    // 最后砖块动画相关
    bool lastBrickAnimating = false;
    int lastBrickIndex = -1;
    Rectangle lastBrickStartRect;
    Rectangle lastBrickTargetRect;
    float lastBrickAnimTimer = 0.0f;
    const float lastBrickAnimDuration = 1.0f;

    // 私有方法
    void ResetBricks();
    void CheckBallHitRedLine();
    void LoadLevel(int index);
    void ApplySkillEffect(SkillType type);
    void UpdateBuffs(float dt);
    void CheckLevelTransition();
    void HandleBallCollisions(); // 球间碰撞

public:
    Game(int screenWidth, int screenHeight);
    ~Game();
    void ResetGame();

    void HandleInput(Vector2 mousePos);
    void Update(float dt);
    void Draw();

    bool IsGameRunning() const;
    GameState GetState() const { return currentState; }
    int GetHearts() const { return hearts; }
    void SimulateBallDrop() { CheckBallHitRedLine(); }
};

#endif