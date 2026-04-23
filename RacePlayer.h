#ifndef RACE_PLAYER_H
#define RACE_PLAYER_H

#include "raylib.h"
#include "Ball.h"
#include "Paddle.h"
#include "Brick.h"
#include "SkillBall.h"
#include "Particle.h"
#include "LevelManager.h"
#include "Effect.h"
#include "SoundManager.h"
#include <deque>
#include <vector>
#include <memory>
#include <string>
#include "json.hpp"

using json = nlohmann::json;

enum class RacePlayerState {
    PLAYING,
    PAUSED,         // 仅由全局暂停触发，生命损失不再暂停
    GAME_OVER,
    LEVEL_CLEAR,
    VICTORY
};

class RacePlayer {
public:
    RacePlayer(int screenWidth, int screenHeight, const json& config, bool isLeftSide);
    ~RacePlayer();

    void LoadLevel(int index);
    void ResetForNewGame();

    void HandleInput();
    void Update(float dt);
    void Draw();

    bool IsGameOver() const { return state == RacePlayerState::GAME_OVER; }
    bool IsVictory() const { return state == RacePlayerState::VICTORY; }
    int  GetScore() const { return score; }
    int  GetHearts() const { return hearts; }
    float GetGameTime() const { return gameTimer; }
    int  GetDeaths() const { return totalDeaths; }

    void SetPaused(bool paused);
    void StartGame();
    void SetOpponentPaddleX(float x);       // 设置远端挡板位置
    void ForceGameOver();
    void ForceVictory();

    float GetPaddleX() const { return paddle.GetRectangle().x; }
    void SetRandomSeed(unsigned int seed);

private:
    json config;
    int screenWidth, screenHeight;
    bool isLeftSide;

    std::vector<Ball> balls;
    Paddle paddle;
    std::vector<Brick> bricks;
    std::vector<SkillBall> skillBalls;
    ParticleSystem particleSystem;
    std::vector<std::deque<Vector2>> ballTrails;

    LevelManager levelManager;
    std::vector<std::unique_ptr<Effect>> activeEffects;

    SoundManager soundManager;

    int score;
    int hearts;
    float paddleMoveSpeed;

    Rectangle redLine;

    float originalPaddleWidth;
    float originalBallRadius;

    float skillDropChance;
    float skillBallSpeedY;
    float skillBallRadius;
    int particlesPerBrick;
    float particleGravity;
    int maxTrailLength;

    int currentLevel;
    int totalLevels;

    RacePlayerState state;
    bool timerRunning;
    float gameTimer;
    int totalDeaths;

    float opponentPaddleX;
    bool hasOpponentPaddle;

    void CheckBallHitRedLine();
    void ApplyEffect(std::unique_ptr<Effect> effect);
    void UpdateEffects(float dt);
    void CheckLevelTransition();
    void HandleBallCollisions();
    bool HasEffectOfType(const std::string& typeName) const;
};

#endif