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
#include <iostream>   // 用于错误输出

#include "json.hpp"
using json = nlohmann::json;

// ======================== 【状态机核心】4种状态 ========================
enum class GameState {
    MENU,       // 主菜单
    PLAYING,    // 游戏进行
    PAUSED,     // 暂停
    GAME_OVER   // 游戏结束
};

enum class PauseCause {
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
    float paddleMoveSpeed;   // 修正类型：应为 float

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

    // 新增系统
    std::deque<Vector2> ballTrail;
    ParticleSystem particleSystem;
    std::vector<SkillBall> skillBalls;
    LevelManager levelManager;      // 使用默认构造，后续加载配置

    // Buff 计时与状态
    float buffTimer = 0.0f;
    SkillType activeBuffType;
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

    // 关卡相关
    int currentLevel = 0;
    int totalLevels = 0;

    // 私有方法
    void LoadLevel(int index);
    void ApplySkillEffect(SkillType type);
    void UpdateBuffs(float dt);
    void CheckLevelTransition();

public:
    Game(int screenWidth, int screenHeight);
    ~Game();
    void ResetGame();

    // 状态机三大核心方法
    void HandleInput(Vector2 mousePos);
    void Update(float dt);
    void Draw();

    bool IsGameRunning() const;
    GameState GetState() const { return currentState; }
    int GetHearts() const { return hearts; }
    void SimulateBallDrop() { CheckBallHitRedLine(); }
};

#endif