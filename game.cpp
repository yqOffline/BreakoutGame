#include "game.h"
#include "EffectFactory.h"
#include "SoundManager.h"
#include "TextureCache.h"
#include "ThreadPool.h"
#include "rlgl.h"
#include <set>
#include <fstream>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <ctime>
#include <sstream>
#include <iomanip>

// ========== UI 辅助函数 ==========
static void DrawRoundedRect(Rectangle rect, float radius, Color color) {
    if (radius <= 0) {
        DrawRectangleRec(rect, color);
        return;
    }
    // 限制半径不超过矩形最小边长的一半
    radius = fmin(radius, fmin(rect.width / 2.0f, rect.height / 2.0f));
    // 绘制中心矩形
    DrawRectangle(rect.x + radius, rect.y, rect.width - radius * 2, rect.height, color);
    DrawRectangle(rect.x, rect.y + radius, rect.width, rect.height - radius * 2, color);
    // 四个圆角
    DrawCircle(rect.x + radius, rect.y + radius, radius, color);
    DrawCircle(rect.x + rect.width - radius, rect.y + radius, radius, color);
    DrawCircle(rect.x + radius, rect.y + rect.height - radius, radius, color);
    DrawCircle(rect.x + rect.width - radius, rect.y + rect.height - radius, radius, color);
}

static bool IsPointInRect(Vector2 point, Rectangle rect) {
    return point.x >= rect.x && point.x <= rect.x + rect.width &&
           point.y >= rect.y && point.y <= rect.y + rect.height;
}

static void DrawButton(Rectangle rect, const char* text, int fontSize, Color normalColor, Color hoverColor, Color pressColor, bool isHover, bool isPressed) {
    Color drawColor = normalColor;
    if (isPressed) drawColor = pressColor;
    else if (isHover) drawColor = hoverColor;
    
    DrawRoundedRect(rect, 10.0f, drawColor);
    // 绘制边框
    DrawRectangleLinesEx(rect, 2, GRAY);
    
    int textWidth = MeasureText(text, fontSize);
    float textX = rect.x + (rect.width - textWidth) / 2;
    float textY = rect.y + (rect.height - fontSize) / 2;
    DrawText(text, textX, textY, fontSize, BLACK);
}

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
      levelLoadState(LevelLoadState::IDLE),
      brickGrid(80.0f, 40.0f, screenWidth, screenHeight)
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
    maxTrailLength = config["trail"].value("max_length", 10);
    // 3. 用配置值设置 balls
    float bx = config["ball"]["init_x"];
    float by = config["ball"]["init_y"];
    float br = config["ball"]["radius"];
    float spx = config["ball"]["speed_x"];
    float spy = config["ball"]["speed_y"];
    balls.emplace_back(Vector2{bx, by}, Vector2{spx, spy}, br);
    ballTrails.emplace_back(maxTrailLength);

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
    
    
    singleBtn = { (float)screenWidth / 2 - 100, 250, 200, 50 };
    raceBtn   = { (float)screenWidth / 2 - 100, 310, 200, 50 };
    versusBtn = { (float)screenWidth / 2 - 100, 370, 200, 50 };

    // 多人模式 Lobby 按钮
    hostBtn        = { (float)screenWidth / 2 - 100, 200, 200, 50 };
    guestBtn       = { (float)screenWidth / 2 - 100, 270, 200, 50 };
    startGameBtn   = { (float)screenWidth / 2 - 80,  350, 160, 50 };
    backToModeBtn2 = { (float)screenWidth / 2 - 60,  (float)screenHeight - 80, 120, 50 };
    
    startBtn        = { (float)screenWidth / 2 - 100, 220, 200, 50 };
    rankBtn         = { (float)screenWidth / 2 - 100, 290, 200, 50 };
    eraseRankBtn    = { (float)screenWidth / 2 - 100, 360, 200, 50 };
    continueGameBtn = { (float)screenWidth / 2 - 100, 150, 200, 50 };
    backToModeBtn   = { (float)screenWidth / 2 - 100, (float)screenHeight - 80, 200, 50 };
    continueBtn = { (float)screenWidth / 2 - 100, (float)screenHeight / 2 - 10, 200, 45 };
    restartBtn  = { (float)screenWidth / 2 - 100, (float)screenHeight / 2 + 50, 200, 45 };
    quitBtn     = { (float)screenWidth / 2 - 100, (float)screenHeight / 2 + 110, 200, 45 };
    gameOverRestartBtn = { (float)screenWidth / 2 - 100,(float)screenHeight / 2 + 20, 200, 50 };
    replayBtn = { (float)screenWidth / 2 - 160,(float)screenHeight / 2 + 20, 120, 50 };
    goAheadBtn = { (float)screenWidth / 2 + 40,(float)screenHeight / 2 + 20, 120, 50 };
    victoryRestartBtn = { (float)screenWidth / 2 - 130,(float)screenHeight / 2 + 60, 120, 50 };
    victoryReplayBtn = { (float)screenWidth / 2 + 10,(float)screenHeight / 2 + 60, 120, 50 };
    backBtn = { (float)screenWidth / 2 - 60,(float)screenHeight - 80, 120, 50 };
   
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

    // 确保粒子系统拥有纹理
    particleSystem.LoadDefaultTexture();

    // 10. 加载排行榜
    LoadRanking();

    fadeAlpha = 0.0f;
    fading = false;
    // 音量设置
    LoadVolumeSettings();
    draggingBgm = false;
    draggingSfx = false;
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

void Game::SaveGame() {
    SaveData data;
    data.version = 1;
    data.currentLevel = currentLevel;
    data.score = score;
    data.hearts = hearts;
    data.gameTimer = gameTimer;
    data.totalDeaths = totalDeaths;

    // 保存所有球
    for (const auto& ball : balls) {
        data.ballPosX.push_back(ball.GetPosition().x);
        data.ballPosY.push_back(ball.GetPosition().y);
        data.ballSpeedX.push_back(ball.GetSpeed().x);
        data.ballSpeedY.push_back(ball.GetSpeed().y);
        data.ballRadius.push_back(ball.GetRadius());
    }

    json saveJson;
    saveJson["version"] = data.version;
    saveJson["current_level"] = data.currentLevel;
    saveJson["score"] = data.score;
    saveJson["hearts"] = data.hearts;
    saveJson["game_timer"] = data.gameTimer;
    saveJson["total_deaths"] = data.totalDeaths;
    saveJson["ball_pos_x"] = data.ballPosX;
    saveJson["ball_pos_y"] = data.ballPosY;
    saveJson["ball_speed_x"] = data.ballSpeedX;
    saveJson["ball_speed_y"] = data.ballSpeedY;
    saveJson["ball_radius"] = data.ballRadius;

    std::ofstream file(saveFileName);
    if (file.is_open()) {
        file << saveJson.dump(4);
        std::cout << "Game saved. Level=" << currentLevel << " Balls=" << balls.size() << std::endl;
    } else {
        std::cerr << "Failed to save game!" << std::endl;
    }
}

bool Game::LoadGame() {
    std::ifstream file(saveFileName);
    if (!file.is_open()) {
        std::cout << "No save file found." << std::endl;
        return false;
    }

    json saveJson;
    try {
        file >> saveJson;
        int version = saveJson.value("version", 0);
        if (version != 1) {
            std::cerr << "Save version incompatible!" << std::endl;
            return false;
        }
        currentLevel = saveJson.value("current_level", 0);
        score = saveJson.value("score", 0);
        hearts = saveJson.value("hearts", config["game"]["initial_hearts"]);
        gameTimer = saveJson.value("game_timer", 0.0f);
        totalDeaths = saveJson.value("total_deaths", 0);
    } catch (const json::parse_error& e) {
        std::cerr << "Save file parse error: " << e.what() << std::endl;
        return false;
    }

    // 加载关卡（会重置砖块和初始球）
    LoadLevel(currentLevel, false);
    hearts = saveJson.value("hearts", config["game"]["initial_hearts"]);

    // 读取保存的球数据
    std::vector<float> posX = saveJson.value("ball_pos_x", std::vector<float>());
    std::vector<float> posY = saveJson.value("ball_pos_y", std::vector<float>());
    std::vector<float> spdX = saveJson.value("ball_speed_x", std::vector<float>());
    std::vector<float> spdY = saveJson.value("ball_speed_y", std::vector<float>());
    std::vector<float> radii = saveJson.value("ball_radius", std::vector<float>());

    if (!posX.empty()) {
        // 清空 LoadLevel 生成的默认球
        balls.clear();
        ballTrails.clear();
        for (size_t i = 0; i < posX.size(); ++i) {
            Vector2 pos = { posX[i], posY[i] };
            Vector2 sp = { spdX[i], spdY[i] };
            float r = (i < radii.size()) ? radii[i] : originalBallRadius;
            balls.emplace_back(pos, sp, r);
            ballTrails.emplace_back(maxTrailLength);
        }
    }

    timerRunning = true;
    currentState = GameState::PLAYING;
    return true;
}

bool Game::HasSaveFile() const {
    std::ifstream file(saveFileName);
    return file.good();
}

void Game::DeleteSaveFile() {
    std::remove(saveFileName.c_str());
}

void Game::ContinueGame() {
    if (LoadGame()) {
        gameJustLoaded = true;
    }
}

// ======================== 完全重置游戏状态 ========================
void Game::ResetGameState() {
    currentLevel = 0;
    LoadLevel(0);
    
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

    // 注意：activeBrickCount 已在 LoadLevel(0) 中正确计算，此处不再清零
    // activeBrickCount = 0;   // ← 删除这一行

    std::remove(saveFileName.c_str());
    editingMode = false;
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
    ballTrails.emplace_back(maxTrailLength);

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
        SaveGame();   // ★ 自动保存

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
            ballTrails.emplace_back(maxTrailLength);
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

    // 按 E 键切换编辑模式（仅在单机菜单或暂停状态可用）
    if (IsKeyPressed(KEY_E) && (currentState == GameState::SINGLE_MENU || currentState == GameState::PAUSED || currentState == GameState::PLAYING)) {
        ToggleEditMode();
        return;
    }
    
    // 编辑模式：只处理编辑输入
    if (editingMode) {
        HandleEditModeInput();
        return;
    }

    // 异步纹理加载快捷键（保留）
    if (IsKeyPressed(KEY_L) && loadState == LoadState::IDLE) {
        pendingTexturePath = "big_image.png";
        loadState = LoadState::LOADING;
        loadFuture = std::async(std::launch::async, [](const std::string& path) {
            return LoadImage(path.c_str());
        }, pendingTexturePath);
        return;
    }

    // 异步关卡加载快捷键（保留）
    if (IsKeyPressed(KEY_N) && levelLoadState == LevelLoadState::IDLE && currentState == GameState::PLAYING) {
        if (currentLevel + 1 < totalLevels) {
            pendingLevelIndex = currentLevel + 1;
            levelLoadState = LevelLoadState::LOADING;
            static ThreadPool levelPool(2);
            levelLoadFuture = levelPool.Enqueue([this]() {
                return GenerateLevelData(pendingLevelIndex);
            });
        }
        return;
    }

    switch (currentState) {
        case GameState::MODE_SELECT: {
            if ((IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, singleBtn)) || IsKeyPressed(KEY_ONE)) {
                soundManager.PlayUIClick();
                currentState = GameState::SINGLE_MENU;
            }
            if ((IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, raceBtn)) || IsKeyPressed(KEY_TWO)) {
                soundManager.PlayUIClick();
                exitToRaceLobby = true;
            }
            if ((IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, versusBtn)) || IsKeyPressed(KEY_THREE)) {
                soundManager.PlayUIClick();
                exitToVersusLobby = true;
            }

            if ((IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, Rectangle{ GetScreenWidth() / 2.0f - 100, 440, 200, 50 })) || IsKeyPressed(KEY_O)) {
                soundManager.PlayUIClick();
                currentState = GameState::SETTINGS;
            }
            break;
        }

        case GameState::SINGLE_MENU:{
            // 开始新游戏
            if ((IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, startBtn)) || IsKeyPressed(KEY_SPACE)) {
                soundManager.PlayUIClick();
                StartSinglePlayer();
                currentState = GameState::PLAYING;
                for (auto& b : balls) b.SetSpeed({ config["ball"]["speed_x"], config["ball"]["speed_y"] });
                timerRunning = true;
            }
            // 排行榜
            if ((IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, rankBtn)) || IsKeyPressed(KEY_R)) {
                soundManager.PlayUIClick();
                currentState = GameState::RANKING;
            }
            // 清除排行榜
            if ((IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, eraseRankBtn)) || IsKeyPressed(KEY_E)) {
                soundManager.PlayUIClick();
                ClearRanking();
                showEraseHint = true;
                eraseHintTimer = 0.5f;
            }
            // 返回模式选择
            if ((IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, backToModeBtn)) || IsKeyPressed(KEY_B)) {
                soundManager.PlayUIClick();
                currentState = GameState::MODE_SELECT;
            }
            // 继续游戏（读档）
            if ((IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, continueGameBtn)) || IsKeyPressed(KEY_C)) {
                soundManager.PlayUIClick();
                if (LoadGame()) {
                    // 加载成功，游戏状态已在 LoadGame 中设为 PLAYING，无需额外动作
                } else {
                    showEraseHint = true;
                    eraseHintTimer = 2.0f;
                }
            }
            break;
        }
        case GameState::MULTIPLAYER_MENU:
        case GameState::MULTIPLAYER_LOBBY: {
            int sw = GetScreenWidth();
            int sh = GetScreenHeight();
            
            // 半透明遮罩加强背景暗化
            DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.6f));
            
            // 主面板
            Rectangle panel = { sw/2.0f - 280, sh/2.0f - 180, 560, 440 };
            DrawRoundedRect(panel, 20, Fade(GRAY, 0.7f));
            
            const char* modeText = isRaceMode ? "RACE MODE" : "VERSUS MODE";
            DrawText(modeText, sw/2 - 120, sh/2 - 140, 40, GOLD);
            
            DrawText("Select your role", sw/2 - 80, sh/2 - 80, 20, LIGHTGRAY);
            
            // Host 按钮
            bool hoverHost = CheckCollisionPointRec(GetMousePosition(), hostBtn);
            bool pressHost = IsMouseButtonDown(MOUSE_LEFT_BUTTON) && hoverHost;
            DrawButton(hostBtn, "HOST (H)", 24, isHost ? DARKGREEN : BLUE, DARKBLUE, MAROON, hoverHost, pressHost);
            
            // Guest 按钮
            bool hoverGuest = CheckCollisionPointRec(GetMousePosition(), guestBtn);
            bool pressGuest = IsMouseButtonDown(MOUSE_LEFT_BUTTON) && hoverGuest;
            DrawButton(guestBtn, "GUEST (G)", 24, isGuest ? DARKGREEN : BLUE, DARKBLUE, MAROON, hoverGuest, pressGuest);
            
            // 如果 Host 或 Guest 被选中，显示模拟的连接状态
            if (isHost || isGuest) {
                DrawText("Waiting for network connection...", sw/2 - 160, sh/2 + 20, 18, YELLOW);
                // 绘制旋转加载指示器
                float angle = GetTime() * 3.0f;
                Vector2 center = { sw/2.0f, sh/2.0f + 50 };
                DrawCircleSector(center, 12, angle*57.3f, (angle+60)*57.3f, 8, Fade(WHITE, 0.7f));
            }
            
            // 开始按钮（仅当有角色选择时启用）
            bool canStart = (isHost || isGuest);
            Color startColor = canStart ? GREEN : GRAY;
            bool hoverStart = canStart && CheckCollisionPointRec(GetMousePosition(), startGameBtn);
            bool pressStart = canStart && IsMouseButtonDown(MOUSE_LEFT_BUTTON) && hoverStart;
            DrawButton(startGameBtn, "READY (S)", 20, startColor, DARKGREEN, MAROON, hoverStart, pressStart);
            if (!canStart) {
                DrawText("Select HOST or GUEST first", sw/2 - 140, sh/2 + 100, 18, RED);
            }
            
            // 返回按钮
            bool hoverBack = CheckCollisionPointRec(GetMousePosition(), backToModeBtn2);
            bool pressBack = IsMouseButtonDown(MOUSE_LEFT_BUTTON) && hoverBack;
            DrawButton(backToModeBtn2, "BACK", 20, GRAY, DARKGRAY, BLACK, hoverBack, pressBack);
            
            // 底部提示
            DrawText("Host: wait for client, then press READY", sw/2 - 180, sh - 50, 16, WHITE);
            DrawText("Guest: connect to host, then wait for host", sw/2 - 180, sh - 30, 16, WHITE);
            break;
        }

        case GameState::MULTIPLAYER_READY: {
            int sw = GetScreenWidth();
            int sh = GetScreenHeight();
            
            DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.6f));
            Rectangle panel = { sw/2.0f - 280, sh/2.0f - 140, 560, 280 };
            DrawRoundedRect(panel, 20, Fade(GRAY, 0.7f));
            
            const char* modeText = isRaceMode ? "RACE MODE" : "VERSUS MODE";
            DrawText(modeText, sw/2 - 120, sh/2 - 100, 36, GOLD);
            
            if (isHost) {
                DrawText("You are the HOST", sw/2 - 80, sh/2 - 40, 24, BLUE);
                DrawText("Press START to begin", sw/2 - 100, sh/2 + 0, 20, WHITE);
                
                bool hoverStart = CheckCollisionPointRec(GetMousePosition(), startGameBtn);
                bool pressStart = IsMouseButtonDown(MOUSE_LEFT_BUTTON) && hoverStart;
                DrawButton(startGameBtn, "START (S)", 24, GREEN, DARKGREEN, MAROON, hoverStart, pressStart);
            } else if (isGuest) {
                DrawText("You are the GUEST", sw/2 - 80, sh/2 - 40, 24, RED);
                DrawText("Waiting for host to start...", sw/2 - 130, sh/2 + 0, 20, LIGHTGRAY);
                // 三个跳动圆点
                float time = GetTime();
                for (int i = 0; i < 3; ++i) {
                    float alpha = 0.3f + 0.7f * (sinf(time * 5 + i * 2) * 0.5f + 0.5f);
                    DrawCircle(sw/2 - 40 + i * 25, sh/2 + 40, 8, Fade(WHITE, alpha));
                }
            }
            
            // 返回按钮
            bool hoverBack = CheckCollisionPointRec(GetMousePosition(), backToModeBtn2);
            bool pressBack = IsMouseButtonDown(MOUSE_LEFT_BUTTON) && hoverBack;
            DrawButton(backToModeBtn2, "BACK", 20, GRAY, DARKGRAY, BLACK, hoverBack, pressBack);
            break;
        }

        case GameState::PLAYING:
            if (IsKeyPressed(KEY_SPACE)) {
                soundManager.PlayUIClick();   // 暂停按钮音效
                currentState = GameState::PAUSED;
                pauseCause = PauseCause::MANUAL_PAUSE;
                timerRunning = false;
            }
            break;

        case GameState::PAUSED:
            // Continue 按钮
            if (IsKeyPressed(KEY_SPACE) || (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, continueBtn))) {
                soundManager.PlayUIClick();
                currentState = GameState::PLAYING;
                timerRunning = true;
            }
            // Restart 按钮
            if ((IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, restartBtn)) || IsKeyPressed(KEY_R)) {
                soundManager.PlayUIClick();
                StartSinglePlayer();
            }
            // Quit 按钮
            if ((IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, quitBtn)) || IsKeyPressed(KEY_Q)) {
                soundManager.PlayUIClick();
                SaveGame();
                currentState = GameState::MODE_SELECT;
                timerRunning = false;
            }
            break;

        case GameState::GAME_OVER:
            if ((IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, gameOverRestartBtn)) || IsKeyPressed(KEY_SPACE)) {
                soundManager.PlayUIClick();
                ResetGame();
            }
            break;

        case GameState::LEVEL_CLEAR:
            if (IsKeyPressed(KEY_P) || (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, replayBtn))) {
                soundManager.PlayUIClick();
                LoadLevel(currentLevel);
                for (auto& b : balls) b.SetSpeed({ config["ball"]["speed_x"], config["ball"]["speed_y"] });
                currentState = GameState::PLAYING;
                timerRunning = true;
            }
            if (IsKeyPressed(KEY_G) || (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, goAheadBtn))) {
                soundManager.PlayUIClick();
                currentLevel++;
                LoadLevel(currentLevel);
                for (auto& b : balls) b.SetSpeed({ config["ball"]["speed_x"], config["ball"]["speed_y"] });
                currentState = GameState::PLAYING;
                timerRunning = true;
            }
            break;

        case GameState::VICTORY:
            if ((IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, victoryRestartBtn)) || IsKeyPressed(KEY_R)) {
                soundManager.PlayUIClick();
                ResetGameState();
                currentState = GameState::MODE_SELECT;
                timerRunning = false;
            }
            if ((IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, victoryReplayBtn)) || IsKeyPressed(KEY_P)) {
                soundManager.PlayUIClick();
                LoadLevel(currentLevel);
                for (auto& b : balls) b.SetSpeed({ config["ball"]["speed_x"], config["ball"]["speed_y"] });
                currentState = GameState::PLAYING;
                timerRunning = true;
            }
            break;

        case GameState::RANKING:
            if ((IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, backBtn)) || IsKeyPressed(KEY_B)) {
                soundManager.PlayUIClick();
                currentState = GameState::SINGLE_MENU;
            }
            break;
        case GameState::SETTINGS: {
            Vector2 mousePos = GetMousePosition();

            // ----- 背景音乐滑块拖拽 -----
            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, bgmSlider)) {
                draggingBgm = true;
            }
            if (draggingBgm) {
                if (IsMouseButtonUp(MOUSE_LEFT_BUTTON)) {
                    draggingBgm = false;
                } else {
                    float newX = mousePos.x - bgmSlider.x;
                    newX = fmaxf(0.0f, fminf(newX, bgmSlider.width));
                    bgmVolume = newX / bgmSlider.width;
                    ApplyVolume();   // 立即应用音量（需实现该函数）
                }
            }

            // ----- 音效滑块拖拽 -----
            if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, sfxSlider)) {
                draggingSfx = true;
            }
            if (draggingSfx) {
                if (IsMouseButtonUp(MOUSE_LEFT_BUTTON)) {
                    draggingSfx = false;
                } else {
                    float newX = mousePos.x - sfxSlider.x;
                    newX = fmaxf(0.0f, fminf(newX, sfxSlider.width));
                    sfxVolume = newX / sfxSlider.width;
                    ApplyVolume();   // 立即应用音量
                }
            }

            // ----- 保存按钮 -----
            Rectangle saveBtn = { GetScreenWidth() / 2.0f - 40, GetScreenHeight() / 2.0f + 100, 80, 40 };
            if ((IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, saveBtn)) || IsKeyPressed(KEY_S)) {
                soundManager.PlayUIClick();
                SaveVolumeSettings();
                ApplyVolume();   // 保存时也应用一次，确保音量生效
            }

            // ----- 返回按钮 -----
            Rectangle backBtn = { GetScreenWidth() / 2.0f - 40, (float)GetScreenHeight() - 60, 80, 40 };
            if ((IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, backBtn)) || IsKeyPressed(KEY_B)) {
                soundManager.PlayUIClick();
                currentState = GameState::MODE_SELECT;
            }

            break;
        }
        default:
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
    if (fading) {
        fadeAlpha -= dt * 2.0f;  // 每秒减少2，约0.5秒完成
        if (fadeAlpha <= 0.0f) {
            fadeAlpha = 0.0f;
            fading = false;
        }
    }
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
        return;
    }

    // ---------- 板块二：异步关卡加载处理 ----------
    if (levelLoadState == LevelLoadState::LOADING) {
        if (levelLoadFuture.valid() &&
            levelLoadFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            LevelLoadData data = levelLoadFuture.get();
            ApplyLevelLoadData(data);
            for (auto& b : balls) b.SetSpeed({ config["ball"]["speed_x"], config["ball"]["speed_y"] });
            levelLoadState = LevelLoadState::DONE;
        }
        return;
    }

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
    
    // ========== 性能测量起点 ==========
    double totalStart = GetTime();
    /*
    // 后门按键（不计入测量）
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
    }*/
    
    // 1. 拖尾记录 + 球移动与边界/挡板碰撞 → 物理部分
    double physStart = GetTime();
    for (size_t i = 0; i < balls.size(); ++i) {
        ballTrails[i].push(balls[i].GetPosition());
    }
    for (auto& ball : balls) {
        ball.Move();
        if (ball.BounceEdge(GetScreenWidth(), GetScreenHeight())) {
            soundManager.PlayHitSound();
        }
        if (ball.CheckCollisionPaddle(paddle)) {
            soundManager.PlayHitSound();
        }
    }
    m_physicsTime = GetTime() - physStart;

    // 2. 砖块碰撞检测
    double brickStart = GetTime();
    // 使用网格查询候选砖块
    std::vector<Brick*> candidates;
    for (auto& ball : balls) {
        brickGrid.Query(ball.GetPosition(), ball.GetRadius(), candidates);
        for (Brick* brickPtr : candidates) {
            Brick& brick = *brickPtr;
            if (!brick.IsActive()) continue;   // 双重检查，虽然 Query 已过滤
            if (CheckCollisionCircleRec(ball.GetPosition(), ball.GetRadius(), brick.GetRectangle())) {
                soundManager.PlayHitSound();
                int damage = 1;
                if (HasEffectOfType("Explosion")) damage = 2;
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
                        }
                        skillBalls.emplace_back(spawnPos, type, skillBallRadius,
                                                Vector2{0, skillBallSpeedY}, glowColor);
                    }
                    activeBrickCount--;  // ★ 活跃砖块减少
                }
                Vector2 sp = ball.GetSpeed();
                sp.y *= -1;
                ball.SetSpeed(sp);
                if (sp.y > 0)
                    ball.SetPosition({ ball.GetPosition().x, brick.GetRectangle().y + brick.GetRectangle().height + ball.GetRadius() });
                else
                    ball.SetPosition({ ball.GetPosition().x, brick.GetRectangle().y - ball.GetRadius() });
                // 注意：break 只跳出内层 for (candidates)，仍需跳出球循环吗？
                // 原逻辑每个球只处理一块砖，然后 break 外层循环。此处应保留 break。
                goto nextBall; // 跳出到下一个球
            }
        }
        nextBall:;
    }

    // 3. 球间碰撞
    HandleBallCollisions();
    m_collisionTime = GetTime() - brickStart;

    // 4. 技能球更新 + 检测收集
    double skillStart = GetTime();
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

    // 5. 粒子系统
    particleSystem.Update(dt, particleGravity);
    m_skillParticleTime = GetTime() - skillStart;

    // 6. 效果更新
    double effectStart = GetTime();
    UpdateEffects(dt);
    m_effectsTime = GetTime() - effectStart;

    // 7. 挡板移动
    if (IsKeyDown(KEY_LEFT)) paddle.MoveLeft(paddleMoveSpeed);
    if (IsKeyDown(KEY_RIGHT)) paddle.MoveRight(paddleMoveSpeed);
/*
    // 8. 最后一块砖动画（略）
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
*/
    // 9. 红线碰撞检测
    CheckBallHitRedLine();

    // 10. 关卡过渡检查
    CheckLevelTransition();

    m_totalTime = GetTime() - totalStart;
    // ========== 测量结束 ==========
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
        DrawHUD();
    }

    // UI 文字
    DrawText(TextFormat("SCORE: %d", score), 10, 10, 20, BLUE);
    DrawText(TextFormat("LIVES: %d", hearts), 10, 40, 20, RED);
    DrawText(TextFormat("LEVEL: %d", currentLevel + 1), 10, 70, 20, DARKGREEN);
    DrawText(TextFormat("TIME: %.1f", gameTimer), 10, 100, 20, PURPLE);
    DrawText(TextFormat("DEATHS: %d", totalDeaths), 10, 130, 20, MAROON);

    int effectY = 160;
    for (const auto& effect : activeEffects) {
        DrawText(TextFormat("%s: %.1fs", effect->GetName().c_str(), effect->GetRemainingTime()), 10, effectY, 20, GREEN);
        effectY += 20;
    }

    // 根据不同状态绘制 UI（完整按钮）
    switch (currentState) {
        case GameState::MODE_SELECT: {
            int sw = GetScreenWidth();
            int sh = GetScreenHeight();
            // 背景半透明黑色圆角面板
            Rectangle panel = { sw/2.0f - 200, 80, 400, 440 };
            DrawRoundedRect(panel, 25, Fade(WHITE, 0.4f)); 
            // 标题
            DrawText("2D BREAKOUT", sw/2 - 140, 120, 48, GOLD);
            DrawText("SELECT MODE", sw/2 - 100, 180, 28, SKYBLUE);
            
            // 单机按钮
            bool hoverSingle = IsPointInRect(GetMousePosition(), singleBtn);
            bool pressSingle = IsMouseButtonDown(MOUSE_LEFT_BUTTON) && hoverSingle;
            DrawButton(singleBtn, "SINGLE (1)", 24, GREEN, DARKGREEN, MAROON, hoverSingle, pressSingle);
            
            // 竞速按钮
            bool hoverRace = IsPointInRect(GetMousePosition(), raceBtn);
            bool pressRace = IsMouseButtonDown(MOUSE_LEFT_BUTTON) && hoverRace;
            DrawButton(raceBtn, "RACE (2)", 24, BLUE, DARKBLUE, MAROON, hoverRace, pressRace);
            
            // 对战按钮
            bool hoverVersus = IsPointInRect(GetMousePosition(), versusBtn);
            bool pressVersus = IsMouseButtonDown(MOUSE_LEFT_BUTTON) && hoverVersus;
            DrawButton(versusBtn, "VERSUS (3)", 24, PURPLE, PURPLE, MAROON, hoverVersus, pressVersus);

            // 设置按钮
            Rectangle settingsBtn = { sw/2.0f - 100, 440, 200, 50 };
            bool hoverSettings = CheckCollisionPointRec(GetMousePosition(), settingsBtn);
            bool pressSettings = IsMouseButtonDown(MOUSE_LEFT_BUTTON) && hoverSettings;
            DrawButton(settingsBtn, "SETTINGS (O)", 20, GRAY, DARKGRAY, BLACK, hoverSettings, pressSettings);
            break;
        }
        case GameState::SINGLE_MENU: {
            int sw = GetScreenWidth();
            int sh = GetScreenHeight();
            // 调整面板位置和高度：Y=60, 高度=440 (容纳按钮和标题)
            Rectangle panel = { sw/2.0f - 180, 60, 360, 440 };
            DrawRoundedRect(panel, 20, Fade(WHITE, 0.4f));
            
            // 标题文字位置：Y=100 (比之前稍高，与调整后的按钮布局协调)
            DrawText("SINGLE PLAYER", sw/2 - 120, 100, 36, GOLD);
            
            // 绘制按钮（悬停/按下效果）
            bool hoverCont = IsPointInRect(GetMousePosition(), continueGameBtn);
            bool pressCont = IsMouseButtonDown(MOUSE_LEFT_BUTTON) && hoverCont;
            DrawButton(continueGameBtn, "CONTINUE (C)", 20, PINK, MAGENTA, PURPLE, hoverCont, pressCont);
            
            bool hoverStart = IsPointInRect(GetMousePosition(), startBtn);
            bool pressStart = IsMouseButtonDown(MOUSE_LEFT_BUTTON) && hoverStart;
            DrawButton(startBtn, "START GAME", 20, GREEN, DARKGREEN, MAROON, hoverStart, pressStart);
            
            bool hoverRank = IsPointInRect(GetMousePosition(), rankBtn);
            bool pressRank = IsMouseButtonDown(MOUSE_LEFT_BUTTON) && hoverRank;
            DrawButton(rankBtn, "RANK (R)", 20, BLUE, DARKBLUE, MAROON, hoverRank, pressRank);
            
            bool hoverErase = IsPointInRect(GetMousePosition(), eraseRankBtn);
            bool pressErase = IsMouseButtonDown(MOUSE_LEFT_BUTTON) && hoverErase;
            DrawButton(eraseRankBtn, "ERASE (E)", 20, RED, MAROON, MAROON, hoverErase, pressErase);
            
            bool hoverBack = IsPointInRect(GetMousePosition(), backToModeBtn);
            bool pressBack = IsMouseButtonDown(MOUSE_LEFT_BUTTON) && hoverBack;
            DrawButton(backToModeBtn, "BACK (B)", 20, GRAY, DARKGRAY, BLACK, hoverBack, pressBack);
            
            if (showEraseHint) {
                DrawRoundedRect({ sw/2.0f - 200, sh/2.0f - 40, 400, 80 }, 15, Fade(BLACK, 0.8f));
                DrawText("RANKING ERASED!", sw/2 - 100, sh/2 - 20, 24, RED);
            }
            break;
        }
        case GameState::MULTIPLAYER_MENU:
        case GameState::MULTIPLAYER_LOBBY: {
            int sw = GetScreenWidth();
            int sh = GetScreenHeight();

            // 绘制背景图片 (1.png)
            Texture2D bg = TextureCache::Instance().GetTexture("1.png");
            if (bg.id != 0) {
                DrawTexturePro(bg,
                    { 0, 0, (float)bg.width, (float)bg.height },
                    { 0, 0, (float)sw, (float)sh },
                    { 0, 0 }, 0, WHITE);
            } else {
                ClearBackground(DARKGRAY);
            }

            // 半透明遮罩，让文字更清晰
            DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.5f));

            // 主圆角面板
            Rectangle panel = { sw/2.0f - 250, sh/2.0f - 160, 500, 320 };
            DrawRoundedRect(panel, 20, Fade(GRAY, 0.7f));

            // 标题
            const char* modeText = isRaceMode ? "RACE MODE" : "VERSUS MODE";
            DrawText(modeText, sw/2 - 120, sh/2 - 120, 40, GOLD);
            DrawText("Select your role", sw/2 - 80, sh/2 - 70, 20, LIGHTGRAY);

            // Host 按钮
            bool hoverHost = CheckCollisionPointRec(GetMousePosition(), hostBtn);
            bool pressHost = IsMouseButtonDown(MOUSE_LEFT_BUTTON) && hoverHost;
            DrawButton(hostBtn, "HOST (H)", 24, isHost ? DARKGREEN : BLUE, DARKBLUE, MAROON, hoverHost, pressHost);

            // Guest 按钮
            bool hoverGuest = CheckCollisionPointRec(GetMousePosition(), guestBtn);
            bool pressGuest = IsMouseButtonDown(MOUSE_LEFT_BUTTON) && hoverGuest;
            DrawButton(guestBtn, "GUEST (G)", 24, isGuest ? DARKGREEN : BLUE, DARKBLUE, MAROON, hoverGuest, pressGuest);

            // Ready 按钮 (仅当有角色选择时可用)
            bool canStart = (isHost || isGuest);
            Color startColor = canStart ? GREEN : GRAY;
            bool hoverStart = canStart && CheckCollisionPointRec(GetMousePosition(), startGameBtn);
            bool pressStart = canStart && IsMouseButtonDown(MOUSE_LEFT_BUTTON) && hoverStart;
            DrawButton(startGameBtn, "READY (S)", 20, startColor, DARKGREEN, MAROON, hoverStart, pressStart);
            if (!canStart) {
                DrawText("Select HOST or GUEST first", sw/2 - 140, sh/2 + 90, 18, RED);
            }

            // 返回按钮
            bool hoverBack = CheckCollisionPointRec(GetMousePosition(), backToModeBtn2);
            bool pressBack = IsMouseButtonDown(MOUSE_LEFT_BUTTON) && hoverBack;
            DrawButton(backToModeBtn2, "BACK", 20, GRAY, DARKGRAY, BLACK, hoverBack, pressBack);

            // 提示文字
            DrawText("Host: wait for client, then press READY", sw/2 - 180, sh - 100, 16, WHITE);
            DrawText("Guest: connect to host, then wait for host", sw/2 - 180, sh - 80, 16, WHITE);
            break;
        }

        case GameState::MULTIPLAYER_READY: {
            int sw = GetScreenWidth();
            int sh = GetScreenHeight();

            DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.5f));
            Rectangle panel = { sw/2.0f - 250, sh/2.0f - 120, 500, 240 };
            DrawRectangleRounded(panel, 0.2f, 10, Fade(GRAY, 0.8f));
            DrawRectangleLinesEx(panel, 2, Fade(WHITE, 0.3f));

            const char* modeText = isRaceMode ? "RACE MODE" : "VERSUS MODE";
            DrawText(modeText, sw/2 - 120, sh/2 - 90, 36, GOLD);

            if (isHost) {
                DrawText("You are the HOST", sw/2 - 80, sh/2 - 30, 24, SKYBLUE);
                DrawText("Press START to begin", sw/2 - 100, sh/2 + 10, 20, WHITE);
                bool hoverStart = CheckCollisionPointRec(GetMousePosition(), startGameBtn);
                Color startColor = hoverStart ? DARKGREEN : GREEN;
                DrawRectangleRounded(startGameBtn, 0.2f, 8, startColor);
                DrawText("START (S)", startGameBtn.x + 35, startGameBtn.y + 12, 20, BLACK);
            } else if (isGuest) {
                DrawText("You are the GUEST", sw/2 - 80, sh/2 - 30, 24, RED);
                DrawText("Waiting for host to start...", sw/2 - 130, sh/2 + 10, 20, LIGHTGRAY);
                // 等待动画：三个圆点
                float time = GetTime();
                for (int i = 0; i < 3; ++i) {
                    float alpha = 0.3f + 0.7f * (sinf(time * 5 + i * 2) * 0.5f + 0.5f);
                    DrawCircle(sw/2 - 40 + i * 25, sh/2 + 50, 6, Fade(WHITE, alpha));
                }
            }

            bool hoverBack = CheckCollisionPointRec(GetMousePosition(), backToModeBtn2);
            Color backColor = hoverBack ? DARKGRAY : GRAY;
            DrawRectangleRounded(backToModeBtn2, 0.2f, 8, backColor);
            DrawText("BACK", backToModeBtn2.x + 35, backToModeBtn2.y + 12, 20, WHITE);
            break;
        }

        case GameState::PAUSED: {
            int sw = GetScreenWidth();
            int sh = GetScreenHeight();

            // 背景保持游戏画面（不覆盖），只需绘制半透明白色面板
            // 先绘制半透明遮罩使背景变暗，突出面板
            DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.4f));

            // 主面板
            Rectangle panel = { sw/2.0f - 220, sh/2.0f - 130, 440, 300 };
            DrawRectangleRounded(panel, 0.2f, 10, Fade(WHITE, 0.9f));
            DrawRectangleLinesEx(panel, 2, BLACK);

            DrawText("PAUSED", sw/2 - 70, sh/2 - 110, 48, DARKBLUE);

            // 显示当前关卡和分数
            DrawText(TextFormat("Level: %d   Score: %d", currentLevel + 1, score),
                    sw/2 - 100, sh/2 - 50, 20, DARKGRAY);

            // 按钮：继续游戏
            Rectangle continueRect = { sw/2.0f - 100, sh/2.0f - 10, 200, 45 };
            bool hoverCont = CheckCollisionPointRec(GetMousePosition(), continueRect);
            bool pressCont = IsMouseButtonDown(MOUSE_LEFT_BUTTON) && hoverCont;
            Color contColor = hoverCont ? DARKGREEN : GREEN;
            DrawRectangleRounded(continueRect, 0.2f, 8, contColor);
            DrawRectangleLinesEx(continueRect, 2, BLACK);
            DrawText("CONTINUE", continueRect.x + 40, continueRect.y + 10, 22, WHITE);

            // 按钮：重新开始
            Rectangle restartRect = { sw/2.0f - 100, sh/2.0f + 50, 200, 45 };
            bool hoverRest = CheckCollisionPointRec(GetMousePosition(), restartRect);
            bool pressRest = IsMouseButtonDown(MOUSE_LEFT_BUTTON) && hoverRest;
            Color restColor = hoverRest ? MAROON : RED;
            DrawRectangleRounded(restartRect, 0.2f, 8, restColor);
            DrawRectangleLinesEx(restartRect, 2, BLACK);
            DrawText("RESTART", restartRect.x + 50, restartRect.y + 10, 22, WHITE);

            // 按钮：退出到主菜单（保存游戏）
            Rectangle quitRect = { sw/2.0f - 100, sh/2.0f + 110, 200, 45 };
            bool hoverQuit = CheckCollisionPointRec(GetMousePosition(), quitRect);
            bool pressQuit = IsMouseButtonDown(MOUSE_LEFT_BUTTON) && hoverQuit;
            Color quitColor = hoverQuit ? DARKGRAY : GRAY;
            DrawRectangleRounded(quitRect, 0.2f, 8, quitColor);
            DrawRectangleLinesEx(quitRect, 2, BLACK);
            DrawText("QUIT & SAVE", quitRect.x + 35, quitRect.y + 10, 22, WHITE);

            break;
        }
        case GameState::GAME_OVER: {
            int sw = GetScreenWidth();
            int sh = GetScreenHeight();
            DrawRoundedRect({ sw/2.0f - 200, sh/2.0f - 100, 400, 200 }, 20, Fade(BLACK, 0.85f));
            DrawText("GAME OVER", sw/2 - 110, sh/2 - 60, 48, RED);
            
            bool hoverRest = IsPointInRect(GetMousePosition(), gameOverRestartBtn);
            bool pressRest = IsMouseButtonDown(MOUSE_LEFT_BUTTON) && hoverRest;
            DrawButton(gameOverRestartBtn, "PLAY AGAIN", 24, GREEN, DARKGREEN, MAROON, hoverRest, pressRest);
            break;
        }
        case GameState::LEVEL_CLEAR: {
            int sw = GetScreenWidth();
            int sh = GetScreenHeight();
            DrawRoundedRect({ sw/2.0f - 200, sh/2.0f - 100, 400, 200 }, 20, Fade(BLACK, 0.85f));
            DrawText(TextFormat("LEVEL %d CLEAR!", currentLevel + 1), sw/2 - 140, sh/2 - 60, 36, YELLOW);
            
            bool hoverReplay = IsPointInRect(GetMousePosition(), replayBtn);
            bool pressReplay = IsMouseButtonDown(MOUSE_LEFT_BUTTON) && hoverReplay;
            DrawButton(replayBtn, "REPLAY (P)", 20, BLUE, DARKBLUE, MAROON, hoverReplay, pressReplay);
            
            bool hoverGo = IsPointInRect(GetMousePosition(), goAheadBtn);
            bool pressGo = IsMouseButtonDown(MOUSE_LEFT_BUTTON) && hoverGo;
            DrawButton(goAheadBtn, "GO AHEAD (G)", 20, GREEN, DARKGREEN, MAROON, hoverGo, pressGo);
            break;
        }
        case GameState::VICTORY: {
            int sw = GetScreenWidth();
            int sh = GetScreenHeight();
            DrawRoundedRect({ sw/2.0f - 220, sh/2.0f - 140, 440, 280 }, 20, Fade(BLACK, 0.85f));
            DrawText("VICTORY!", sw/2 - 80, sh/2 - 100, 56, GOLD);
            DrawText(TextFormat("Time: %.2f s", gameTimer), sw/2 - 80, sh/2 - 30, 24, WHITE);
            DrawText(TextFormat("Deaths: %d", totalDeaths), sw/2 - 60, sh/2 + 10, 24, WHITE);
            
            bool hoverRest = IsPointInRect(GetMousePosition(), victoryRestartBtn);
            bool pressRest = IsMouseButtonDown(MOUSE_LEFT_BUTTON) && hoverRest;
            DrawButton(victoryRestartBtn, "RESTART", 20, GREEN, DARKGREEN, MAROON, hoverRest, pressRest);
            
            bool hoverReplay = IsPointInRect(GetMousePosition(), victoryReplayBtn);
            bool pressReplay = IsMouseButtonDown(MOUSE_LEFT_BUTTON) && hoverReplay;
            DrawButton(victoryReplayBtn, "REPLAY", 20, BLUE, DARKBLUE, MAROON, hoverReplay, pressReplay);
            break;
        }
        case GameState::RANKING: {
            int sw = GetScreenWidth();
            int sh = GetScreenHeight();

            // 背景图片
            Texture2D bg = TextureCache::Instance().GetTexture("1.png");
            if (bg.id != 0) {
                DrawTexturePro(bg,
                    {0, 0, (float)bg.width, (float)bg.height},
                    {0, 0, (float)sw, (float)sh},
                    {0, 0}, 0, WHITE);
            } else {
                ClearBackground(DARKGRAY);
            }

            // 半透明白色圆角面板
            Rectangle panel = { sw/2.0f - 300, 60, 600, 460 };
            DrawRectangleRounded(panel, 0.2f, 10, Fade(WHITE, 0.85f));
            DrawRectangleLinesEx(panel, 2, BLACK);

            DrawText("RANKING (TOP 5)", sw/2 - 130, 90, 36, DARKBLUE);

            // 表头
            DrawText("Rank", sw/2 - 220, 150, 22, DARKGRAY);
            DrawText("Time (s)", sw/2 - 100, 150, 22, DARKGRAY);
            DrawText("Deaths", sw/2 + 80, 150, 22, DARKGRAY);

            // 分隔线
            DrawLine(sw/2 - 250, 170, sw/2 + 250, 170, GRAY);

            // 显示记录
            for (int i = 0; i < (int)rankList.size() && i < MAX_RANK_COUNT; ++i) {
                const auto& rec = rankList[i];
                int y = 190 + i * 50;
                // 交替行背景色（浅灰）
                if (i % 2 == 1) {
                    DrawRectangle(sw/2 - 240, y - 5, 480, 40, Fade(LIGHTGRAY, 0.3f));
                }
                const char* rankStr = TextFormat("%d.", i+1);
                DrawText(rankStr, sw/2 - 210, y, 22, BLACK);
                DrawText(rankStr, sw/2 - 210, y, 22, BLACK);
                DrawText(TextFormat("%.2f", rec.time), sw/2 - 100, y, 22, BLACK);
                DrawText(TextFormat("%d", rec.deaths), sw/2 + 80, y, 22, BLACK);
            }
            if (rankList.empty()) {
                DrawText("No records yet", sw/2 - 70, 250, 24, DARKGRAY);
            }

            // 返回按钮
            Rectangle backBtnRect = { sw/2.0f - 60, (float)sh - 70, 120, 50 };
            bool hoverBack = CheckCollisionPointRec(GetMousePosition(), backBtnRect);
            bool pressBack = IsMouseButtonDown(MOUSE_LEFT_BUTTON) && hoverBack;
            Color backColor = hoverBack ? DARKGRAY : GRAY;
            DrawRectangleRounded(backBtnRect, 0.2f, 8, backColor);
            DrawRectangleLinesEx(backBtnRect, 2, BLACK);
            DrawText("BACK (B)", backBtnRect.x + 20, backBtnRect.y + 12, 20, WHITE);

            break;
        }
        case GameState::SETTINGS: {
            int sw = GetScreenWidth();
            int sh = GetScreenHeight();

            // 背景图片
            Texture2D bg = TextureCache::Instance().GetTexture("1.png");
            if (bg.id != 0) {
                DrawTexturePro(bg,
                    {0, 0, (float)bg.width, (float)bg.height},
                    {0, 0, (float)sw, (float)sh},
                    {0, 0}, 0, WHITE);
            } else {
                ClearBackground(DARKGRAY);
            }

            // 半透明白色圆角面板
            Rectangle panel = { sw/2.0f - 200, sh/2.0f - 150, 400, 300 };
            DrawRectangleRounded(panel, 0.2f, 10, Fade(WHITE, 0.85f));
            DrawRectangleLinesEx(panel, 2, BLACK);

            DrawText("SETTINGS", sw/2 - 70, sh/2 - 120, 36, DARKBLUE);

            // ---- 背景音乐滑块 ----
            DrawText("Music Volume", sw/2 - 100, sh/2 - 50, 20, DARKGRAY);
            bgmSlider = { sw/2.0f - 80, sh/2.0f - 20, 160, 20 };   // 更新成员变量矩形
            DrawRectangleRec(bgmSlider, LIGHTGRAY);
            float bgmWidth = bgmSlider.width * bgmVolume;
            DrawRectangle(bgmSlider.x, bgmSlider.y, bgmWidth, bgmSlider.height, BLUE);
            DrawText(TextFormat("%d%%", (int)(bgmVolume * 100)), 
                    bgmSlider.x + bgmSlider.width + 10, bgmSlider.y - 2, 18, BLACK);

            // ---- 音效滑块 ----
            DrawText("SFX Volume", sw/2 - 100, sh/2 + 20, 20, DARKGRAY);
            sfxSlider = { sw/2.0f - 80, sh/2.0f + 50, 160, 20 };   // 更新成员变量矩形
            DrawRectangleRec(sfxSlider, LIGHTGRAY);
            float sfxWidth = sfxSlider.width * sfxVolume;
            DrawRectangle(sfxSlider.x, sfxSlider.y, sfxWidth, sfxSlider.height, GREEN);
            DrawText(TextFormat("%d%%", (int)(sfxVolume * 100)), 
                    sfxSlider.x + sfxSlider.width + 10, sfxSlider.y - 2, 18, BLACK);

            // ---- 保存按钮 ----
            Rectangle saveBtn = { sw/2.0f - 40, sh/2.0f + 100, 80, 40 };
            bool hoverSave = CheckCollisionPointRec(GetMousePosition(), saveBtn);
            DrawRectangleRounded(saveBtn, 0.2f, 8, hoverSave ? DARKGREEN : GREEN);
            DrawRectangleLinesEx(saveBtn, 2, BLACK);
            DrawText("SAVE", saveBtn.x + 15, saveBtn.y + 10, 20, WHITE);

            // ---- 返回按钮 ----
            Rectangle backBtn = { sw/2.0f - 40, (float)sh - 60, 80, 40 };
            bool hoverBack = CheckCollisionPointRec(GetMousePosition(), backBtn);
            DrawRectangleRounded(backBtn, 0.2f, 8, hoverBack ? DARKGRAY : GRAY);
            DrawRectangleLinesEx(backBtn, 2, BLACK);
            DrawText("BACK", backBtn.x + 15, backBtn.y + 10, 20, WHITE);

            break;
        }
        default: break;
        }
    if (editingMode)
    {
        DrawEditModeUI();
    }
    
    int fps = GetFPS();
    float frameTime = GetFrameTime() * 1000.0f; // 毫秒
    DrawText(TextFormat("FPS: %d", fps), GetScreenWidth() - 150, 10, 20, YELLOW);
    DrawText(TextFormat("Frame: %.2f ms", frameTime), GetScreenWidth() - 200, 30, 20, YELLOW);
        // 性能模块耗时（毫秒）
    DrawText(TextFormat("Phys: %.2fms", m_physicsTime * 1000), GetScreenWidth() - 150, 50, 18, GREEN);
    DrawText(TextFormat("Col:  %.2fms", m_collisionTime * 1000), GetScreenWidth() - 150, 65, 18, GREEN);
    DrawText(TextFormat("SP:   %.2fms", m_skillParticleTime * 1000), GetScreenWidth() - 150, 80, 18, GREEN);
    DrawText(TextFormat("Eff:  %.2fms", m_effectsTime * 1000), GetScreenWidth() - 150, 95, 18, GREEN);
    DrawText(TextFormat("Total:%.2fms", m_totalTime * 1000), GetScreenWidth() - 150, 110, 18, YELLOW);
    
    if (fading || fadeAlpha > 0.0f) {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, fadeAlpha));
    }

}

void Game::LoadLevel(int index, bool resetHearts) {
    if (index >= levelManager.GetLevelCount()) {
        currentState = GameState::VICTORY;
        timerRunning = false;
        AddVictoryRecord();
        return;
    }
    currentLevel = index;

    // 只有需要重置时才从配置读取初始生命值
    if (resetHearts) {
        hearts = config["game"].value("initial_hearts", 3);
    }

    float brickWidth, startX;
    levelManager.LoadLevel(index, bricks, brickWidth, startX,
                           GetScreenWidth(), GetScreenHeight());

    // 清空球和拖尾，重新添加一个球
    balls.clear();
    ballTrails.clear();
    Vector2 initPos = { (float)config["ball"]["init_x"], (float)config["ball"]["init_y"] };
    Vector2 initSpeed = { 0.0f, 0.0f };
    float initRadius = config["ball"]["radius"];
    balls.emplace_back(initPos, initSpeed, initRadius);
    ballTrails.emplace_back(maxTrailLength);

    skillBalls.clear();
    particleSystem.Clear();

    skillBalls.reserve(20);
    balls.reserve(8);

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

    brickGrid.Build(bricks);
    activeBrickCount = 0;
    for (const auto& b : bricks) {
        if (b.IsActive()) activeBrickCount++;
    }
}


void Game::CheckLevelTransition() {
    if (activeBrickCount == 0 && currentState == GameState::PLAYING) {
        SaveGame();   // 保存当前进度（本关已完成）
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
            // ★ 胜利后删除存档，避免继续游戏
            std::remove(saveFileName.c_str());
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
    DeleteSaveFile();
    ResetGameState();
    currentState = GameState::PLAYING;
    for (auto& b : balls) {
        b.SetSpeed({ config["ball"]["speed_x"], config["ball"]["speed_y"] });
    }
    timerRunning = true;
    // 启动淡入效果：从全黑渐隐到透明
    fadeAlpha = 1.0f;
    fading = true;
}

void Game::ToggleEditMode() {
    editingMode = !editingMode;
    if (editingMode) {
        // 进入编辑模式时，暂停游戏（如果正在游戏中）
        if (currentState == GameState::PLAYING) {
            currentState = GameState::PAUSED;
            timerRunning = false;
        }
        // 可选：保存一份原始砖块备份，以便取消编辑时恢复
    } else {
        // 退出编辑模式，恢复游戏状态（如果是暂停状态）
        if (currentState == GameState::PAUSED && pauseCause == PauseCause::MANUAL_PAUSE) {
            currentState = GameState::PLAYING;
            timerRunning = true;
        }
    }
}

void Game::SaveCurrentLayoutToJSON() {
    const LevelConfig& cfg = levelManager.GetCurrentConfig();
    int rows = cfg.rows;
    int cols = cfg.cols;
    float brickWidth = (GetScreenWidth() - 2 * cfg.margin - (cols - 1) * cfg.spacing) / cols;
    float startX = (GetScreenWidth() - (cols * brickWidth + (cols - 1) * cfg.spacing)) / 2.0f;
    float brickHeight = cfg.brickHeight;
    float startY = cfg.startY;
    float spacing = cfg.spacing;
    
    // 构建 layout 二维数组（初始为 0）
    json layout = json::array();
    for (int r = 0; r < rows; ++r) {
        json row = json::array();
        for (int c = 0; c < cols; ++c) {
            row.push_back(0);
        }
        layout.push_back(row);
    }
    
    // 遍历现有砖块，填充 layout
    for (const auto& brick : bricks) {
        if (!brick.IsActive()) continue;
        Rectangle rect = brick.GetRectangle();
        int col = (int)((rect.x - startX) / (brickWidth + spacing));
        int row = (int)((rect.y - startY) / (brickHeight + spacing));
        if (col >= 0 && col < cols && row >= 0 && row < rows) {
            layout[row][col] = brick.GetMaxHealth();
        }
    }
    
    // 读取现有配置文件
    json fullConfig;
    std::ifstream inFile("config.json");
    if (inFile.is_open()) {
        inFile >> fullConfig;
        inFile.close();
    }
    
    if (!fullConfig.contains("levels") || !fullConfig["levels"].is_array()) {
        fullConfig["levels"] = json::array();
    }
    int levelIndex = currentLevel;
    if (levelIndex >= (int)fullConfig["levels"].size()) {
        json newLevel = {
            {"rows", rows},
            {"cols", cols},
            {"brick_height", brickHeight},
            {"spacing", spacing},
            {"start_y", startY},
            {"margin", cfg.margin},
            {"layout", layout}
        };
        fullConfig["levels"].push_back(newLevel);
    } else {
        fullConfig["levels"][levelIndex]["layout"] = layout;
        // 自动生成 health_map
        std::set<int> healthValues;
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                int val = layout[r][c];
                if (val > 0) healthValues.insert(val);
            }
        }
        json healthMap;
        for (int val : healthValues) {
            healthMap[std::to_string(val)] = val;
        }
        fullConfig["levels"][levelIndex]["health_map"] = healthMap;
    }
    
    std::ofstream outFile("config.json");
    if (outFile.is_open()) {
        outFile << fullConfig.dump(4);
        outFile.close();
        std::cout << "Layout saved to config.json for level " << currentLevel << std::endl;
    } else {
        std::cerr << "Failed to save config.json" << std::endl;
    }
}

void Game::DrawEditModeUI() {
    // 半透明遮罩
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.4f));
    
    const LevelConfig& cfg = levelManager.GetCurrentConfig();
    float brickWidth = (GetScreenWidth() - 2 * cfg.margin - (cfg.cols - 1) * cfg.spacing) / cfg.cols;
    float startX = (GetScreenWidth() - (cfg.cols * brickWidth + (cfg.cols - 1) * cfg.spacing)) / 2.0f;
    float brickHeight = cfg.brickHeight;
    float startY = cfg.startY;
    float spacing = cfg.spacing;
    
    // 垂直网格线
    for (int c = 0; c <= cfg.cols; ++c) {
        float x = startX + c * (brickWidth + spacing);
        DrawLine(x, 0, x, GetScreenHeight(), GRAY);
    }
    // 水平网格线
    for (int r = 0; r <= cfg.rows; ++r) {
        float y = startY + r * (brickHeight + spacing);
        DrawLine(0, y, GetScreenWidth(), y, GRAY);
    }
    
    // 右侧按钮区域（底部）
    int btnW = 120, btnH = 35;
    int btnStartX = GetScreenWidth() - btnW - 10;
    int btnStartY = GetScreenHeight() - 150;
    
    // 清空按钮
    Rectangle clearBtn = { (float)btnStartX, (float)btnStartY, (float)btnW, (float)btnH };
    bool hoverClear = CheckCollisionPointRec(GetMousePosition(), clearBtn);
    DrawRectangleRounded(clearBtn, 0.2f, 8, hoverClear ? RED : MAROON);
    DrawText("Clear All", clearBtn.x + 20, clearBtn.y + 8, 18, WHITE);
    
    // 填充按钮
    Rectangle fillBtn = { (float)btnStartX, (float)(btnStartY + btnH + 5), (float)btnW, (float)btnH };
    bool hoverFill = CheckCollisionPointRec(GetMousePosition(), fillBtn);
    DrawRectangleRounded(fillBtn, 0.2f, 8, hoverFill ? DARKGREEN : GREEN);
    DrawText("Fill All", fillBtn.x + 25, fillBtn.y + 8, 18, WHITE);
    
    // 随机按钮
    Rectangle randomBtn = { (float)btnStartX, (float)(btnStartY + (btnH + 5) * 2), (float)btnW, (float)btnH };
    bool hoverRandom = CheckCollisionPointRec(GetMousePosition(), randomBtn);
    DrawRectangleRounded(randomBtn, 0.2f, 8, hoverRandom ? DARKBLUE : BLUE);
    DrawText("Random", randomBtn.x + 25, randomBtn.y + 8, 18, WHITE);
    
    // 底部提示文字
    DrawText(TextFormat("Selected Type: %d (1-5)", selectedBrickType), 10, GetScreenHeight() - 80, 20, YELLOW);
    DrawText("Left Click: Add Brick | Right Click: Delete Brick | S: Save | E: Exit", 10, GetScreenHeight() - 50, 20, WHITE);
    DrawText("EDIT MODE", GetScreenWidth() - 150, 10, 30, RED);
}

void Game::DrawHUD() {
    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    // 顶部半透明条
    Rectangle topBar = { 0, 0, (float)sw, 55 };
    DrawRectangleRec(topBar, Fade(BLACK, 0.6f));
    
    // 分数
    DrawText(TextFormat("Score: %d", score), 15, 12, 24, YELLOW);
    
    // 生命（红心简单形状）
    int heartX = 180;
    for (int i = 0; i < hearts; ++i) {
        DrawCircle(heartX + i * 35, 27, 12, RED);
        DrawCircle(heartX + i * 35 - 3, 22, 4, MAROON);
        DrawCircle(heartX + i * 35 + 3, 22, 4, MAROON);
    }
    
    // 等级
    DrawText(TextFormat("Level %d", currentLevel + 1), sw - 150, 12, 22, SKYBLUE);
    
    // 游戏时间
    DrawText(TextFormat("Time: %.1f", gameTimer), sw - 150, 40, 18, LIGHTGRAY);
    
    // 死亡次数（小骷髅简单文字）
    DrawText(TextFormat("Deaths: %d", totalDeaths), sw - 250, 12, 18, GRAY);
    
    // 效果列表（右下角横向排列）
    if (!activeEffects.empty()) {
        int effectX = sw - 20;
        int effectY = sh - 40;
        for (auto it = activeEffects.rbegin(); it != activeEffects.rend(); ++it) {
            const auto& eff = *it;
            float remaining = eff->GetRemainingTime();
            const char* name = eff->GetName().c_str();
            int textW = MeasureText(TextFormat("%s %.1fs", name, remaining), 16);
            effectX -= (textW + 15);
            DrawRectangle(effectX, effectY - 20, textW + 10, 24, Fade(BLACK, 0.7f));
            DrawText(TextFormat("%s %.1fs", name, remaining), effectX + 5, effectY - 16, 16, GREEN);
        }
    }
}

void Game::DrawButton(Rectangle rect, const char* text, int fontSize, Color normal, Color hover, Color pressed, bool isHover, bool isPressed) {
    Color drawColor = normal;
    if (isPressed) drawColor = pressed;
    else if (isHover) drawColor = hover;
    
    DrawRoundedRect(rect, 10.0f, drawColor);
    // 绘制边框
    DrawRectangleLinesEx(rect, 2, BLACK);
    
    int textWidth = MeasureText(text, fontSize);
    float textX = rect.x + (rect.width - textWidth) / 2;
    float textY = rect.y + (rect.height - fontSize) / 2;
    DrawText(text, textX, textY, fontSize, BLACK);
}

void Game::DrawRoundedRect(Rectangle rect, float radius, Color color) {
    // 直接调用文件顶部的静态辅助函数
    ::DrawRoundedRect(rect, radius, color);
}

void Game::SaveVolumeSettings() {
    json volJson;
    volJson["bgm_volume"] = bgmVolume;
    volJson["sfx_volume"] = sfxVolume;
    std::ofstream file("volume.json");
    if (file.is_open()) {
        file << volJson.dump(4);
    }
}

void Game::LoadVolumeSettings() {
    std::ifstream file("volume.json");
    if (file.is_open()) {
        json volJson;
        file >> volJson;
        bgmVolume = volJson.value("bgm_volume", 0.5f);
        sfxVolume = volJson.value("sfx_volume", 0.5f);
    } else {
        bgmVolume = 0.5f;
        sfxVolume = 0.5f;
    }
    // 应用音量（需要获取外部音乐和音效的引用，这里假设在 main 中有 bgm 变量）
    // 实际使用时需要在 main 中提供全局或通过回调设置，简单起见先存储值。
}
void Game::SetBGM(Music music) {
    bgmMusic = music;
    bgmMusicLoaded = (bgmMusic.stream.buffer != nullptr);
    // 初始化时应用当前音量
    ApplyVolume();
}

void Game::ApplyVolume() {
    // 应用背景音乐音量
    if (bgmMusicLoaded) {
        SetMusicVolume(bgmMusic, bgmVolume);
    }
    // 应用所有音效音量
    soundManager.SetMasterVolume(sfxVolume);
}

void Game::HandleEditModeInput() {
    Vector2 mousePos = GetMousePosition();
    
    // 切换砖块类型（数字键 1-5）
    if (IsKeyPressed(KEY_ONE)) selectedBrickType = 1;
    if (IsKeyPressed(KEY_TWO)) selectedBrickType = 2;
    if (IsKeyPressed(KEY_THREE)) selectedBrickType = 3;
    if (IsKeyPressed(KEY_FOUR)) selectedBrickType = 4;
    if (IsKeyPressed(KEY_FIVE)) selectedBrickType = 5;
    
    // 获取当前关卡的固定配置
    const LevelConfig& cfg = levelManager.GetCurrentConfig();
    float brickWidth = (GetScreenWidth() - 2 * cfg.margin - (cfg.cols - 1) * cfg.spacing) / cfg.cols;
    float startX = (GetScreenWidth() - (cfg.cols * brickWidth + (cfg.cols - 1) * cfg.spacing)) / 2.0f;
    float brickHeight = cfg.brickHeight;
    float startY = cfg.startY;
    float spacing = cfg.spacing;
    
    // 添加砖块（左键）
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        int col = (int)((mousePos.x - startX) / (brickWidth + spacing));
        int row = (int)((mousePos.y - startY) / (brickHeight + spacing));
        if (col >= 0 && col < cfg.cols && row >= 0 && row < cfg.rows) {
            Rectangle targetRect = {
                startX + col * (brickWidth + spacing),
                startY + row * (brickHeight + spacing),
                brickWidth,
                brickHeight
            };
            bool exists = false;
            for (const auto& brick : bricks) {
                if (brick.IsActive() && CheckCollisionRecs(brick.GetRectangle(), targetRect)) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                bricks.emplace_back(targetRect.x, targetRect.y, brickWidth, brickHeight, selectedBrickType);
                brickGrid.Build(bricks);
                activeBrickCount++;
            }
        }
    }
    
    // 删除砖块（右键）
    if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
        for (auto it = bricks.begin(); it != bricks.end(); ) {
            if (it->IsActive() && CheckCollisionPointRec(mousePos, it->GetRectangle())) {
                it = bricks.erase(it);
                activeBrickCount--;
            } else {
                ++it;
            }
        }
        brickGrid.Build(bricks);
    }
    
    // 保存布局（按 S 键）
    if (IsKeyPressed(KEY_S)) {
        SaveCurrentLayoutToJSON();
        showEraseHint = true;
        eraseHintTimer = 2.0f;
    }
    
    // 功能按钮：清空、填充、随机
    int btnW = 120, btnH = 35;
    int btnStartX = GetScreenWidth() - btnW - 10;
    int btnStartY = GetScreenHeight() - 150;
    Rectangle clearBtn = { (float)btnStartX, (float)btnStartY, (float)btnW, (float)btnH };
    Rectangle fillBtn  = { (float)btnStartX, (float)(btnStartY + btnH + 5), (float)btnW, (float)btnH };
    Rectangle randomBtn = { (float)btnStartX, (float)(btnStartY + (btnH + 5) * 2), (float)btnW, (float)btnH };
    
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, clearBtn)) {
        bricks.clear();
        brickGrid.Build(bricks);
        activeBrickCount = 0;
        showEraseHint = true;
        eraseHintTimer = 2.0f;
    }
    
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, fillBtn)) {
        bricks.clear();
        for (int r = 0; r < cfg.rows; ++r) {
            for (int c = 0; c < cfg.cols; ++c) {
                float x = startX + c * (brickWidth + spacing);
                float y = startY + r * (brickHeight + spacing);
                bricks.emplace_back(x, y, brickWidth, brickHeight, selectedBrickType);
            }
        }
        brickGrid.Build(bricks);
        activeBrickCount = bricks.size();
        showEraseHint = true;
        eraseHintTimer = 2.0f;
    }
    
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, randomBtn)) {
        // 重新生成砖块
        float brickWidthOut, startXOut;
        levelManager.LoadLevel(currentLevel, bricks, brickWidthOut, startXOut,
                               GetScreenWidth(), GetScreenHeight());
        brickGrid.Build(bricks);
        activeBrickCount = 0;
        for (const auto& b : bricks) if (b.IsActive()) activeBrickCount++;
        showEraseHint = true;
        eraseHintTimer = 2.0f;
    }
}