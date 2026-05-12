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
#include "Grid.h"
#include "TrailBuffer.h"
#include <deque>
#include <vector>
#include <memory>
#include <string>
#include <algorithm>
#include <atomic>
#include <future>

#include "json.hpp"
using json = nlohmann::json;

enum class GameState {
    MODE_SELECT,
    SINGLE_MENU,
    MULTIPLAYER_MENU,
    MULTIPLAYER_LOBBY,
    MULTIPLAYER_READY,
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

enum class MultiplayerSubState {
    LOBBY,
    READY
};

enum class LoadState {
    IDLE,
    LOADING,
    DONE
};

enum class LevelLoadState {
    IDLE,
    LOADING,
    DONE
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

struct LevelLoadData {
    std::vector<Brick> bricks;
    float brickWidth;
    float startX;
    int levelIndex;
};
class Game {
private:
    json config;

    std::vector<Ball> balls;
    Paddle paddle;
    std::vector<Brick> bricks;
    
    Grid brickGrid;                // ★ 网格

    int score;
    int hearts;
    float paddleMoveSpeed;

    Rectangle redLine;
    Rectangle singleBtn, raceBtn, versusBtn, startBtn, rankBtn, eraseRankBtn;
    Rectangle backToModeBtn, hostBtn, guestBtn, startGameBtn, backToModeBtn2;
    Rectangle continueBtn, restartBtn, gameOverRestartBtn, replayBtn, goAheadBtn;
    Rectangle victoryRestartBtn, victoryReplayBtn, backBtn;

    Texture2D backgroundTex, paddleTex;
    bool bgLoaded, paddleLoaded;

    GameState currentState;
    PauseCause pauseCause;

    bool isRaceMode, isHost, isGuest;
    MultiplayerSubState multiSubState;

    std::vector<TrailBuffer> ballTrails;
    ParticleSystem particleSystem;
    std::vector<SkillBall> skillBalls;
    LevelManager levelManager;

    std::vector<std::unique_ptr<Effect>> activeEffects;

    float originalPaddleWidth, originalBallRadius;
    float skillDropChance, skillBallSpeedY, skillBallRadius;
    int particlesPerBrick;
    float particleGravity;
    int maxTrailLength;

    int currentLevel = 0, totalLevels = 0;

    bool lastBrickAnimating = false;
    int lastBrickIndex = -1;
    Rectangle lastBrickStartRect, lastBrickTargetRect;
    float lastBrickAnimTimer = 0.0f;
    const float lastBrickAnimDuration = 1.0f;

    SoundManager soundManager;

    float gameTimer;
    int totalDeaths;
    bool timerRunning;

    std::vector<RankRecord> rankList;
    static const int MAX_RANK_COUNT = 5;
    const std::string rankFileName = "rank.dat";

    bool showEraseHint;
    float eraseHintTimer;

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

    bool exitToRaceLobby;

    // 异步加载
    std::atomic<LoadState> loadState{LoadState::IDLE};
    std::future<Image> loadFuture;
    std::string pendingTexturePath;
    Texture2D loadedTexture{0};
    bool useLoadedTexture = false;

    LevelLoadState levelLoadState = LevelLoadState::IDLE;
    std::future<LevelLoadData> levelLoadFuture;
    int pendingLevelIndex = -1;
    LevelLoadData GenerateLevelData(int index) const;
    void ApplyLevelLoadData(const LevelLoadData& data);

    // 性能测量
    double m_physicsTime = 0.0;
    double m_collisionTime = 0.0;
    double m_skillParticleTime = 0.0;
    double m_effectsTime = 0.0;
    double m_otherTime = 0.0;
    double m_totalTime = 0.0;

    int activeBrickCount = 0;

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
    void AddBallTrail() { ballTrails.emplace_back(maxTrailLength); } 
    bool HasEffectOfType(const std::string& typeName) const;
    const std::vector<std::unique_ptr<Effect>>& GetActiveEffects() const { return activeEffects; }
    void StartSinglePlayer();
    bool ShouldExitToRaceLobby() const { return exitToRaceLobby; }
    bool exitToVersusLobby = false;
    bool ShouldExitToVersusLobby() const { return exitToVersusLobby; }
};

#endif