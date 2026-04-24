// VersusGame.h
#ifndef VERSUS_GAME_H
#define VERSUS_GAME_H

#include "raylib.h"
#include "Ball.h"
#include "Paddle.h"
#include "Brick.h"
#include "SkillBall.h"
#include "Particle.h"
#include "Effect.h"
#include "SoundManager.h"
#include "json.hpp"
#include <vector>
#include <deque>
#include <memory>
#include <random>

using json = nlohmann::json;   // 必须添加，否则 json 不可用

// 前置声明，避免循环依赖
struct GameStateSnapshot;

class VersusGame {
public:
    VersusGame(int screenWidth, int screenHeight, const json& config);
    ~VersusGame() = default;

    void LoadLevel(unsigned int seed);
    void Update(float dt);
    void Draw();

    void MovePaddle(bool isUpper, int direction);
    void TryLaunchBall(bool isUpper);

    GameStateSnapshot GetSnapshot() const;
    void ApplySnapshot(const GameStateSnapshot& snap);

    void ApplyEffectToPlayer(std::unique_ptr<Effect> effect, bool upper);
    void UpdateEffects(float dt);

    // 查询
    int GetUpperLives() const { return upperLives; }
    int GetLowerLives() const { return lowerLives; }
    bool IsBallAttached() const { return waitingForLaunch; }
    bool IsUpperWaitingLaunch() const { return waitingForLaunch && ballAttachedToUpper; }
    bool IsLowerWaitingLaunch() const { return waitingForLaunch && !ballAttachedToUpper; }
    Color GetBallColor() const { return ballColor; }

    // 回调
    std::function<void()> OnLifeLost;
    std::function<void(bool)> OnBallLaunched;
    std::function<void(EffectType, bool)> OnEffectApplied;
    std::function<void(EffectType, bool)> OnEffectRemoved;
    std::function<void(const SkillBall&)> OnSkillBallSpawned;

private:
    int screenWidth, screenHeight;
    json config;

    Ball ball;
    Paddle upperPaddle, lowerPaddle;
    Color ballColor = WHITE;

    std::vector<Brick> bricks;
    std::vector<SkillBall> skillBalls;
    ParticleSystem particleSystem;
    std::vector<std::deque<Vector2>> ballTrails;

    Rectangle upperRedLine, lowerRedLine;

    int upperLives, lowerLives;
    bool waitingForLaunch;
    bool ballAttachedToUpper;

    std::vector<std::unique_ptr<Effect>> upperEffects;
    std::vector<std::unique_ptr<Effect>> lowerEffects;

    // 配置
    float paddleMoveSpeed;
    float ballInitSpeedX, ballInitSpeedY;
    float skillBallSpeedY;
    float skillBallRadius;
    float skillDropChance;
    int particlesPerBrick;
    float particleGravity;
    int maxTrailLength;

    SoundManager soundManager;
    std::mt19937 rng;

    // 辅助
    void HandleBallEdgeBounce();
    void HandlePaddleCollision(Paddle& paddle, bool isUpper);
    void HandleBrickCollision();
    void HandleRedLine();
    Color skillBallColorForType(SkillType type) const;
};

#endif