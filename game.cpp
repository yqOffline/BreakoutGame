#include "game.h"
#include <iostream>

// ======================== 构造函数 ========================
Game::Game(int screenWidth, int screenHeight)
    : ball({ 0,0 }, { 0,0 }, 0),
      paddle(0,0,0,0),
      score(0),
      hearts(3),
      brickRowColors({ RED, ORANGE, YELLOW, GREEN, BLUE }),
      currentState(GameState::MENU)  // 初始状态：菜单
{
    // 加载配置
    std::ifstream f("config.json");
    f >> config;
    f.close();

    // 读取配置
    int sw = config["screen"]["width"];
    int sh = config["screen"]["height"];
    int ballX = config["ball"]["init_x"];
    int ballY = config["ball"]["init_y"];
    float ballR = config["ball"]["radius"];
    float speedX = config["ball"]["speed_x"];
    float speedY = config["ball"]["speed_y"];

    float padW = config["paddle"]["width"];
    float padH = config["paddle"]["height"];
    float padY = config["paddle"]["y_pos"];
    paddleMoveSpeed = config["paddle"]["speed"];

    // 初始化对象
    ball = Ball({ (float)ballX, (float)ballY }, { 0,0 }, ballR);
    paddle = Paddle((sw - padW) / 2, padY, padW, padH);

    // 砖块配置
    brickRows = config["bricks"]["rows"];
    brickCols = config["bricks"]["cols"];
    brickWidth = config["bricks"]["width"];
    brickHeight = config["bricks"]["height"];
    brickSpacing = config["bricks"]["spacing"];
    brickStartY = config["bricks"]["start_y"];
    hearts = config["game"]["initial_hearts"];

    // UI 按钮
    redLine = {
        0.0f, paddle.GetRectangle().y + paddle.GetRectangle().height + 5.0f,
        (float)sw, 3.0f
    };
    startBtn = { (float)sw / 2 - 60, (float)sh / 2 - 25, 120, 50 };
    continueBtn = { (float)sw / 2 - 100, (float)sh / 2 - 25, 100, 50 };
    restartBtn = { (float)sw / 2 + 20, (float)sh / 2 - 25, 100, 50 };
    gameOverRestartBtn = { (float)sw / 2 - 60, (float)sh / 2 + 40, 120, 50 };

    // 加载纹理
    backgroundTex = LoadTexture("1.png");
    paddleTex = LoadTexture("2.png");
    bgLoaded = (backgroundTex.id != 0);
    paddleLoaded = (paddleTex.id != 0);

    //初始化暂停原因
    this->pauseCause = PauseCause::MANUAL_PAUSE;

    ResetBricks();
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
    ball.SetSpeed({ 0,0 });
    score = 0;
    hearts = config["game"]["initial_hearts"];
    currentState = GameState::MENU;  // 回到菜单
    ResetBricks();
}

// ======================== 重置砖块 ========================
void Game::ResetBricks() {
    bricks.clear();
    float totalW = brickCols * brickWidth + (brickCols - 1) * brickSpacing;
    float startX = (GetScreenWidth() - totalW) / 2.0f;

    for (int r = 0; r < brickRows; r++) {
        Color c = brickRowColors[r % brickRowColors.size()];
        for (int col = 0; col < brickCols; col++) {
            float x = startX + col * (brickWidth + brickSpacing);
            float y = brickStartY + r * (brickHeight + brickSpacing);
            bricks.emplace_back(x, y, brickWidth, brickHeight, c);
        }
    }
}

// ======================== 球碰红线扣血 ========================
void Game::CheckBallHitRedLine() {
    if (CheckCollisionCircleRec(ball.GetPosition(), ball.GetRadius(), redLine)) {
        hearts--;
        ball.SetSpeed({ 0,0 });

        if (hearts <= 0) {
            currentState = GameState::GAME_OVER;  // 状态切换：游戏结束
        } else {
            currentState = GameState::PAUSED;     // 状态切换：暂停
            pauseCause = PauseCause::LIFE_LOSS_PAUSE;
        }
    }
}

// ======================== 胜利检测 ========================
void Game::CheckGameVictory() {
    if (score >= brickRows * brickCols) {
        currentState = GameState::GAME_OVER;
        ball.SetSpeed({ 0,0 });
    }
}

// ======================== 【状态机】输入处理 ========================
void Game::HandleInput(Vector2 mousePos) {
    switch (currentState) {
        case GameState::MENU:
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, startBtn)||IsKeyPressed(KEY_SPACE)) {
                currentState = GameState::PLAYING;
                ball.SetSpeed({ config["ball"]["speed_x"], config["ball"]["speed_y"] });
            }
            break;

        case GameState::PLAYING:
            // space 键 → 暂停
            if (IsKeyPressed(KEY_SPACE)) {
                currentState = GameState::PAUSED;
                pauseCause = PauseCause::MANUAL_PAUSE;
            }
            break;

        case GameState::PAUSED:
            // space → 继续游戏
            if (IsKeyPressed(KEY_SPACE)||IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, continueBtn)) {
                currentState = GameState::PLAYING;
            }
            // 点击继续
            
            if ((IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, continueBtn) || IsKeyPressed(KEY_SPACE)) && pauseCause == PauseCause::LIFE_LOSS_PAUSE) {
            // 把球放到挡板正上方（远离红线，避免二次碰撞）
                float paddleCenterX = paddle.GetRectangle().x + paddle.GetRectangle().width / 2;
                float paddleTopY = paddle.GetRectangle().y - ball.GetRadius() - 2;
                ball.SetPosition({ paddleCenterX, paddleTopY });
                // 设置球初始速度
                ball.SetSpeed({ 2, -5 });
                // 恢复游戏
                currentState = GameState::PLAYING;
            }
            // 点击重启
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, restartBtn)|| IsKeyPressed(KEY_R)) {
                ResetGame();
            }
            break;

        case GameState::GAME_OVER:
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, gameOverRestartBtn)|| IsKeyPressed(KEY_SPACE)) {
                ResetGame();
            }
            break;
    }
}

// ======================== 【状态机】逻辑更新 ========================
void Game::Update(float dt) {
    // 只有 PLAYING 状态才更新游戏逻辑
    if (currentState != GameState::PLAYING) return;

    ball.Move();
    ball.BounceEdge(GetScreenWidth(), GetScreenHeight());
    ball.CheckCollisionPaddle(paddle);
    ball.CheckCollisionBricks(bricks, score);

    // 挡板移动
    if (IsKeyDown(KEY_LEFT)) paddle.MoveLeft(paddleMoveSpeed);
    if (IsKeyDown(KEY_RIGHT)) paddle.MoveRight(paddleMoveSpeed);

    CheckBallHitRedLine();
    CheckGameVictory();
}

// ======================== 【状态机】绘制 ========================
void Game::Draw() {
    // 背景
    if (bgLoaded) {
        DrawTexturePro(backgroundTex,
            { 0,0,(float)backgroundTex.width,(float)backgroundTex.height },
            { 0,0,(float)GetScreenWidth(),(float)GetScreenHeight() },
            { 0,0 }, 0, WHITE);
    } else {
        ClearBackground(RAYWHITE);
    }

    // 墙体
    DrawRectangle(0, 0, 5, GetScreenHeight(), GRAY);
    DrawRectangle(GetScreenWidth() - 5, 0, 5, GetScreenHeight(), GRAY);
    DrawRectangle(0, 0, GetScreenWidth(), 5, GRAY);
    DrawRectangle(0, GetScreenHeight() - 5, GetScreenWidth(), 5, GRAY);

    // 游戏对象
    ball.Draw();
    paddleLoaded ? DrawTexturePro(paddleTex, { 0,0,(float)paddleTex.width,(float)paddleTex.height },
        paddle.GetRectangle(), { 0,0 }, 0, WHITE) : paddle.Draw();
    for (auto& b : bricks) b.Draw();

    // 红线
    if (currentState == GameState::PLAYING) DrawRectangleRec(redLine, RED);

    // UI 文字
    DrawText(TextFormat("SCORE: %d", score), 10, 10, 20, BLUE);
    DrawText(TextFormat("LIVES: %d", hearts), 10, 40, 20, RED);

    // ======================== 按状态绘制UI ========================
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
            DrawText("PAUSED", GetScreenWidth()/2 - 60, GetScreenHeight()/2 - 80, 40, WHITE);
            break;

        case GameState::GAME_OVER:
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.8f));
            DrawText("GAME OVER", GetScreenWidth()/2 - 100, GetScreenHeight()/2 - 40, 50, RED);
            DrawRectangleRec(gameOverRestartBtn, CheckCollisionPointRec(GetMousePosition(), gameOverRestartBtn) ? DARKGREEN : GREEN);
            DrawText("PLAY AGAIN", gameOverRestartBtn.x + 10, gameOverRestartBtn.y + 15, 20, BLACK);
            break;

        default: break;
    }
}

// ======================== 游戏运行判断 ========================
bool Game::IsGameRunning() const {
    return !WindowShouldClose();
}