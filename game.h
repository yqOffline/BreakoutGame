#ifndef GAME_H
#define GAME_H

#include "raylib.h"
#include "Ball.h"
#include "Paddle.h"
#include "Brick.h"
#include "SkillBall.h"
#include "Particle.h"
#include "LevelManager.h"
#include "Effect.h"
#include <deque>
#include <vector>
#include <memory>

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
    std::vector<Ball> balls;
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
    Rectangle replayBtn;
    Rectangle goAheadBtn;

    // 纹理
    Texture2D backgroundTex;
    Texture2D paddleTex;
    bool bgLoaded;
    bool paddleLoaded;

    // 状态机
    GameState currentState;
    PauseCause pauseCause;

    // 系统
    std::vector<std::deque<Vector2>> ballTrails;
    ParticleSystem particleSystem;
    std::vector<SkillBall> skillBalls;
    LevelManager levelManager;

    // 效果系统（工厂模式）
    std::unique_ptr<Effect> activeEffect;

    // 原始尺寸（用于恢复）
    float originalPaddleWidth;
    float originalBallRadius;

    // 技能球配置
    float skillDropChance;
    float skillBallSpeedY;
    float skillBallRadius;

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
    void ApplyEffect(std::unique_ptr<Effect> effect);
    void UpdateEffects(float dt);
    void CheckLevelTransition();
    void HandleBallCollisions();

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

    // 供 Effect 访问的方法
    Paddle& GetPaddle() { return paddle; }
    std::vector<Ball>& GetBalls() { return balls; }
    float GetOriginalPaddleWidth() const { return originalPaddleWidth; }
    float GetOriginalBallRadius() const { return originalBallRadius; }
    void AddBallTrail() { ballTrails.emplace_back(); }
    
    // 检查当前激活效果的类型（用于伤害判定等）
    bool HasEffectOfType(const std::string& typeName) const;
    const Effect* GetActiveEffect() const { return activeEffect.get(); }
};

#endif