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
    PAUSED,
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

    void HandleInput();                     // 本地玩家输入（左右键移动板）
    void Update(float dt);
    void Draw();

    // 状态查询
    bool IsGameOver() const { return state == RacePlayerState::GAME_OVER; }
    bool IsVictory() const { return state == RacePlayerState::VICTORY; }
    RacePlayerState GetState() const { return state; }
    int GetScore() const { return score; }
    int GetHearts() const { return hearts; }
    float GetGameTime() const { return gameTimer; }
    int GetDeaths() const { return totalDeaths; }
    int GetCurrentLevel() const { return currentLevel; }

    // 网络控制
    void SetPaused(bool paused);
    void StartGame();                       // 给球初速度
    void SetOpponentPaddleX(float x);       // 设置对手板位置（用于绘制）
    void ForceGameOver();                   // 强制结束（对方退出）
    void ForceVictory();                    // 强制胜利（用于显示）

    // 获取本地板位置（用于发送）
    float GetPaddleX() const { return paddle.GetRectangle().x; }


    // 重置随机种子（用于同步初始状态）
    void SetRandomSeed(unsigned int seed);
    //void SetPaddleX(float x) { paddle.SetRect({x, paddle.GetRectangle().y, paddle.GetWidth(), paddle.GetRectangle().height}); }

private:
    json config;
    int screenWidth, screenHeight;
    bool isLeftSide;                        // 用于调整绘制偏移

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

    // 对手板（仅用于绘制）
    float opponentPaddleX;
    bool hasOpponentPaddle;

    // 辅助函数
    void CheckBallHitRedLine();
    void ApplyEffect(std::unique_ptr<Effect> effect);
    void UpdateEffects(float dt);
    void CheckLevelTransition();
    void HandleBallCollisions();
    bool HasEffectOfType(const std::string& typeName) const;
};

#endif