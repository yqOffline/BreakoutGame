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
#include "VersusNetMessage.h"
#include "json.hpp"
#include <vector>
#include <deque>
#include <memory>
#include <random>

using json = nlohmann::json;

class VersusGame {
public:
    VersusGame(int screenWidth, int screenHeight, const json& config);
    ~VersusGame() = default;

    void LoadLevel(unsigned int seed);
    void Update(float dt);
    void Draw();

    void MovePaddle(bool isUpper, int direction);
    void TryLaunchBall(bool isUpper);
    void SetBallAttachedToUpper(bool upper) { ballAttachedToUpper = upper; }   // 新增

    GameStateSnapshot GetSnapshot() const;
    void ApplySnapshot(const GameStateSnapshot& snap);
    void ApplyInterpolatedState(const GameStateSnapshot& prev, const GameStateSnapshot& next, float t);

    void ApplyEffectToPlayer(std::unique_ptr<Effect> effect, bool upper);
    void UpdateEffects(float dt);
    void UpdateVisuals(float dt);

    int GetUpperLives() const { return upperLives; }
    int GetLowerLives() const { return lowerLives; }
    bool IsBallAttached() const { return waitingForLaunch; }
    bool IsUpperWaitingLaunch() const { return waitingForLaunch && ballAttachedToUpper; }
    bool IsLowerWaitingLaunch() const { return waitingForLaunch && !ballAttachedToUpper; }
    Color GetBallColor() const { return ballColor; }

    ParticleSystem& GetParticleSystem() { return particleSystem; }
    void AddSkillBall(const SkillBall& sb) { skillBalls.push_back(sb); }

    std::function<void()> OnLifeLost;
    std::function<void(bool)> OnBallLaunched;
    std::function<void(EffectType, bool)> OnEffectApplied;
    std::function<void(EffectType, bool)> OnEffectRemoved;
    std::function<void(const SkillBall&)> OnSkillBallSpawned;
    std::function<void(Vector2, Color, int, bool)> OnParticleSpawned;

    Vector2 lastSnapshotSpeed;

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
    bool ballOwnedByUpper;

    std::vector<std::unique_ptr<Effect>> upperEffects;
    std::vector<std::unique_ptr<Effect>> lowerEffects;

    float paddleMoveSpeed;
    float ballInitSpeedX, ballInitSpeedY;
    float skillBallSpeedY;
    float skillBallRadius;
    float skillDropChance;
    int particlesPerBrick;
    float particleGravity;
    int maxTrailLength;

    float originalUpperPaddleWidth;
    float originalLowerPaddleWidth;
    float originalBallRadius;

    SoundManager soundManager;
    std::mt19937 rng;

    float m_gameTimer = 0.0f;

    void HandleBallEdgeBounce();
    void HandlePaddleCollision(Paddle& paddle, bool isUpper);
    void HandleBrickCollision();
    void HandleRedLine();
    Color skillBallColorForType(SkillType type) const;
};

#endif