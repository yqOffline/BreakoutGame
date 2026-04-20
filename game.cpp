#include "game.h"
#include "EffectFactory.h"
#include "SoundManager.h"
#include <fstream>
#include <iostream>
#include <cmath>
#include <algorithm>

// ======================== 构造函数 ========================
Game::Game(int screenWidth, int screenHeight)
    : score(0),
      hearts(3),
      currentState(GameState::MENU)
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

    // 7. 加载第一关
    LoadLevel(0);

    // 8. UI 按钮初始化
    redLine = {
        0.0f, paddle.GetRectangle().y + paddle.GetRectangle().height + 5.0f,
        (float)screenWidth, 3.0f
    };
    startBtn = { (float)screenWidth / 2 - 60, (float)screenHeight / 2 - 25, 120, 50 };
    continueBtn = { (float)screenWidth / 2 - 100, (float)screenHeight / 2 - 25, 100, 50 };
    restartBtn = { (float)screenWidth / 2 + 20, (float)screenHeight / 2 - 25, 100, 50 };
    gameOverRestartBtn = { (float)screenWidth / 2 - 60, (float)screenHeight / 2 + 40, 120, 50 };
    replayBtn = { (float)screenWidth / 2 - 160, (float)screenHeight / 2 + 20, 120, 50 };
    goAheadBtn = { (float)screenWidth / 2 + 40, (float)screenHeight / 2 + 20, 120, 50 };
    victoryRestartBtn = { (float)screenWidth / 2 -60,(float)screenHeight / 2 + 40,120,50};

    // 9. 加载纹理
    backgroundTex = LoadTexture("1.png");
    paddleTex = LoadTexture("2.png");
    bgLoaded = (backgroundTex.id != 0);
    paddleLoaded = (paddleTex.id != 0);

    pauseCause = PauseCause::MANUAL_PAUSE;
    lastBrickAnimating = false;
    lastBrickIndex = -1;
    lastBrickAnimTimer = 0.0f;
}

// ======================== 析构函数 ========================
Game::~Game() {
    UnloadTexture(backgroundTex);
    UnloadTexture(paddleTex);
}

// ======================== 重置游戏 ========================
void Game::ResetGame() {
    int bx = config["ball"]["init_x"];
    int by = config["ball"]["init_y"];
    balls.clear();
    ballTrails.clear();
    balls.emplace_back(Vector2{(float)bx, (float)by}, Vector2{0, 0}, config["ball"]["radius"]);
    ballTrails.emplace_back();
    score = 0;
    hearts = config["game"]["initial_hearts"];
    currentState = GameState::MENU;
    ResetBricks();
    activeEffect.reset();
}

// ======================== 重置砖块 ========================
void Game::ResetBricks() {
    LoadLevel(currentLevel);
}

// ======================== 球碰红线处理 ========================
void Game::CheckBallHitRedLine() {
    std::vector<size_t> ballsToRemove;

    for (size_t i = 0; i < balls.size(); ++i) {
        Ball& ball = balls[i];
        if (CheckCollisionCircleRec(ball.GetPosition(), ball.GetRadius(), redLine)) {
            // 无敌效果：反弹
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
        if (hearts <= 0) {
            currentState = GameState::GAME_OVER;
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

// ======================== 应用效果 ========================
void Game::ApplyEffect(std::unique_ptr<Effect> effect) {
    // 如果已有激活效果且新效果不是瞬时效果（SPLIT），先撤销旧效果
    if (activeEffect && effect->GetName() != "Split") {
        activeEffect->Revert(this);
    }
    activeEffect = std::move(effect);
    if (activeEffect) {
        activeEffect->Apply(this);
    }
}

// ======================== 更新效果计时 ========================
void Game::UpdateEffects(float dt) {
    if (!activeEffect) return;
    bool stillActive = activeEffect->Update(dt);
    if (!stillActive) {
        activeEffect->Revert(this);
        activeEffect.reset();
    }
}

// ======================== 检查是否有特定效果 ========================
bool Game::HasEffectOfType(const std::string& typeName) const {
    return activeEffect && activeEffect->GetName() == typeName;
}

// ======================== 【状态机】输入处理 ========================
void Game::HandleInput(Vector2 mousePos) {
    switch (currentState) {
        case GameState::MENU:
            if ((IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, startBtn)) || IsKeyPressed(KEY_SPACE)) {
                currentState = GameState::PLAYING;
                for (auto& b : balls) b.SetSpeed({ config["ball"]["speed_x"], config["ball"]["speed_y"] });
            }
            break;

        case GameState::PLAYING:
            if (IsKeyPressed(KEY_SPACE)) {
                currentState = GameState::PAUSED;
                pauseCause = PauseCause::MANUAL_PAUSE;
            }
            break;

        case GameState::PAUSED:
            if (IsKeyPressed(KEY_SPACE) || (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, continueBtn))) {
                currentState = GameState::PLAYING;
            }
            if ((IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, continueBtn) || IsKeyPressed(KEY_SPACE)) && pauseCause == PauseCause::LIFE_LOSS_PAUSE) {
                for (auto& b : balls) b.SetSpeed({ config["ball"]["speed_x"], config["ball"]["speed_y"] });
                currentState = GameState::PLAYING;
            }
            if ((IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, restartBtn)) || IsKeyPressed(KEY_R)) {
                ResetGame();
            }
            break;

        case GameState::GAME_OVER:
            //soundManager.PlayGameOver();
            if ((IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, gameOverRestartBtn)) || IsKeyPressed(KEY_SPACE)) {
                ResetGame();
            }
            break;

        case GameState::LEVEL_CLEAR:
            if (IsKeyPressed(KEY_P) || (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, replayBtn))) {
                LoadLevel(currentLevel);
                for (auto& b : balls) b.SetSpeed({ config["ball"]["speed_x"], config["ball"]["speed_y"] });
                currentState = GameState::PLAYING;
            }
            if (IsKeyPressed(KEY_G) || (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, goAheadBtn))) {
                currentLevel++;
                LoadLevel(currentLevel);
                for (auto& b : balls) b.SetSpeed({ config["ball"]["speed_x"], config["ball"]["speed_y"] });
                currentState = GameState::PLAYING;
            }
            break;
        case GameState::VICTORY:
            if ((IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, victoryRestartBtn)) || IsKeyPressed(KEY_SPACE)) {
                ResetGame();
            }
            break;        
    }
}

// ======================== 球间碰撞处理 ========================
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

// ======================== 【状态机】逻辑更新 ========================
void Game::Update(float dt) {
    if (currentState != GameState::PLAYING) return;

    //===================后门====================
    if (IsKeyPressed(KEY_L)) {
        // 直接通关当前关卡，进入 LEVEL_CLEAR 状态
        currentState = GameState::LEVEL_CLEAR;
        for (auto& b : balls) b.SetSpeed({0, 0});
        return;  // 跳过本帧其余更新
    }
    if (IsKeyPressed(KEY_W)) {
        // 直接游戏胜利，进入 VICTORY 状态
        currentState = GameState::VICTORY;
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
        ball.BounceEdge(GetScreenWidth(), GetScreenHeight());
        ball.CheckCollisionPaddle(paddle);
    }

    // 3. 砖块碰撞处理
    for (auto& ball : balls) {
        for (auto& brick : bricks) {
            if (brick.IsActive() && CheckCollisionCircleRec(ball.GetPosition(), ball.GetRadius(), brick.GetRectangle())) {
                int damage = 1;
                if (HasEffectOfType("Explosion")) {
                    damage = 2;
                    // TODO: 范围伤害可在此扩展
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
                        // 从配置获取光晕颜色（可以预先定义，这里简单映射）
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
            // 使用工厂创建效果并应用
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

    // 7. 效果更新（在效果结束时播放音效）
    if (activeEffect) {
        bool stillActive = activeEffect->Update(dt);
        if (!stillActive) {
            soundManager.PlayPowerupEnd();
            activeEffect->Revert(this);
            activeEffect.reset();
        }
    }

    // 8. 挡板移动
    if (IsKeyDown(KEY_LEFT)) paddle.MoveLeft(paddleMoveSpeed);
    if (IsKeyDown(KEY_RIGHT)) paddle.MoveRight(paddleMoveSpeed);

    // 9. 最后一块砖特殊动画（保持原有逻辑）
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

// ======================== 【状态机】绘制 ========================
void Game::Draw() {
    // 1. 背景
    if (bgLoaded) {
        DrawTexturePro(backgroundTex,
            { 0, 0, (float)backgroundTex.width, (float)backgroundTex.height },
            { 0, 0, (float)GetScreenWidth(), (float)GetScreenHeight() },
            { 0, 0 }, 0, WHITE);
    } else {
        ClearBackground(RAYWHITE);
    }

    // 2. 墙体
    DrawRectangle(0, 0, 5, GetScreenHeight(), GRAY);
    DrawRectangle(GetScreenWidth() - 5, 0, 5, GetScreenHeight(), GRAY);
    DrawRectangle(0, 0, GetScreenWidth(), 5, GRAY);
    DrawRectangle(0, GetScreenHeight() - 5, GetScreenWidth(), 5, GRAY);

    // 3. 拖尾
    for (size_t i = 0; i < balls.size(); ++i) {
        for (size_t j = 0; j < ballTrails[i].size(); ++j) {
            float alpha = 0.3f * (float)j / ballTrails[i].size();
            float size = balls[i].GetRadius() * (0.5f + 0.5f * j / ballTrails[i].size());
            DrawCircleV(ballTrails[i][j], size, Fade(RED, alpha));
        }
    }

    // 4. 主球
    for (auto& b : balls) b.Draw();

    // 5. 挡板
    if (paddleLoaded) {
        DrawTexturePro(paddleTex,
            { 0, 0, (float)paddleTex.width, (float)paddleTex.height },
            paddle.GetRectangle(), { 0, 0 }, 0, WHITE);
    } else {
        paddle.Draw();
    }

    // 6. 砖块
    for (auto& b : bricks) b.Draw();

    // 7. 技能球
    for (auto& sb : skillBalls) sb.Draw();

    // 8. 粒子
    particleSystem.Draw();

    // 9. 红线
    if (currentState == GameState::PLAYING) DrawRectangleRec(redLine, RED);

    // 10. UI文字
    DrawText(TextFormat("SCORE: %d", score), 10, 10, 20, BLUE);
    DrawText(TextFormat("LIVES: %d", hearts), 10, 40, 20, RED);
    DrawText(TextFormat("LEVEL: %d", currentLevel + 1), 10, 70, 20, DARKGREEN);

    // 显示激活的效果信息
    if (activeEffect) {
        DrawText(TextFormat("%s: %.1fs", activeEffect->GetName().c_str(), activeEffect->GetRemainingTime()), 10, 100, 20, GREEN);
    }

    // 11. 按状态绘制UI
    switch (currentState) {
        case GameState::MENU:
            DrawRectangleRec(startBtn, CheckCollisionPointRec(GetMousePosition(), startBtn) ? DARKGREEN : GREEN);
            DrawText("START GAME", startBtn.x + 12, startBtn.y + 15, 20, BLACK);
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
            DrawText("VICTORY!", GetScreenWidth() / 2 - 120, GetScreenHeight() / 2 - 40, 50, GOLD);
            DrawText(TextFormat("FINAL SCORE: %d", score), GetScreenWidth() / 2 - 100, GetScreenHeight() / 2 + 20, 30, WHITE);
            DrawRectangleRec(victoryRestartBtn, CheckCollisionPointRec(GetMousePosition(), victoryRestartBtn) ? DARKGREEN : GREEN);
            DrawText("PLAY AGAIN", victoryRestartBtn.x + 10, victoryRestartBtn.y + 15, 20, BLACK);
            break;        
        default: break;
    }
}

// ======================== 加载关卡 ========================
void Game::LoadLevel(int index) {
    if (index >= levelManager.GetLevelCount()) {
        currentState = GameState::GAME_OVER;
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

    // 清除激活效果
    if (activeEffect) {
        activeEffect->Revert(this);
        activeEffect.reset();
    }
    paddle.SetWidth(originalPaddleWidth);
}

// ======================== 检查关卡过渡 ========================
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
            for (auto& b : balls) b.SetSpeed({0, 0});
        } else {
            currentState = GameState::VICTORY;
            for (auto& b : balls) b.SetSpeed({0,0});
        }
    }
}

// ======================== 游戏运行判断 ========================
bool Game::IsGameRunning() const {
    return !WindowShouldClose();
}