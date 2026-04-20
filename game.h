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
#include "SoundManager.h"
#include <deque>
#include <vector>
#include <memory>
#include <string>
#include <algorithm>

#include "json.hpp"
using json = nlohmann::json;

enum class GameState {
    MENU,
    PLAYING,
    PAUSED,
    GAME_OVER,
    LEVEL_CLEAR,
    VICTORY,
    RANKING
};

enum class PauseCause {
    MANUAL_PAUSE,
    LIFE_LOSS_PAUSE
};

struct RankRecord {
    float time;
    int deaths;
    std::string timestamp;
    
    bool operator<(const RankRecord& other) const {
        if (time != other.time) return time < other.time;
        return deaths < other.deaths;
    }
};

class Game {
private:
    json config;

    std::vector<Ball> balls;
    Paddle paddle;
    std::vector<Brick> bricks;

    int score;
    int hearts;
    float paddleMoveSpeed;

    Rectangle redLine;
    Rectangle startBtn;
    Rectangle continueBtn;
    Rectangle restartBtn;
    Rectangle gameOverRestartBtn;
    Rectangle replayBtn;
    Rectangle goAheadBtn;
    Rectangle victoryRestartBtn;
    Rectangle victoryReplayBtn;
    Rectangle rankBtn;
    Rectangle eraseRankBtn;
    Rectangle backBtn;

    Texture2D backgroundTex;
    Texture2D paddleTex;
    bool bgLoaded;
    bool paddleLoaded;

    GameState currentState;
    PauseCause pauseCause;

    std::vector<std::deque<Vector2>> ballTrails;
    ParticleSystem particleSystem;
    std::vector<SkillBall> skillBalls;
    LevelManager levelManager;

    std::vector<std::unique_ptr<Effect>> activeEffects;  // 改为容器

    float originalPaddleWidth;
    float originalBallRadius;

    float skillDropChance;
    float skillBallSpeedY;
    float skillBallRadius;

    int particlesPerBrick;
    float particleGravity;

    int maxTrailLength;

    int currentLevel = 0;
    int totalLevels = 0;

    bool lastBrickAnimating = false;
    int lastBrickIndex = -1;
    Rectangle lastBrickStartRect;
    Rectangle lastBrickTargetRect;
    float lastBrickAnimTimer = 0.0f;
    const float lastBrickAnimDuration = 1.0f;

    SoundManager soundManager;

    float gameTimer;
    int totalDeaths;
    bool timerRunning;

    std::vector<RankRecord> rankList;
    static const int MAX_RANK_COUNT = 5;
    const std::string rankFileName = "rank.dat";

    void ResetBricks();
    void CheckBallHitRedLine();
    void LoadLevel(int index);
    void ApplyEffect(std::unique_ptr<Effect> effect);
    void UpdateEffects(float dt);
    void CheckLevelTransition();
    void HandleBallCollisions();

    void LoadRanking();
    void SaveRanking();
    void AddVictoryRecord();
    void ResetGameState();
    void ClearRanking();

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

    Paddle& GetPaddle() { return paddle; }
    std::vector<Ball>& GetBalls() { return balls; }
    float GetOriginalPaddleWidth() const { return originalPaddleWidth; }
    float GetOriginalBallRadius() const { return originalBallRadius; }
    void AddBallTrail() { ballTrails.emplace_back(); }
    
    bool HasEffectOfType(const std::string& typeName) const;
    const std::vector<std::unique_ptr<Effect>>& GetActiveEffects() const { return activeEffects; }
};

#endif