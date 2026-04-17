#include "game.h"
#include <fstream>
#include <iostream>
#include <cmath>          // 用于 sqrtf
#include <algorithm>      // 用于 std::remove_if

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

    // 2. 用配置值重新设置 balls（多球支持）
    float bx = config["ball"]["init_x"];
    float by = config["ball"]["init_y"];
    float br = config["ball"]["radius"];
    float spx = config["ball"]["speed_x"];
    float spy = config["ball"]["speed_y"];
    balls.emplace_back(Vector2{bx, by}, Vector2{spx, spy}, br);
    ballTrails.emplace_back();  // 对应第一个球的拖尾队列

    // 3. 用配置值重新设置 paddle
    float pw = config["paddle"]["width"];
    float ph = config["paddle"]["height"];
    float px = (screenWidth - pw) / 2.0f;
    float py = config["paddle"]["y_pos"];
    paddle = Paddle(px, py, pw, ph);
    paddleMoveSpeed = config["paddle"]["speed"];

    // 4. 初始化 LevelManager（延迟加载配置）
    levelManager.LoadConfig(config);

    // 5. 读取其他配置项
    skillDropChance = config["skill_ball"].value("drop_chance", 0.3f);
    skillBallSpeedY = config["skill_ball"].value("speed_y", 45.0f);
    skillBallRadius = config["skill_ball"].value("radius", 6.0f);
    buffDuration = config["skill_ball"].value("buff_duration", 5.0f);
    paddleExtendFactor = config["skill_ball"].value("paddle_extend_factor", 1.5f);
    ballEnlargeFactor = config["skill_ball"].value("ball_enlarge_factor", 1.5f);
    ballShrinkFactor = config["skill_ball"].value("ball_shrink_factor", 0.7f);
    
    particlesPerBrick = config["particles"].value("count_per_brick", 12);
    particleGravity = config["particles"].value("gravity", 300.0f);
    
    maxTrailLength = config["trail"].value("max_length", 10);
    
    totalLevels = levelManager.GetLevelCount();
    
    // 记录原始尺寸用于Buff恢复
    originalPaddleWidth = paddle.GetWidth();
    originalBallRadius = balls[0].GetRadius();  // 多球系统：以第一个球半径为准（所有球半径应一致）

    // 6. 加载第一关
    LoadLevel(0);

    // 7. UI 按钮初始化
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

    // 8. 加载纹理
    backgroundTex = LoadTexture("1.png");
    paddleTex = LoadTexture("2.png");
    bgLoaded = (backgroundTex.id != 0);
    paddleLoaded = (paddleTex.id != 0);

    // 9. 初始化暂停原因
    pauseCause = PauseCause::MANUAL_PAUSE;

    // 10. 最后一块砖动画相关变量初始化
    lastBrickAnimating = false;
    lastBrickIndex = -1;
    lastBrickAnimTimer = 0.0f;
}

// ======================== 析构函数 ========================
Game::~Game() {
    UnloadTexture(backgroundTex);
    UnloadTexture(paddleTex);
}

// ======================== 重置游戏（状态机专用） ========================
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
            if (activeBuffType == SkillType::INVINCIBLE && buffActive) {
                Vector2 sp = ball.GetSpeed();
                sp.y *= -1;
                ball.SetSpeed(sp);
                ball.SetPosition({ ball.GetPosition().x, redLine.y - ball.GetRadius() });
                continue;
            }
            // 普通情况：标记移除
            ballsToRemove.push_back(i);
            particleSystem.EmitExplosion(ball.GetPosition(), RED, 15);
        }
    }

    if (ballsToRemove.empty()) return;

    // 从后往前删除
    for (auto it = ballsToRemove.rbegin(); it != ballsToRemove.rend(); ++it) {
        balls.erase(balls.begin() + *it);
        ballTrails.erase(ballTrails.begin() + *it);
    }

    // 所有球都消失，扣血并暂停/结束
    if (balls.empty()) {
        hearts--;
        if (hearts <= 0) {
            currentState = GameState::GAME_OVER;
        } else {
            currentState = GameState::PAUSED;
            pauseCause = PauseCause::LIFE_LOSS_PAUSE;
            // 生成新球准备下一回合
            float paddleCenterX = paddle.GetRectangle().x + paddle.GetRectangle().width / 2;
            float paddleTopY = paddle.GetRectangle().y - originalBallRadius - 2;
            balls.emplace_back(Vector2{paddleCenterX, paddleTopY},
                              Vector2{config["ball"]["speed_x"], config["ball"]["speed_y"]},
                              originalBallRadius);
            ballTrails.emplace_back();
        }
    }
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
                // 生命损失后的继续，球已在 CheckBallHitRedLine 中重新生成，只需恢复速度
                for (auto& b : balls) b.SetSpeed({ config["ball"]["speed_x"], config["ball"]["speed_y"] });
                currentState = GameState::PLAYING;
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
                // 弹性碰撞（质量相同）
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
                // 分离避免卡住
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
                if (activeBuffType == SkillType::EXPLOSION && buffActive) {
                    damage = 2;
                    // 爆炸效果：对相邻砖块造成1点伤害（简化处理，仅对上下左右相邻且存在的砖块）
                    // 实际实现需要找到相邻砖块，此处省略具体索引查找，仅作示意
                }
                // 执行伤害
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
                        SkillType type = static_cast<SkillType>(GetRandomValue(0, 5)); // 0-5 共6种
                        skillBalls.emplace_back(spawnPos, type, skillBallRadius, Vector2{0, skillBallSpeedY});
                    }
                }
                // 反弹
                Vector2 sp = ball.GetSpeed();
                sp.y *= -1;
                ball.SetSpeed(sp);
                // 位置修正
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
            ApplySkillEffect(sb.skillType);
            sb.active = false;
        }
    }
    skillBalls.erase(std::remove_if(skillBalls.begin(), skillBalls.end(),
        [](const SkillBall& sb) { return !sb.active; }), skillBalls.end());

    // 6. 粒子更新
    particleSystem.Update(dt, particleGravity);

    // 7. Buff更新
    UpdateBuffs(dt);

    // 8. 挡板移动
    if (IsKeyDown(KEY_LEFT)) paddle.MoveLeft(paddleMoveSpeed);
    if (IsKeyDown(KEY_RIGHT)) paddle.MoveRight(paddleMoveSpeed);

    // 9. 最后一块砖特殊动画
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
        float newH = lastBrickStartRect.height; // 高度不变
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
    // 显示当前关卡
    DrawText(TextFormat("LEVEL: %d", currentLevel + 1), 10, 70, 20, DARKGREEN);

    // 显示激活的 Buff 信息
    if (buffActive) {
        const char* effectName = "";
        Color effectColor = WHITE;
        switch (activeBuffType) {
            case SkillType::PADDLE_EXTEND: effectName = "Paddle Extend"; effectColor = BLUE; break;
            case SkillType::BALL_ENLARGE:  effectName = "Ball Enlarge"; effectColor = GREEN; break;
            case SkillType::BALL_SHRINK:   effectName = "Ball Shrink"; effectColor = RED; break;
            case SkillType::EXPLOSION:     effectName = "Explosion"; effectColor = ORANGE; break;
            case SkillType::INVINCIBLE:    effectName = "Invincible"; effectColor = GOLD; break;
            default: break;
        }
        DrawText(TextFormat("%s: %.1fs", effectName, buffTimer), 10, 100, 20, effectColor);
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

    // 每关开始时重置心数
    hearts = config["game"].value("initial_hearts", 10);

    float brickWidth, startX;
    levelManager.LoadLevel(index, bricks, brickWidth, startX,
                           GetScreenWidth(), GetScreenHeight());

    // 重置多球
    balls.clear();
    ballTrails.clear();
    Vector2 initPos = { (float)config["ball"]["init_x"], (float)config["ball"]["init_y"] };
    Vector2 initSpeed = { 0.0f, 0.0f };
    float initRadius = config["ball"]["radius"];
    balls.emplace_back(initPos, initSpeed, initRadius);
    ballTrails.emplace_back();

    skillBalls.clear();
    particleSystem.Clear();

    // 重置最后一块砖动画
    lastBrickAnimating = false;
    lastBrickIndex = -1;
    lastBrickAnimTimer = 0.0f;

    if (buffActive) {
        paddle.SetWidth(originalPaddleWidth);
        buffActive = false;
        buffTimer = 0.0f;
    }
    // 确保挡板宽度恢复
    paddle.SetWidth(originalPaddleWidth);
}

// ======================== 应用技能效果 ========================
void Game::ApplySkillEffect(SkillType type) {
    if (buffActive && type != SkillType::SPLIT) {
        paddle.SetWidth(originalPaddleWidth);
        // 球的半径恢复：因为多球，需要遍历所有球恢复半径
        for (auto& b : balls) b.SetRadius(originalBallRadius);
        buffActive = false;
        buffTimer = 0.0f;
    }

    switch (type) {
        case SkillType::PADDLE_EXTEND:
            paddle.SetWidth(originalPaddleWidth * paddleExtendFactor);
            buffActive = true;
            activeBuffType = type;
            buffTimer = buffDuration;
            break;
        case SkillType::BALL_ENLARGE:
            for (auto& b : balls) b.SetRadius(originalBallRadius * ballEnlargeFactor);
            buffActive = true;
            activeBuffType = type;
            buffTimer = buffDuration;
            break;
        case SkillType::BALL_SHRINK:
            for (auto& b : balls) b.SetRadius(originalBallRadius * ballShrinkFactor);
            buffActive = true;
            activeBuffType = type;
            buffTimer = buffDuration;
            break;
        case SkillType::EXPLOSION:
        case SkillType::INVINCIBLE:
            buffActive = true;
            activeBuffType = type;
            buffTimer = buffDuration;
            break;
        case SkillType::SPLIT:
        {
            std::vector<Ball> newBalls;
            for (auto& ball : balls) {
                Vector2 pos = ball.GetPosition();
                Vector2 sp = ball.GetSpeed();
                float r = ball.GetRadius();
                Ball newBall(pos, Vector2{sp.x, -sp.y}, r);
                newBalls.push_back(newBall);
            }
            balls.insert(balls.end(), newBalls.begin(), newBalls.end());
            for (size_t i = 0; i < newBalls.size(); ++i)
                ballTrails.emplace_back();
            // 分裂不占用buff计时器
        }
        break;
        default: break;
    }
}

// ======================== 更新Buff ========================
void Game::UpdateBuffs(float dt) {
    if (!buffActive) return;
    buffTimer -= dt;
    if (buffTimer <= 0.0f) {
        paddle.SetWidth(originalPaddleWidth);
        for (auto& b : balls) b.SetRadius(originalBallRadius);
        buffActive = false;
    }
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
            currentState = GameState::GAME_OVER;
        }
    }
}

// ======================== 游戏运行判断 ========================
bool Game::IsGameRunning() const {
    return !WindowShouldClose();
}