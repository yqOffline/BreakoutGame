#include "game.h"
#include <fstream>
#include <iostream>

// ======================== 构造函数 ========================
Game::Game(int screenWidth, int screenHeight)
    : ball({0, 0}, {0, 0}, 10),        // 临时默认值
      paddle(0, 0, 100, 20),           // 临时默认值
      score(0),
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

    // 2. 用配置值重新设置 ball
    float bx = config["ball"]["init_x"];
    float by = config["ball"]["init_y"];
    float br = config["ball"]["radius"];
    float spx = config["ball"]["speed_x"];
    float spy = config["ball"]["speed_y"];
    ball.SetPosition({bx, by});
    ball.SetSpeed({spx, spy});
    ball.SetRadius(br);

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
    skillBallSpeedY = config["skill_ball"].value("speed_y", 3.0f);
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
    originalBallRadius = ball.GetRadius();

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

    // 8. 加载纹理
    backgroundTex = LoadTexture("1.png");
    paddleTex = LoadTexture("2.png");
    bgLoaded = (backgroundTex.id != 0);
    paddleLoaded = (paddleTex.id != 0);

    // 9. 初始化暂停原因
    pauseCause = PauseCause::MANUAL_PAUSE;
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
    ball.SetPosition({ (float)bx, (float)by });
    ball.SetSpeed({ 0, 0 });
    score = 0;
    hearts = config["game"]["initial_hearts"];
    currentState = GameState::MENU;
    ResetBricks();
}

// ======================== 重置砖块 ========================
void Game::ResetBricks() {
    LoadLevel(currentLevel);
}

// ======================== 球碰红线扣血 ========================
void Game::CheckBallHitRedLine() {
    if (CheckCollisionCircleRec(ball.GetPosition(), ball.GetRadius(), redLine)) {
        hearts--;
        ball.SetSpeed({ 0, 0 });

        if (hearts <= 0) {
            currentState = GameState::GAME_OVER;
        } else {
            currentState = GameState::PAUSED;
            pauseCause = PauseCause::LIFE_LOSS_PAUSE;
        }
    }
}

// ======================== 【状态机】输入处理 ========================
void Game::HandleInput(Vector2 mousePos) {
    switch (currentState) {
        case GameState::MENU:
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, startBtn) || IsKeyPressed(KEY_SPACE)) {
                currentState = GameState::PLAYING;
                ball.SetSpeed({ config["ball"]["speed_x"], config["ball"]["speed_y"] });
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
                float paddleCenterX = paddle.GetRectangle().x + paddle.GetRectangle().width / 2;
                float paddleTopY = paddle.GetRectangle().y - ball.GetRadius() - 2;
                ball.SetPosition({ paddleCenterX, paddleTopY });
                ball.SetSpeed({ 2, -5 });
                currentState = GameState::PLAYING;
            }
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, restartBtn) || IsKeyPressed(KEY_R)) {
                ResetGame();
            }
            break;

        case GameState::GAME_OVER:
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, gameOverRestartBtn) || IsKeyPressed(KEY_SPACE)) {
                ResetGame();
            }
            break;
    }
}

// ======================== 【状态机】逻辑更新 ========================
void Game::Update(float dt) {
    if (currentState != GameState::PLAYING) return;

    // 拖尾记录
    ballTrail.push_back(ball.GetPosition());
    if ((int)ballTrail.size() > maxTrailLength) ballTrail.pop_front();

    // 主球移动与碰撞
    ball.Move();
    ball.BounceEdge(GetScreenWidth(), GetScreenHeight());
    ball.CheckCollisionPaddle(paddle);

    // 砖块碰撞处理
    for (auto& brick : bricks) {
        if (brick.IsActive() && CheckCollisionCircleRec(ball.GetPosition(), ball.GetRadius(), brick.GetRectangle())) {
            bool destroyed = brick.TakeDamage();
            if (destroyed) {
                score++;
                particleSystem.EmitBrickBreak(brick.GetRectangle(), brick.GetColor(), particlesPerBrick);
                if (brick.ShouldDropSkill(skillDropChance)) {
                    Vector2 spawnPos = { brick.GetRectangle().x + brick.GetRectangle().width / 2,
                                         brick.GetRectangle().y + brick.GetRectangle().height / 2 };
                    SkillType type = static_cast<SkillType>(GetRandomValue(0, 2));
                    skillBalls.emplace_back(spawnPos, type, skillBallRadius,
                                            Vector2{0, skillBallSpeedY});
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

    // 技能球更新
    for (auto& sb : skillBalls) {
        sb.Update(dt);
        if (sb.active && CheckCollisionCircleRec(sb.GetPosition(), sb.GetRadius(), paddle.GetRectangle())) {
            ApplySkillEffect(sb.skillType);
            sb.active = false;
        }
    }
    skillBalls.erase(std::remove_if(skillBalls.begin(), skillBalls.end(),
        [](const SkillBall& sb) { return !sb.active; }), skillBalls.end());

    // 粒子更新
    particleSystem.Update(dt, particleGravity);

    // Buff更新
    UpdateBuffs(dt);

    // 挡板移动
    if (IsKeyDown(KEY_LEFT)) paddle.MoveLeft(paddleMoveSpeed);
    if (IsKeyDown(KEY_RIGHT)) paddle.MoveRight(paddleMoveSpeed);

    CheckBallHitRedLine();

    // 检查关卡过渡
    CheckLevelTransition();
}

// ======================== 【状态机】绘制 ========================
void Game::Draw() {
    // 1. 绘制背景
    if (bgLoaded) {
        DrawTexturePro(backgroundTex,
            { 0, 0, (float)backgroundTex.width, (float)backgroundTex.height },
            { 0, 0, (float)GetScreenWidth(), (float)GetScreenHeight() },
            { 0, 0 }, 0, WHITE);
    } else {
        ClearBackground(RAYWHITE);
    }

    // 2. 绘制墙体
    DrawRectangle(0, 0, 5, GetScreenHeight(), GRAY);
    DrawRectangle(GetScreenWidth() - 5, 0, 5, GetScreenHeight(), GRAY);
    DrawRectangle(0, 0, GetScreenWidth(), 5, GRAY);
    DrawRectangle(0, GetScreenHeight() - 5, GetScreenWidth(), 5, GRAY);

    // 3. 绘制拖尾
    for (size_t i = 0; i < ballTrail.size(); ++i) {
        float alpha = 0.3f * (float)i / ballTrail.size();
        float size = ball.GetRadius() * (0.5f + 0.5f * i / ballTrail.size());
        DrawCircleV(ballTrail[i], size, Fade(RED, alpha));
    }

    // 4. 绘制主球
    ball.Draw();

    // 5. 绘制挡板
    if (paddleLoaded) {
        DrawTexturePro(paddleTex,
            { 0, 0, (float)paddleTex.width, (float)paddleTex.height },
            paddle.GetRectangle(), { 0, 0 }, 0, WHITE);
    } else {
        paddle.Draw();
    }

    // 6. 绘制砖块
    for (auto& b : bricks) {
        b.Draw();
    }

    // 7. 绘制技能球
    for (auto& sb : skillBalls) {
        sb.Draw();
    }

    // 8. 绘制粒子
    particleSystem.Draw();

    // 9. 绘制红线
    if (currentState == GameState::PLAYING) {
        DrawRectangleRec(redLine, RED);
    }

    // 10. 绘制UI文字
    DrawText(TextFormat("SCORE: %d", score), 10, 10, 20, BLUE);
    DrawText(TextFormat("LIVES: %d", hearts), 10, 40, 20, RED);

    // 11. 按状态绘制UI按钮
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

    float brickWidth, startX;
    levelManager.LoadLevel(index, bricks, brickWidth, startX,
                           GetScreenWidth(), GetScreenHeight());

    ball.SetPosition({ (float)config["ball"]["init_x"], (float)config["ball"]["init_y"] });
    ball.SetSpeed({ 0, 0 });

    skillBalls.clear();
    particleSystem.Clear();
    ballTrail.clear();

    if (buffActive) {
        paddle.SetWidth(originalPaddleWidth);
        ball.SetRadius(originalBallRadius);
        buffActive = false;
        buffTimer = 0.0f;
    }
}

// ======================== 应用技能效果 ========================
void Game::ApplySkillEffect(SkillType type) {
    if (buffActive) {
        paddle.SetWidth(originalPaddleWidth);
        ball.SetRadius(originalBallRadius);
        buffActive = false;
        buffTimer = 0.0f;
    }

    activeBuffType = type;
    buffTimer = buffDuration;
    buffActive = true;

    switch (type) {
        case SkillType::PADDLE_EXTEND:
            paddle.SetWidth(originalPaddleWidth * paddleExtendFactor);
            break;
        case SkillType::BALL_ENLARGE:
            ball.SetRadius(originalBallRadius * ballEnlargeFactor);
            break;
        case SkillType::BALL_SHRINK:
            ball.SetRadius(originalBallRadius * ballShrinkFactor);
            break;
    }
}

// ======================== 更新Buff ========================
void Game::UpdateBuffs(float dt) {
    if (!buffActive) return;
    buffTimer -= dt;
    if (buffTimer <= 0.0f) {
        paddle.SetWidth(originalPaddleWidth);
        ball.SetRadius(originalBallRadius);
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
            currentLevel++;
            LoadLevel(currentLevel);
            ball.SetSpeed({ config["ball"]["speed_x"], config["ball"]["speed_y"] });
        } else {
            currentState = GameState::GAME_OVER;
        }
    }
}

// ======================== 游戏运行判断 ========================
bool Game::IsGameRunning() const {
    return !WindowShouldClose();
}