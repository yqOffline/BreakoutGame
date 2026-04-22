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
    MODE_SELECT,        // 新增：模式选择
    SINGLE_MENU,        // 原 MENU，重命名
    MULTIPLAYER_MENU,   // 双人模式菜单（竞速或对抗）
    MULTIPLAYER_LOBBY,  // 双人等待界面
    MULTIPLAYER_READY,  // 双人准备开始界面
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
    LOBBY,      // 选择 Host/Guest
    READY       // 准备开始
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
    // 模式选择菜单按钮
    Rectangle singleBtn;
    Rectangle raceBtn;
    Rectangle versusBtn;
    // 单人菜单按钮
    Rectangle startBtn;
    Rectangle rankBtn;
    Rectangle eraseRankBtn;
    Rectangle backToModeBtn;        // 从单人菜单返回模式选择
    // 双人菜单按钮（竞速/对抗共用）
    Rectangle hostBtn;
    Rectangle guestBtn;
    Rectangle startGameBtn;         // Host 开始游戏按钮
    Rectangle backToModeBtn2;       // 从双人菜单返回模式选择
    // 其他原有按钮
    Rectangle continueBtn;
    Rectangle restartBtn;
    Rectangle gameOverRestartBtn;
    Rectangle replayBtn;
    Rectangle goAheadBtn;
    Rectangle victoryRestartBtn;
    Rectangle victoryReplayBtn;
    Rectangle backBtn;              // 排行榜返回按钮

    Texture2D backgroundTex;
    Texture2D paddleTex;
    bool bgLoaded;
    bool paddleLoaded;

    GameState currentState;
    PauseCause pauseCause;

    // 双人模式相关变量
    bool isRaceMode;                // true=竞速，false=对抗
    bool isHost;                    // 当前是否为 Host
    bool isGuest;                   // 当前是否为 Guest
    MultiplayerSubState multiSubState; // 双人菜单子状态

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