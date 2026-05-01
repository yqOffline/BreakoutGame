#include "game.h"
#include "EffectFactory.h"
#include "SoundManager.h"
#include "TextureCache.h"
#include "ThreadPool.h"
#include <fstream>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <ctime>
#include <sstream>
#include <iomanip>

static ThreadPool levelPool(2);

// ======================== 构造函数 ========================
Game::Game(int screenWidth, int screenHeight)
    : score(0),
      hearts(3),
      currentState(GameState::MODE_SELECT),
      gameTimer(0.0f),
      totalDeaths(0),
      timerRunning(false),
      isRaceMode(false),
      isHost(false),
      isGuest(false),
      multiSubState(MultiplayerSubState::LOBBY),
      showEraseHint(false),eraseHintTimer(0.0f),
      exitToRaceLobby(false),
      loadState(LoadState::IDLE),
      levelLoadState(LevelLoadState::IDLE)
{
    // 1. 加载配置文件
    std::ifstream f("config.json");
    if (!f.is_open()) {
        std::cerr << "Error: Could not open config.json!" << std::endl;
    }
    f >> config;
    f.close();

    // 2. 加载效果工厂配置
    EffectFactory::LoadConfig(config);

    // 3. 用配置值设置 balls
    float bx = config["ball"]["init_x"];
    float by = config["ball"]["init_y"];
    float br = config["ball"]["radius"];
    float spx = config["ball"]["speed_x"];
    float spy = config["ball"]["speed_y"];
    balls.emplace_back(Vector2{bx, by}, Vector2{spx, spy}, br);
    ballTrails.emplace_back();

    // 4. 设置 paddle
    float pw = config["paddle"]["width"];
    float ph = config["paddle"]["height"];
    float px = (screenWidth - pw) / 2.0f;
    float py = config["paddle"]["y_pos"];
    paddle = Paddle(px, py, pw, ph);
    paddleMoveSpeed = config["paddle"]["speed"];

    // 5. 初始化 LevelManager
    levelManager.LoadConfig(config);

    // 6. 读取配置项
    skillDropChance = config["skill_ball"].value("drop_chance", 0.3f);
    skillBallSpeedY = config["skill_ball"].value("speed_y", 45.0f);
    skillBallRadius = config["skill_ball"].value("radius", 6.0f);
    
    particlesPerBrick = config["particles"].value("count_per_brick", 12);
    particleGravity = config["particles"].value("gravity", 300.0f);
    
    maxTrailLength = config["trail"].value("max_length", 10);
    
    totalLevels = levelManager.GetLevelCount();
    
    originalPaddleWidth = paddle.GetWidth();
    originalBallRadius = balls[0].GetRadius();

    // 7. 加载第一关（但不立即开始）
    LoadLevel(0);

    // 8. UI 按钮初始化
    redLine = {
        0.0f, paddle.GetRectangle().y + paddle.GetRectangle().height + 5.0f,
        (float)screenWidth, 3.0f
    };
    
    singleBtn = { (float)screenWidth / 2 - 100, 200, 200, 50 };
    raceBtn   = { (float)screenWidth / 2 - 100, 270, 200, 50 };
    versusBtn = { (float)screenWidth / 2 - 100, 340, 200, 50 };
    
    startBtn = { (float)screenWidth / 2 - 60, (float)screenHeight / 2 - 100, 120, 50 };
    rankBtn = { (float)screenWidth / 2 - 60, (float)screenHeight / 2 - 30, 120, 50 };
    eraseRankBtn = { (float)screenWidth / 2 - 60, (float)screenHeight / 2 + 40, 120, 50 };
    backToModeBtn = { (float)screenWidth / 2 - 60, (float)screenHeight - 80, 120, 50 };
    
    hostBtn = { (float)screenWidth / 2 - 150, 250, 120, 50 };
    guestBtn = { (float)screenWidth / 2 + 30, 250, 120, 50 };
    startGameBtn = { (float)screenWidth / 2 - 60, 350, 120, 50 };
    backToModeBtn2 = { (float)screenWidth / 2 - 60, (float)screenHeight - 80, 120, 50 };
    
    continueBtn = { (float)screenWidth / 2 - 100, (float)screenHeight / 2 - 25, 100, 50 };
    restartBtn = { (float)screenWidth / 2 + 20, (float)screenHeight / 2 - 25, 100, 50 };
    gameOverRestartBtn = { (float)screenWidth / 2 - 60, (float)screenHeight / 2 + 40, 120, 50 };
    replayBtn = { (float)screenWidth / 2 - 160, (float)screenHeight / 2 + 20, 120, 50 };
    goAheadBtn = { (float)screenWidth / 2 + 40, (float)screenHeight / 2 + 20, 120, 50 };
    victoryRestartBtn = { (float)screenWidth / 2 - 130, (float)screenHeight / 2 + 40, 120, 50 };
    victoryReplayBtn  = { (float)screenWidth / 2 + 10,  (float)screenHeight / 2 + 40, 120, 50 };
    backBtn = { (float)screenWidth / 2 - 60, (float)screenHeight - 80, 120, 50 };

    // 9. 加载纹理（使用 TextureCache 单例）
    backgroundTex = TextureCache::Instance().GetTexture("1.png");
    paddleTex = TextureCache::Instance().GetTexture("2.png");
    bgLoaded = (backgroundTex.id != 0);
    paddleLoaded = (paddleTex.id != 0);
    loadedTexture = {0};

    pauseCause = PauseCause::MANUAL_PAUSE;
    lastBrickAnimating = false;
    lastBrickIndex = -1;
    lastBrickAnimTimer = 0.0f;

    // 10. 加载排行榜
    LoadRanking();
}

// ======================== 析构函数 ========================
Game::~Game() {

    if (loadedTexture.id != 0) UnloadTexture(loadedTexture);
    SaveRanking();
}

// ======================== 清空排行榜 ========================
void Game::ClearRanking() {
    rankList.clear();
    SaveRanking();
}

// ======================== 完全重置游戏状态 ========================
void Game::ResetGameState() {
    currentLevel = 0;
    LoadLevel(0);
    
    balls.clear();
    ballTrails.clear();
    Vector2 initPos = { (float)config["ball"]["init_x"], (float)config["ball"]["init_y"] };
    Vector2 initSpeed = { 0.0f, 0.0f };
    float initRadius = config["ball"]["radius"];
    balls.emplace_back(initPos, initSpeed, initRadius);
    ballTrails.emplace_back();
    
    score = 0;
    hearts = config["game"]["initial_hearts"];
    currentState = GameState::MODE_SELECT;
    
    skillBalls.clear();
    particleSystem.Clear();
    
    for (auto& effect : activeEffects) {
        effect->Revert(this);
    }
    activeEffects.clear();
    paddle.SetWidth(originalPaddleWidth);
    for (auto& ball : balls) {
        ball.SetRadius(originalBallRadius);
    }
    
    lastBrickAnimating = false;
    lastBrickIndex = -1;
    lastBrickAnimTimer = 0.0f;
    
    gameTimer = 0.0f;
    totalDeaths = 0;
    timerRunning = false;
    
    isHost = false;
    isGuest = false;
    multiSubState = MultiplayerSubState::LOBBY;
    exitToRaceLobby = false;

    loadState = LoadState::IDLE;
    if (loadedTexture.id != 0) {
        UnloadTexture(loadedTexture);
        loadedTexture = {0};
    }
    useLoadedTexture = false;
    levelLoadState = LevelLoadState::IDLE;
}

void Game::ResetGame() {
    ResetGameState();
}

void Game::ResetBricks() {
    LoadLevel(currentLevel);
}

// --------------------------------------------------
// 板块二：异步关卡加载的实现
// --------------------------------------------------
LevelLoadData Game::GenerateLevelData(int index) const {
    LevelLoadData data;
    data.levelIndex = index;
    const LevelConfig& cfg = levelManager.GetLevelConfig(index);

    int rows = cfg.rows;
    int cols = cfg.cols;
    int totalBricks = rows * cols;

    // 生成血量池
    std::vector<int> healthPool = LevelManager::GenerateHealthPool(totalBricks, levelManager.GetHealthDistribution());

    // 创建独立的随机引擎（避免影响主线程的随机状态）
    std::mt19937 rng(std::random_device{}());
    std::shuffle(healthPool.begin(), healthPool.end(), rng);

    // 计算砖块尺寸与起始位置
    float totalSpacing = (cols - 1) * cfg.spacing;
    float availableWidth = GetScreenWidth() - 2 * cfg.margin;
    float brickWidth = (availableWidth - totalSpacing) / cols;
    float totalWidth = cols * brickWidth + totalSpacing;
    float startX = (GetScreenWidth() - totalWidth) / 2.0f;
    float startY = cfg.startY;

    data.brickWidth = brickWidth;
    data.startX = startX;

    // 创建砖块
    data.bricks.clear();
    int healthIndex = 0;
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            float x = startX + c * (brickWidth + cfg.spacing);
            float y = startY + r * (cfg.brickHeight + cfg.spacing);
            int hp = healthPool[healthIndex++];
            data.bricks.emplace_back(x, y, brickWidth, cfg.brickHeight, hp);
        }
    }
    return data;
}

void Game::ApplyLevelLoadData(const LevelLoadData& data) {
    bricks = std::move(data.bricks);
    // 重置游戏状态（球、挡板等），但保留分数等？
    // 按照关卡切换的惯例，重新初始化球和拖尾，清除粒子等
    balls.clear();
    ballTrails.clear();
    Vector2 initPos = { (float)config["ball"]["init_x"], (float)config["ball"]["init_y"] };
    Vector2 initSpeed = { 0.0f, 0.0f };
    float initRadius = config["ball"]["radius"];
    balls.emplace_back(initPos, initSpeed, initRadius);
    ballTrails.emplace_back();

    skillBalls.clear();
    particleSystem.Clear();

    for (auto& effect : activeEffects) {
        effect->Revert(this);
    }
    activeEffects.clear();
    paddle.SetWidth(originalPaddleWidth);
    for (auto& ball : balls) {
        ball.SetRadius(originalBallRadius);
    }

    lastBrickAnimating = false;
    lastBrickIndex = -1;
    lastBrickAnimTimer = 0.0f;

    currentLevel = data.levelIndex;
    hearts = config["game"]["initial_hearts"];  // 重置生命

    // 游戏继续，不改变 currentState
}
// --------------------------------------------------

void Game::CheckBallHitRedLine() {
    std::vector<size_t> ballsToRemove;

    for (size_t i = 0; i < balls.size(); ++i) {
        Ball& ball = balls[i];
        if (CheckCollisionCircleRec(ball.GetPosition(), ball.GetRadius(), redLine)) {
            if (HasEffectOfType("Invincible")) {
                Vector2 sp = ball.GetSpeed();
                sp.y *= -1;
                ball.SetSpeed(sp);
                ball.SetPosition({ ball.GetPosition().x, redLine.y - ball.GetRadius() });
                continue;
            }
            ballsToRemove.push_back(i);
            particleSystem.EmitExplosion(ball.GetPosition(), RED, 15);
        }
    }

    if (ballsToRemove.empty()) return;

    for (auto it = ballsToRemove.rbegin(); it != ballsToRemove.rend(); ++it) {
        balls.erase(balls.begin() + *it);
        ballTrails.erase(ballTrails.begin() + *it);
    }

    if (balls.empty()) {
        hearts--;
        totalDeaths++;
        if (hearts <= 0) {
            currentState = GameState::GAME_OVER;
            timerRunning = false;
            soundManager.PlayGameOver();
        } else {
            currentState = GameState::PAUSED;
            pauseCause = PauseCause::LIFE_LOSS_PAUSE;

            soundManager.PlayLevelOver();

            float paddleCenterX = paddle.GetRectangle().x + paddle.GetRectangle().width / 2;
            float paddleTopY = paddle.GetRectangle().y - originalBallRadius - 2;
            balls.emplace_back(Vector2{paddleCenterX, paddleTopY},
                              Vector2{config["ball"]["speed_x"], config["ball"]["speed_y"]},
                              originalBallRadius);
            ballTrails.emplace_back();
        }
    }
}

void Game::ApplyEffect(std::unique_ptr<Effect> effect) {
    if (!effect) return;
    
    EffectType newType = effect->GetType();
    for (auto it = activeEffects.begin(); it != activeEffects.end(); ++it) {
        if ((*it)->GetType() == newType) {
            (*it)->Revert(this);
            activeEffects.erase(it);
            break;
        }
    }
    
    effect->Apply(this);
    activeEffects.push_back(std::move(effect));
}

void Game::UpdateEffects(float dt) {
    for (auto it = activeEffects.begin(); it != activeEffects.end(); ) {
        bool stillActive = (*it)->Update(dt);
        if (!stillActive) {
            soundManager.PlayPowerupEnd();
            (*it)->Revert(this);
            it = activeEffects.erase(it);
        } else {
            ++it;
        }
    }
}

bool Game::HasEffectOfType(const std::string& typeName) const {
    for (const auto& effect : activeEffects) {
        if (effect->GetName() == typeName) {
            return true;
        }
    }
    return false;
}

void Game::HandleInput(Vector2 mousePos) {
    // 任何加载期间，忽略所有输入
    if (loadState == LoadState::LOADING || levelLoadState == LevelLoadState::LOADING) return;

    // ---------- 板块一：异步纹理加载 ----------
    if (IsKeyPressed(KEY_L) && loadState == LoadState::IDLE) {
        pendingTexturePath = "big_image.png";
        loadState = LoadState::LOADING;
        loadFuture = std::async(std::launch::async, [](const std::string& path) {
            return LoadImage(path.c_str());
        }, pendingTexturePath);
        return;
    }
    // ---------------------------------------

    // ---------- 板块二：异步关卡加载 ----------
    if (IsKeyPressed(KEY_N) && levelLoadState == LevelLoadState::IDLE && currentState == GameState::PLAYING) {
        if (currentLevel + 1 < totalLevels) {
            pendingLevelIndex = currentLevel + 1;
            levelLoadState = LevelLoadState::LOADING;
            
            // 使用线程池
            static ThreadPool levelPool(2);
            levelLoadFuture = levelPool.Enqueue([this]() {
                return GenerateLevelData(pendingLevelIndex);
            });
        }
        return;
    }
    // -----------------------------------------

    switch (currentState) {
        case GameState::MODE_SELECT:
            if ((IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, singleBtn)) || IsKeyPressed(KEY_ONE)) {
                currentState = GameState::SINGLE_MENU;
            }
            if ((IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, raceBtn)) || IsKeyPressed(KEY_TWO)) {
                exitToRaceLobby = true;
            }
            if ((IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, versusBtn)) || IsKeyPressed(KEY_THREE)) {
                exitToVersusLobby = true;
            }
            break;

        case GameState::SINGLE_MENU:
            if ((IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, startBtn)) || IsKeyPressed(KEY_SPACE)) {
                currentState = GameState::PLAYING;
                for (auto& b : balls) b.SetSpeed({ config["ball"]["speed_x"], config["ball"]["speed_y"] });
                timerRunning = true;
            }
            if ((IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, rankBtn)) || IsKeyPressed(KEY_R)) {
                currentState = GameState::RANKING;
            }
            if ((IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, eraseRankBtn)) || IsKeyPressed(KEY_E)) {
                ClearRanking();
                showEraseHint = true;
                eraseHintTimer = 0.5f;
            }
            if ((IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, backToModeBtn)) || IsKeyPressed(KEY_B)) {
                currentState = GameState::MODE_SELECT;
            }
            break;

        case GameState::MULTIPLAYER_MENU:
        case GameState::MULTIPLAYER_LOBBY:
            // ... 原有代码保持不变，此处省略，实际文件中保留原有多人菜单逻辑 ...
            break;

        case GameState::MULTIPLAYER_READY:
            // ... 省略 ...
            break;

        case GameState::PLAYING:
            if (IsKeyPressed(KEY_SPACE)) {
                currentState = GameState::PAUSED;
                pauseCause = PauseCause::MANUAL_PAUSE;
                timerRunning = false;
            }
            break;

        case GameState::PAUSED:
            if (IsKeyPressed(KEY_SPACE) || (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, continueBtn))) {
                currentState = GameState::PLAYING;
                timerRunning = true;
            }
            if ((IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, continueBtn) || IsKeyPressed(KEY_SPACE)) && pauseCause == PauseCause::LIFE_LOSS_PAUSE) {
                for (auto& b : balls) b.SetSpeed({ config["ball"]["speed_x"], config["ball"]["speed_y"] });
                currentState = GameState::PLAYING;
                timerRunning = true;
            }
            if ((IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, restartBtn)) || IsKeyPressed(KEY_R)) {
                ResetGame();
            }
            break;

        case GameState::GAME_OVER:
            if ((IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, gameOverRestartBtn)) || IsKeyPressed(KEY_SPACE)) {
                ResetGame();
            }
            break;

        case GameState::LEVEL_CLEAR:
            // ... 原有处理 ...
            break;

        case GameState::VICTORY:
            // ... 原有处理 ...
            break;

        case GameState::RANKING:
            if ((IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, backBtn)) || IsKeyPressed(KEY_B)) {
                currentState = GameState::SINGLE_MENU;
            }
            break;
    }
}

void Game::HandleBallCollisions() {
    for (size_t i = 0; i < balls.size(); ++i) {
        for (size_t j = i + 1; j < balls.size(); ++j) {
            Ball& a = balls[i];
            Ball& b = balls[j];
            Vector2 posA = a.GetPosition();
            Vector2 posB = b.GetPosition();
            float radiusSum = a.GetRadius() + b.GetRadius();
            float dx = posA.x - posB.x;
            float dy = posA.y - posB.y;
            float dist = sqrtf(dx*dx + dy*dy);
            if (dist < radiusSum && dist > 0.001f) {
                soundManager.PlayHitSound();
                Vector2 spA = a.GetSpeed();
                Vector2 spB = b.GetSpeed();
                Vector2 normal = { dx / dist, dy / dist };
                Vector2 tangent = { -normal.y, normal.x };
                float v1n = normal.x * spA.x + normal.y * spA.y;
                float v1t = tangent.x * spA.x + tangent.y * spA.y;
                float v2n = normal.x * spB.x + normal.y * spB.y;
                float v2t = tangent.x * spB.x + tangent.y * spB.y;
                float v1nAfter = v2n;
                float v2nAfter = v1n;
                Vector2 newSpA = { normal.x * v1nAfter + tangent.x * v1t, normal.y * v1nAfter + tangent.y * v1t };
                Vector2 newSpB = { normal.x * v2nAfter + tangent.x * v2t, normal.y * v2nAfter + tangent.y * v2t };
                a.SetSpeed(newSpA);
                b.SetSpeed(newSpB);
                float overlap = radiusSum - dist;
                Vector2 separation = { normal.x * overlap * 0.5f, normal.y * overlap * 0.5f };
                a.SetPosition({ posA.x + separation.x, posA.y + separation.y });
                b.SetPosition({ posB.x - separation.x, posB.y - separation.y });
            }
        }
    }
}

void Game::Update(float dt) {
    // ---------- 板块一：异步纹理加载处理 ----------
    if (loadState == LoadState::LOADING) {
        if (loadFuture.valid() &&
            loadFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            Image img = loadFuture.get();
            if (img.data != nullptr) {
                if (loadedTexture.id != 0) UnloadTexture(loadedTexture);
                loadedTexture = LoadTextureFromImage(img);
                UnloadImage(img);
                useLoadedTexture = true;
                if (!bricks.empty()) {
                    bricks[0].SetActive(false);
                }
            }
            loadState = LoadState::DONE;
        }
        return; // 跳过游戏逻辑
    }
    // -------------------------------------------------

    // ---------- 板块二：异步关卡加载处理 ----------
    if (levelLoadState == LevelLoadState::LOADING) {
        if (levelLoadFuture.valid() &&
            levelLoadFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            LevelLoadData data = levelLoadFuture.get();
            ApplyLevelLoadData(data);
            // 加载完成后，将球速度设为配置值，让游戏继续进行
            for (auto& b : balls) b.SetSpeed({ config["ball"]["speed_x"], config["ball"]["speed_y"] });
            levelLoadState = LevelLoadState::DONE;
            // 继续游戏，不改变 currentState（游戏仍在 PLAYING）
        }
        return; // 跳过游戏逻辑
    }
    // --------------------------------------------

    if (currentState == GameState::PLAYING && timerRunning) {
        gameTimer += dt;
    }

    if (showEraseHint) {
        eraseHintTimer -= dt;
        if (eraseHintTimer <= 0.0f) {
            showEraseHint = false;
            eraseHintTimer = 0.0f;
        }
    }

    if (currentState != GameState::PLAYING) return;
    
    // 后门按键
    if (IsKeyPressed(KEY_C)) {
        if (currentLevel == totalLevels - 1) {
            currentState = GameState::VICTORY;
            timerRunning = false;
            AddVictoryRecord();
            for (auto& b : balls) b.SetSpeed({0, 0});
        } else {
            currentState = GameState::LEVEL_CLEAR;
            for (auto& b : balls) b.SetSpeed({0, 0});
        }
        return;
    }
    if (IsKeyPressed(KEY_V)) {
        currentState = GameState::VICTORY;
        timerRunning = false;
        AddVictoryRecord();
        for (auto& b : balls) b.SetSpeed({0, 0});
        return;
    }
    
    // 1. 拖尾记录
    for (size_t i = 0; i < balls.size(); ++i) {
        ballTrails[i].push_back(balls[i].GetPosition());
        if ((int)ballTrails[i].size() > maxTrailLength) ballTrails[i].pop_front();
    }

    // 2. 主球移动与碰撞
    for (auto& ball : balls) {
        ball.Move();
        if (ball.BounceEdge(GetScreenWidth(), GetScreenHeight())) {
            soundManager.PlayHitSound();
        }
        if (ball.CheckCollisionPaddle(paddle)) {
            soundManager.PlayHitSound();
        }
    }

    // 3. 砖块碰撞处理
    for (auto& ball : balls) {
        for (auto& brick : bricks) {
            if (brick.IsActive() && CheckCollisionCircleRec(ball.GetPosition(), ball.GetRadius(), brick.GetRectangle())) {
                soundManager.PlayHitSound();
                int damage = 1;
                if (HasEffectOfType("Explosion")) {
                    damage = 2;
                }
                bool destroyed = false;
                for (int d = 0; d < damage; ++d) {
                    if (brick.TakeDamage()) {
                        destroyed = true;
                        break;
                    }
                }
                if (destroyed) {
                    score++;
                    particleSystem.EmitBrickBreak(brick.GetRectangle(), brick.GetColor(), particlesPerBrick);
                    if (brick.ShouldDropSkill(skillDropChance)) {
                        Vector2 spawnPos = { brick.GetRectangle().x + brick.GetRectangle().width / 2,
                                             brick.GetRectangle().y + brick.GetRectangle().height / 2 };
                        SkillType type = static_cast<SkillType>(GetRandomValue(0, 5));
                        Color glowColor = WHITE;
                        switch (type) {
                            case SkillType::PADDLE_EXTEND: glowColor = BLUE; break;
                            case SkillType::BALL_ENLARGE:  glowColor = GREEN; break;
                            case SkillType::BALL_SHRINK:   glowColor = RED; break;
                            case SkillType::EXPLOSION:     glowColor = ORANGE; break;
                            case SkillType::INVINCIBLE:    glowColor = GOLD; break;
                            case SkillType::SPLIT:         glowColor = SKYBLUE; break;
                            default: break;
                        }
                        skillBalls.emplace_back(spawnPos, type, skillBallRadius, Vector2{0, skillBallSpeedY}, glowColor);
                    }
                }
                Vector2 sp = ball.GetSpeed();
                sp.y *= -1;
                ball.SetSpeed(sp);
                if (sp.y > 0)
                    ball.SetPosition({ ball.GetPosition().x, brick.GetRectangle().y + brick.GetRectangle().height + ball.GetRadius() });
                else
                    ball.SetPosition({ ball.GetPosition().x, brick.GetRectangle().y - ball.GetRadius() });
                break;
            }
        }
    }

    // 4. 球间碰撞
    HandleBallCollisions();

    // 5. 技能球更新
    for (auto& sb : skillBalls) {
        sb.Update(dt);
        if (sb.active && CheckCollisionCircleRec(sb.GetPosition(), sb.GetRadius(), paddle.GetRectangle())) {
            soundManager.PlayPowerupGet();
            auto effect = EffectFactory::CreateEffect(sb.skillType);
            if (effect) {
                ApplyEffect(std::move(effect));
            }
            sb.active = false;
        }
    }
    skillBalls.erase(std::remove_if(skillBalls.begin(), skillBalls.end(),
        [](const SkillBall& sb) { return !sb.active; }), skillBalls.end());

    // 6. 粒子更新
    particleSystem.Update(dt, particleGravity);

    // 7. 效果更新
    UpdateEffects(dt);

    // 8. 挡板移动
    if (IsKeyDown(KEY_LEFT)) paddle.MoveLeft(paddleMoveSpeed);
    if (IsKeyDown(KEY_RIGHT)) paddle.MoveRight(paddleMoveSpeed);

    // 9. 最后一块砖特殊动画（略）
    int activeCount = 0;
    int lastActiveIdx = -1;
    for (int i = 0; i < (int)bricks.size(); ++i) {
        if (bricks[i].IsActive()) {
            activeCount++;
            lastActiveIdx = i;
        }
    }
    if (activeCount == 1 && !lastBrickAnimating && lastActiveIdx != -1) {
        lastBrickAnimating = true;
        lastBrickIndex = lastActiveIdx;
        Brick& b = bricks[lastBrickIndex];
        lastBrickStartRect = b.GetRectangle();
        float targetW = lastBrickStartRect.width * 5.0f;
        float targetH = lastBrickStartRect.height;
        float centerX = GetScreenWidth() / 2.0f;
        float centerY = GetScreenHeight() / 2.0f;
        lastBrickTargetRect = { centerX - targetW/2, centerY - targetH/2, targetW, targetH };
        lastBrickAnimTimer = 0.0f;
    }
    if (lastBrickAnimating && lastBrickIndex != -1) {
        lastBrickAnimTimer += dt;
        float t = lastBrickAnimTimer / lastBrickAnimDuration;
        if (t >= 1.0f) {
            t = 1.0f;
            lastBrickAnimating = false;
        }
        Brick& b = bricks[lastBrickIndex];
        float newW = lastBrickStartRect.width + (lastBrickTargetRect.width - lastBrickStartRect.width) * t;
        float newH = lastBrickStartRect.height;
        float newX = lastBrickStartRect.x + (lastBrickTargetRect.x - lastBrickStartRect.x) * t;
        float newY = lastBrickStartRect.y + (lastBrickTargetRect.y - lastBrickStartRect.y) * t;
        b.SetRect({ newX, newY, newW, newH });
    }

    // 10. 红线碰撞检测
    CheckBallHitRedLine();

    // 11. 关卡过渡检查
    CheckLevelTransition();
}

void Game::Draw() {
    // 绘制背景
    if (bgLoaded) {
        DrawTexturePro(backgroundTex,
            { 0, 0, (float)backgroundTex.width, (float)backgroundTex.height },
            { 0, 0, (float)GetScreenWidth(), (float)GetScreenHeight() },
            { 0, 0 }, 0, WHITE);
    } else {
        ClearBackground(RAYWHITE);
    }

    DrawRectangle(0, 0, 5, GetScreenHeight(), GRAY);
    DrawRectangle(GetScreenWidth() - 5, 0, 5, GetScreenHeight(), GRAY);
    DrawRectangle(0, 0, GetScreenWidth(), 5, GRAY);
    DrawRectangle(0, GetScreenHeight() - 5, GetScreenWidth(), 5, GRAY);

    // ---------- 加载中 ----------
    if (loadState == LoadState::LOADING) {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.5f));
        DrawText("Loading...", GetScreenWidth()/2 - 100, GetScreenHeight()/2 - 20, 40, WHITE);
        return;
    }
    if (levelLoadState == LevelLoadState::LOADING) {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.5f));
        DrawText("Loading Level...", GetScreenWidth()/2 - 150, GetScreenHeight()/2 - 20, 40, WHITE);
        return;
    }

    // 绘制游戏元素（仅在游戏进行中或相关状态）
    if (currentState == GameState::PLAYING || currentState == GameState::PAUSED || 
        currentState == GameState::LEVEL_CLEAR || currentState == GameState::GAME_OVER ||
        currentState == GameState::VICTORY) {
        
        for (size_t i = 0; i < balls.size(); ++i) {
            for (size_t j = 0; j < ballTrails[i].size(); ++j) {
                float alpha = 0.3f * (float)j / ballTrails[i].size();
                float size = balls[i].GetRadius() * (0.5f + 0.5f * j / ballTrails[i].size());
                DrawCircleV(ballTrails[i][j], size, Fade(RED, alpha));
            }
        }

        for (auto& b : balls) b.Draw();

        if (paddleLoaded) {
            DrawTexturePro(paddleTex,
                { 0, 0, (float)paddleTex.width, (float)paddleTex.height },
                paddle.GetRectangle(), { 0, 0 }, 0, WHITE);
        } else {
            paddle.Draw();
        }

        for (auto& b : bricks) b.Draw();
        for (auto& sb : skillBalls) sb.Draw();
        particleSystem.Draw();

        if (currentState == GameState::PLAYING) DrawRectangleRec(redLine, RED);
    }

    // UI 文字
    DrawText(TextFormat("SCORE: %d", score), 10, 10, 20, BLUE);
    DrawText(TextFormat("LIVES: %d", hearts), 10, 40, 20, RED);
    DrawText(TextFormat("LEVEL: %d", currentLevel + 1), 10, 70, 20, DARKGREEN);
    DrawText(TextFormat("TIME: %.1f", gameTimer), 10, 100, 20, DARKPURPLE);
    DrawText(TextFormat("DEATHS: %d", totalDeaths), 10, 130, 20, MAROON);

    int effectY = 160;
    for (const auto& effect : activeEffects) {
        DrawText(TextFormat("%s: %.1fs", effect->GetName().c_str(), effect->GetRemainingTime()), 10, effectY, 20, GREEN);
        effectY += 20;
    }

    // 根据不同状态绘制 UI（完整按钮）
    switch (currentState) {
        case GameState::MODE_SELECT:
            DrawText("SELECT MODE", GetScreenWidth()/2 - 100, 120, 40, DARKBLUE);
            DrawRectangleRec(singleBtn, CheckCollisionPointRec(GetMousePosition(), singleBtn) ? DARKGREEN : GREEN);
            DrawText("SINGLE (1)", singleBtn.x + 30, singleBtn.y + 15, 20, BLACK);
            DrawRectangleRec(raceBtn, CheckCollisionPointRec(GetMousePosition(), raceBtn) ? DARKBLUE : BLUE);
            DrawText("RACE (2)", raceBtn.x + 40, raceBtn.y + 15, 20, WHITE);
            DrawRectangleRec(versusBtn, CheckCollisionPointRec(GetMousePosition(), versusBtn) ? DARKPURPLE : PURPLE);
            DrawText("VERSUS (3)", versusBtn.x + 30, versusBtn.y + 15, 20, WHITE);
            break;

        case GameState::SINGLE_MENU:
            DrawRectangleRec(startBtn, CheckCollisionPointRec(GetMousePosition(), startBtn) ? DARKGREEN : GREEN);
            DrawText("START GAME", startBtn.x + 12, startBtn.y + 15, 20, BLACK);
            DrawRectangleRec(rankBtn, CheckCollisionPointRec(GetMousePosition(), rankBtn) ? DARKBLUE : BLUE);
            DrawText("RANK (R)", rankBtn.x + 25, rankBtn.y + 15, 20, WHITE);
            DrawRectangleRec(eraseRankBtn, CheckCollisionPointRec(GetMousePosition(), eraseRankBtn) ? MAROON : RED);
            DrawText("ERASE (E)", eraseRankBtn.x + 20, eraseRankBtn.y + 15, 20, WHITE);
            DrawRectangleRec(backToModeBtn, CheckCollisionPointRec(GetMousePosition(), backToModeBtn) ? DARKGRAY : GRAY);
            DrawText("BACK", backToModeBtn.x + 35, backToModeBtn.y + 15, 20, WHITE);
            if (showEraseHint) {
                DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.5f));
                const char* hintText = "RANKING HAS BEEN ERASED!";
                int fontSize = 30;
                int textWidth = MeasureText(hintText, fontSize);
                DrawText(hintText, GetScreenWidth()/2 - textWidth/2, GetScreenHeight()/2 - fontSize/2, fontSize, RED);
            }
            break;

        case GameState::MULTIPLAYER_MENU:
        case GameState::MULTIPLAYER_LOBBY:
            DrawText(isRaceMode ? "RACE MODE" : "VERSUS MODE", GetScreenWidth()/2 - 120, 120, 40, DARKBLUE);
            DrawRectangleRec(hostBtn, CheckCollisionPointRec(GetMousePosition(), hostBtn) ? (isHost ? DARKGREEN : DARKBLUE) : (isHost ? GREEN : BLUE));
            DrawText("HOST (H)", hostBtn.x + 20, hostBtn.y + 15, 20, WHITE);
            DrawRectangleRec(guestBtn, CheckCollisionPointRec(GetMousePosition(), guestBtn) ? (isGuest ? DARKGREEN : DARKBLUE) : (isGuest ? GREEN : BLUE));
            DrawText("GUEST (G)", guestBtn.x + 15, guestBtn.y + 15, 20, WHITE);
            if (isHost) {
                DrawRectangleRec(startGameBtn, CheckCollisionPointRec(GetMousePosition(), startGameBtn) ? DARKGREEN : GREEN);
                DrawText("READY (S)", startGameBtn.x + 20, startGameBtn.y + 15, 20, BLACK);
            }
            DrawRectangleRec(backToModeBtn2, CheckCollisionPointRec(GetMousePosition(), backToModeBtn2) ? DARKGRAY : GRAY);
            DrawText("BACK", backToModeBtn2.x + 35, backToModeBtn2.y + 15, 20, WHITE);
            break;

        case GameState::MULTIPLAYER_READY:
            DrawText(isRaceMode ? "RACE - READY" : "VERSUS - READY", GetScreenWidth()/2 - 150, 150, 40, DARKBLUE);
            if (isHost) {
                DrawText("Press START to begin the game", GetScreenWidth()/2 - 200, 250, 30, DARKGREEN);
                DrawRectangleRec(startGameBtn, CheckCollisionPointRec(GetMousePosition(), startGameBtn) ? DARKGREEN : GREEN);
                DrawText("START (S)", startGameBtn.x + 15, startGameBtn.y + 15, 20, BLACK);
            } else if (isGuest) {
                DrawText("Waiting for Host to start...", GetScreenWidth()/2 - 180, 250, 30, GRAY);
            }
            DrawRectangleRec(backToModeBtn2, CheckCollisionPointRec(GetMousePosition(), backToModeBtn2) ? DARKGRAY : GRAY);
            DrawText("BACK", backToModeBtn2.x + 35, backToModeBtn2.y + 15, 20, WHITE);
            break;

        case GameState::PAUSED:
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.7f));
            DrawRectangleRec(continueBtn, CheckCollisionPointRec(GetMousePosition(), continueBtn) ? DARKBLUE : BLUE);
            DrawText("CONTINUE", continueBtn.x + 15, continueBtn.y + 15, 20, BLACK);
            DrawRectangleRec(restartBtn, CheckCollisionPointRec(GetMousePosition(), restartBtn) ? MAROON : RED);
            DrawText("RESTART", restartBtn.x + 20, restartBtn.y + 15, 20, BLACK);
            DrawText("PAUSED", GetScreenWidth() / 2 - 60, GetScreenHeight() / 2 - 80, 40, WHITE);
            break;

        case GameState::GAME_OVER:
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.8f));
            DrawText("GAME OVER", GetScreenWidth() / 2 - 100, GetScreenHeight() / 2 - 40, 50, RED);
            DrawRectangleRec(gameOverRestartBtn, CheckCollisionPointRec(GetMousePosition(), gameOverRestartBtn) ? DARKGREEN : GREEN);
            DrawText("PLAY AGAIN", gameOverRestartBtn.x + 10, gameOverRestartBtn.y + 15, 20, BLACK);
            break;

        case GameState::LEVEL_CLEAR:
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.7f));
            DrawText(TextFormat("LEVEL %d CLEAR!", currentLevel + 1), GetScreenWidth() / 2 - 150, GetScreenHeight() / 2 - 80, 40, WHITE);
            DrawRectangleRec(replayBtn, CheckCollisionPointRec(GetMousePosition(), replayBtn) ? DARKBLUE : BLUE);
            DrawText("REPLAY (P)", replayBtn.x + 15, replayBtn.y + 15, 20, WHITE);
            DrawRectangleRec(goAheadBtn, CheckCollisionPointRec(GetMousePosition(), goAheadBtn) ? DARKGREEN : GREEN);
            DrawText("GO AHEAD (G)", goAheadBtn.x + 8, goAheadBtn.y + 15, 20, BLACK);
            break;

        case GameState::VICTORY:
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.8f));
            DrawText("VICTORY!", GetScreenWidth() / 2 - 120, GetScreenHeight() / 2 - 60, 50, GOLD);
            DrawText(TextFormat("TIME: %.2f s", gameTimer), GetScreenWidth() / 2 - 80, GetScreenHeight() / 2, 30, WHITE);
            DrawText(TextFormat("DEATHS: %d", totalDeaths), GetScreenWidth() / 2 - 60, GetScreenHeight() / 2 + 30, 30, WHITE);
            DrawRectangleRec(victoryRestartBtn, CheckCollisionPointRec(GetMousePosition(), victoryRestartBtn) ? DARKGREEN : GREEN);
            DrawText("RESTART", victoryRestartBtn.x + 20, victoryRestartBtn.y + 15, 20, BLACK);
            DrawRectangleRec(victoryReplayBtn, CheckCollisionPointRec(GetMousePosition(), victoryReplayBtn) ? DARKBLUE : BLUE);
            DrawText("REPLAY", victoryReplayBtn.x + 25, victoryReplayBtn.y + 15, 20, WHITE);
            break;

        case GameState::RANKING:
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.8f));
            DrawText("RANKING (TOP 5)", GetScreenWidth() / 2 - 120, 50, 40, GOLD);
            for (int i = 0; i < (int)rankList.size() && i < MAX_RANK_COUNT; ++i) {
                const auto& rec = rankList[i];
                DrawText(TextFormat("%d.  Time: %.2f s  Deaths: %d", i+1, rec.time, rec.deaths),
                         GetScreenWidth() / 2 - 150, 120 + i * 40, 25, WHITE);
            }
            if (rankList.empty()) {
                DrawText("No records yet", GetScreenWidth() / 2 - 100, 150, 30, LIGHTGRAY);
            }
            DrawRectangleRec(backBtn, CheckCollisionPointRec(GetMousePosition(), backBtn) ? DARKGRAY : GRAY);
            DrawText("BACK (B)", backBtn.x + 20, backBtn.y + 15, 20, WHITE);
            break;

        default: break;
        }
    int fps = GetFPS();
    float frameTime = GetFrameTime() * 1000.0f; // 毫秒
    DrawText(TextFormat("FPS: %d", fps), GetScreenWidth() - 150, 10, 20, YELLOW);
    DrawText(TextFormat("Frame: %.2f ms", frameTime), GetScreenWidth() - 200, 30, 20, YELLOW);
}

void Game::LoadLevel(int index) {
    if (index >= levelManager.GetLevelCount()) {
        currentState = GameState::VICTORY;
        timerRunning = false;
        AddVictoryRecord();
        return;
    }
    currentLevel = index;

    hearts = config["game"].value("initial_hearts", 3);

    float brickWidth, startX;
    levelManager.LoadLevel(index, bricks, brickWidth, startX,
                           GetScreenWidth(), GetScreenHeight());

    balls.clear();
    ballTrails.clear();
    Vector2 initPos = { (float)config["ball"]["init_x"], (float)config["ball"]["init_y"] };
    Vector2 initSpeed = { 0.0f, 0.0f };
    float initRadius = config["ball"]["radius"];
    balls.emplace_back(initPos, initSpeed, initRadius);
    ballTrails.emplace_back();

    skillBalls.clear();
    particleSystem.Clear();

    lastBrickAnimating = false;
    lastBrickIndex = -1;
    lastBrickAnimTimer = 0.0f;

    for (auto& effect : activeEffects) {
        effect->Revert(this);
    }
    activeEffects.clear();
    paddle.SetWidth(originalPaddleWidth);
    for (auto& ball : balls) {
        ball.SetRadius(originalBallRadius);
    }
}

void Game::CheckLevelTransition() {
    bool allInactive = true;
    for (const auto& b : bricks) {
        if (b.IsActive()) {
            allInactive = false;
            break;
        }
    }
    if (allInactive && currentState == GameState::PLAYING) {
        if (currentLevel + 1 < totalLevels) {
            currentState = GameState::LEVEL_CLEAR;
            soundManager.PlayLevelComplete();
            for (auto& b : balls) b.SetSpeed({0, 0});
        } else {
            currentState = GameState::VICTORY;
            timerRunning = false;
            soundManager.PlayGameVictory();
            AddVictoryRecord();
            for (auto& b : balls) b.SetSpeed({0, 0});
        }
    }
}

bool Game::IsGameRunning() const {
    return !WindowShouldClose();
}

void Game::LoadRanking() {
    std::ifstream file(rankFileName, std::ios::binary);
    if (!file.is_open()) return;
    
    rankList.clear();
    size_t count;
    file.read(reinterpret_cast<char*>(&count), sizeof(count));
    for (size_t i = 0; i < count; ++i) {
        RankRecord rec;
        file.read(reinterpret_cast<char*>(&rec.time), sizeof(rec.time));
        file.read(reinterpret_cast<char*>(&rec.deaths), sizeof(rec.deaths));
        rankList.push_back(rec);
    }
    std::sort(rankList.begin(), rankList.end());
    if (rankList.size() > MAX_RANK_COUNT) {
        rankList.resize(MAX_RANK_COUNT);
    }
}

void Game::SaveRanking() {
    std::ofstream file(rankFileName, std::ios::binary);
    if (!file.is_open()) return;
    
    size_t count = rankList.size();
    file.write(reinterpret_cast<const char*>(&count), sizeof(count));
    for (const auto& rec : rankList) {
        file.write(reinterpret_cast<const char*>(&rec.time), sizeof(rec.time));
        file.write(reinterpret_cast<const char*>(&rec.deaths), sizeof(rec.deaths));
    }
}

void Game::AddVictoryRecord() {
    RankRecord newRec;
    newRec.time = gameTimer;
    newRec.deaths = totalDeaths;
    rankList.push_back(newRec);
    std::sort(rankList.begin(), rankList.end());
    if (rankList.size() > MAX_RANK_COUNT) {
        rankList.resize(MAX_RANK_COUNT);
    }
    SaveRanking();
}

void Game::StartSinglePlayer() {
    currentState = GameState::SINGLE_MENU;
}