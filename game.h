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

// 异步纹理加载状态（板块一）
enum class LoadState {
    IDLE,
    LOADING,
    DONE
};

// 异步关卡加载状态（板块二）
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

// 异步加载关卡的数据结构（板块二）
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

    int score;
    int hearts;
    float paddleMoveSpeed;

    Rectangle redLine;
    Rectangle singleBtn;
    Rectangle raceBtn;
    Rectangle versusBtn;
    Rectangle startBtn;
    Rectangle rankBtn;
    Rectangle eraseRankBtn;
    Rectangle backToModeBtn;
    Rectangle hostBtn;
    Rectangle guestBtn;
    Rectangle startGameBtn;
    Rectangle backToModeBtn2;
    Rectangle continueBtn;
    Rectangle restartBtn;
    Rectangle gameOverRestartBtn;
    Rectangle replayBtn;
    Rectangle goAheadBtn;
    Rectangle victoryRestartBtn;
    Rectangle victoryReplayBtn;
    Rectangle backBtn;

    Texture2D backgroundTex;
    Texture2D paddleTex;
    bool bgLoaded;
    bool paddleLoaded;

    GameState currentState;
    PauseCause pauseCause;

    bool isRaceMode;
    bool isHost;
    bool isGuest;
    MultiplayerSubState multiSubState;

    std::vector<std::deque<Vector2>> ballTrails;
    ParticleSystem particleSystem;
    std::vector<SkillBall> skillBalls;
    LevelManager levelManager;

    std::vector<std::unique_ptr<Effect>> activeEffects;

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

    // ---------- 板块一：异步纹理加载 ----------
    std::atomic<LoadState> loadState{LoadState::IDLE};
    std::future<Image> loadFuture;
    std::string pendingTexturePath;
    Texture2D loadedTexture{0};
    bool useLoadedTexture = false;
    // ---------------------------------------

    // ---------- 板块二：异步关卡加载 ----------
    LevelLoadState levelLoadState = LevelLoadState::IDLE;
    std::future<LevelLoadData> levelLoadFuture;
    int pendingLevelIndex = -1;                     // 即将加载的关卡编号
    // 异步生成关卡数据的辅助函数（线程安全）
    LevelLoadData GenerateLevelData(int index) const;
    // 应用加载完成的关卡数据到游戏世界
    void ApplyLevelLoadData(const LevelLoadData& data);
    // -------------------------------------------

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

    void StartSinglePlayer();
    bool ShouldExitToRaceLobby() const { return exitToRaceLobby; }
    bool exitToVersusLobby = false;
    bool ShouldExitToVersusLobby() const { return exitToVersusLobby; }
};

#endif