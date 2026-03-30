#include "game.h"
#include <iostream>

// 构造函数：初始化所有游戏数据
Game::Game(int screenWidth, int screenHeight)
    : ball({400, 300}, {0, 0}, 10),
      paddle(350, 550, 100, 20),
      score(0), hearts(3), gameStarted(false), state(START_SCREEN),
      brickRows(5), brickCols(5),
      brickWidth(70), brickHeight(25), brickSpacing(10), brickStartY(80),
      brickRowColors({RED, ORANGE, YELLOW, GREEN, BLUE}) {
    
    // 初始化 UI 元素
    redLine = {0.0f, paddle.GetRectangle().y + paddle.GetRectangle().height + 5.0f, static_cast<float>(screenWidth), 3.0f};
    startButton = { (float)screenWidth/2 - 50, (float)screenHeight/2 - 20, 100, 40 };
    continueButton = { (float)screenWidth/2 - 100, (float)screenHeight/2 - 20, 80, 40 };
    restartButton = { (float)screenWidth/2 + 20, (float)screenHeight/2 - 20, 80, 40 };
    gameOverRestartButton = { (float)screenWidth/2 - 50, (float)screenHeight/2 + 20, 100, 40 };
    victoryRestartButton = { (float)screenWidth/2 - 50, (float)screenHeight/2 + 20, 100, 40 };
    
    // 加载纹理
    backgroundTex = LoadTexture("1.png");
    paddleTex = LoadTexture("2.png");
    bgLoaded = (backgroundTex.id != 0);
    paddleLoaded = (paddleTex.id != 0);
    
    // 初始化砖块
    ResetBricks();
}

// 析构函数：释放资源
Game::~Game() {
    UnloadTexture(backgroundTex);
    UnloadTexture(paddleTex);
}

// 重置砖块（封装原 ResetGame 中的砖块逻辑）
void Game::ResetBricks() {
    bricks.clear();
    float totalWidth = brickCols * brickWidth + (brickCols - 1) * brickSpacing;
    float startX = (GetScreenWidth() - totalWidth) / 2.0f;
    for (int row = 0; row < brickRows; row++) {
        Color brickColor = brickRowColors[row % brickRowColors.size()];
        for (int col = 0; col < brickCols; col++) {
            float x = startX + col * (brickWidth + brickSpacing);
            float y = brickStartY + row * (brickHeight + brickSpacing);
            bricks.emplace_back(x, y, brickWidth, brickHeight, brickColor);
        }
    }
}

// 检测胜利条件（封装原 PLAYING 中的胜利判断）
void Game::CheckGameVictory() {
    if (score == brickRows * brickCols) { // 动态判断（不再写死25）
        state = VICTORY;
        ball.SetSpeed({0, 0});
    }
}

// 检测球碰到红线（封装原 PLAYING 中的扣血逻辑）
void Game::CheckBallHitRedLine() {
    if (CheckCollisionCircleRec(ball.GetPosition(), ball.GetRadius(), redLine)) {
        hearts--;
        ball.SetSpeed({0, 0});
        if (hearts > 0) {
            state = PAUSE_LOSE_HEART;
        } else {
            state = GAME_OVER;
        }
    }
}

// 重置游戏（公开方法）
void Game::Reset() {
    ball.SetPosition({400, 300});
    ball.SetSpeed({0, 0});
    score = 0;
    hearts = 3;
    gameStarted = false;
    state = START_SCREEN;
    ResetBricks();
}

// 游戏逻辑更新（每帧调用）
void Game::Update(float deltaTime) {
    switch (state) {
        case PLAYING: {
            // 游戏对象更新
            ball.Move();
            ball.BounceEdge(GetScreenWidth(), GetScreenHeight());
            ball.CheckCollisionPaddle(paddle);
            ball.CheckCollisionBricks(bricks, score);
            
            // 挡板移动
            if (IsKeyDown(KEY_LEFT)) paddle.MoveLeft(5);
            if (IsKeyDown(KEY_RIGHT)) paddle.MoveRight(5);
            
            // 检测扣血和胜利
            CheckBallHitRedLine();
            CheckGameVictory();
            break;
        }
        default:
            break; // 其他状态无更新逻辑
    }
}

// 处理输入（鼠标/键盘）
void Game::HandleInput(Vector2 mousePos) {
    switch (state) {
        case START_SCREEN: {
            bool startHover = CheckCollisionPointRec(mousePos, startButton);
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && startHover) {
                state = PLAYING;
                gameStarted = true;
                ball.SetSpeed({4, 5});
                ball.SetPosition({400, 300});
            }
            break;
        }
        case PAUSE_LOSE_HEART: {
            bool continueHover = CheckCollisionPointRec(mousePos, continueButton);
            bool restartHover = CheckCollisionPointRec(mousePos, restartButton);
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (continueHover) {
                    ball.SetPosition({paddle.GetRectangle().x + paddle.GetRectangle().width/2,
                                      paddle.GetRectangle().y - ball.GetRadius()});
                    ball.SetSpeed({2, -5});
                    state = PLAYING;
                } else if (restartHover) {
                    Reset(); // 重置游戏
                }
            }
            break;
        }
        case GAME_OVER: {
            bool gameOverRestartHover = CheckCollisionPointRec(mousePos, gameOverRestartButton);
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && gameOverRestartHover) {
                Reset();
            }
            break;
        }
        case VICTORY: {
            bool victoryRestartHover = CheckCollisionPointRec(mousePos, victoryRestartButton);
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && victoryRestartHover) {
                Reset();
            }
            break;
        }
        default:
            break;
    }
}

// 游戏内容绘制
void Game::Draw() {
    // 绘制背景
    if (bgLoaded) {
        DrawTexturePro(backgroundTex,
                       (Rectangle){0, 0, (float)backgroundTex.width, (float)backgroundTex.height},
                       (Rectangle){0, 0, (float)GetScreenWidth(), (float)GetScreenHeight()},
                       (Vector2){0, 0}, 0.0f, WHITE);
    } else {
        ClearBackground(RAYWHITE);
    }

    // 绘制墙体
    int screenWidth = GetScreenWidth();
    int screenHeight = GetScreenHeight();
    DrawRectangle(0, 0, 5, screenHeight, GRAY);
    DrawRectangle(screenWidth-5, 0, 5, screenHeight, GRAY);
    DrawRectangle(0, 0, screenWidth, 5, GRAY);
    DrawRectangle(0, screenHeight-5, screenWidth, 5, GRAY);

    // 绘制游戏对象
    ball.Draw();
    // 绘制挡板（优先纹理，回退到矩形）
    if (paddleLoaded) {
        Rectangle srcRect = { 0, 0, (float)paddleTex.width, (float)paddleTex.height };
        DrawTexturePro(paddleTex, srcRect, paddle.GetRectangle(), {0, 0}, 0.0f, WHITE);
    } else {
        paddle.Draw();
    }
    // 绘制砖块
    for (auto& brick : bricks) brick.Draw();

    // 绘制红线（仅游戏中/暂停扣血状态显示）
    if (state == PLAYING || state == PAUSE_LOSE_HEART) {
        DrawRectangleRec(redLine, RED);
    }

    // 绘制分数和血量
    DrawText(TextFormat("SCORE: %d", score), 10, 10, 20, BLUE);
    DrawText(TextFormat("LIVES: %d", hearts), 10, 40, 20, RED);

    // 绘制UI按钮/状态提示
    switch (state) {
        case START_SCREEN: {
            bool startHover = CheckCollisionPointRec(GetMousePosition(), startButton);
            Color btnColor = startHover ? DARKGREEN : GREEN;
            DrawRectangleRec(startButton, btnColor);
            DrawRectangleLinesEx(startButton, 2, BLACK);
            DrawText("START", startButton.x + 25, startButton.y + 12, 20, BLACK);
            break;
        }
        case PAUSE_LOSE_HEART: {
            // 半透明遮罩
            DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.7f));
            // 继续按钮
            bool continueHover = CheckCollisionPointRec(GetMousePosition(), continueButton);
            Color continueColor = continueHover ? DARKGREEN : GREEN;
            DrawRectangleRec(continueButton, continueColor);
            DrawRectangleLinesEx(continueButton, 2, BLACK);
            DrawText("CONTINUE", continueButton.x + 8, continueButton.y + 12, 15, BLACK);
            // 重启按钮
            bool restartHover = CheckCollisionPointRec(GetMousePosition(), restartButton);
            Color restartColor = restartHover ? DARKGREEN : GREEN;
            DrawRectangleRec(restartButton, restartColor);
            DrawRectangleLinesEx(restartButton, 2, BLACK);
            DrawText("RESTART", restartButton.x + 12, restartButton.y + 12, 15, BLACK);
            // 提示文字
            DrawText("yiban!", screenWidth/2 - 80, screenHeight/2 - 60, 20, RED);
            break;
        }
        case GAME_OVER: {
            DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.7f));
            DrawText("GAME OVER", screenWidth/2 - 70, screenHeight/2 - 40, 30, RED);
            bool gameOverRestartHover = CheckCollisionPointRec(GetMousePosition(), gameOverRestartButton);
            Color restartColor = gameOverRestartHover ? DARKGREEN : GREEN;
            DrawRectangleRec(gameOverRestartButton, restartColor);
            DrawRectangleLinesEx(gameOverRestartButton, 2, BLACK);
            DrawText("RESTART", gameOverRestartButton.x + 20, gameOverRestartButton.y + 12, 20, BLACK);
            break;
        }
        case VICTORY: {
            DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.7f));
            DrawText("VICTORY!", screenWidth/2 - 50, screenHeight/2 - 40, 30, GREEN);
            bool victoryRestartHover = CheckCollisionPointRec(GetMousePosition(), victoryRestartButton);
            Color restartColor = victoryRestartHover ? DARKGREEN : GREEN;
            DrawRectangleRec(victoryRestartButton, restartColor);
            DrawRectangleLinesEx(victoryRestartButton, 2, BLACK);
            DrawText("RESTART", victoryRestartButton.x + 20, victoryRestartButton.y + 12, 20, BLACK);
            break;
        }
        default:
            break;
    }
}

// 判断游戏是否继续运行（简化main中的窗口检测）
bool Game::IsGameRunning() const {
    return !WindowShouldClose();
}