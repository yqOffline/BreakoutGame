#include "game.h"
#include <iostream>

// ======================== 构造函数（完全修复版） ========================
Game::Game(int screenWidth, int screenHeight)
    // 必须在这里初始化！不能在函数体内！
    : ball({0,0}, {0,0}, 0),
      paddle(0,0,0,0),
      score(0),
      gameStarted(false),
      state(START_SCREEN),
      brickRowColors({RED, ORANGE, YELLOW, GREEN, BLUE})
{
    // 1. 加载配置
    std::ifstream f("config.json");
    f >> config;
    f.close();

    // 2. 读取配置
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

    // 3. 重新给 ball 和 paddle 正确赋值
    ball = Ball({(float)ballX, (float)ballY}, {speedX, speedY}, ballR);
    paddle = Paddle((sw - padW) / 2, padY, padW, padH);

    // 砖块配置
    brickRows = config["bricks"]["rows"];
    brickCols = config["bricks"]["cols"];
    brickWidth = config["bricks"]["width"];
    brickHeight = config["bricks"]["height"];
    brickSpacing = config["bricks"]["spacing"];
    brickStartY = config["bricks"]["start_y"];

    hearts = config["game"]["initial_hearts"];

    // UI
    redLine = {
        0.0f,
        paddle.GetRectangle().y + paddle.GetRectangle().height + 5.0f,
        (float)sw,
        3.0f
    };

    startButton = { (float)sw/2 - 50, (float)sh/2 - 20, 100, 40 };
    continueButton = { (float)sw/2 - 100, (float)sh/2 - 20, 80, 40 };
    restartButton = { (float)sw/2 + 20, (float)sh/2 - 20, 80, 40 };
    gameOverRestartButton = { (float)sw/2 - 50, (float)sh/2 + 20, 100, 40 };
    victoryRestartButton = { (float)sw/2 - 50, (float)sh/2 + 20, 100, 40 };

    backgroundTex = LoadTexture("1.png");
    paddleTex = LoadTexture("2.png");
    bgLoaded = (backgroundTex.id != 0);
    paddleLoaded = (paddleTex.id != 0);

    ResetBricks();
}

Game::~Game() {
    UnloadTexture(backgroundTex);
    UnloadTexture(paddleTex);
}

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

void Game::CheckGameVictory() {
    if (score == brickRows * brickCols) {
        state = VICTORY;
        ball.SetSpeed({0,0});
    }
}

void Game::CheckBallHitRedLine() {
    if (CheckCollisionCircleRec(ball.GetPosition(), ball.GetRadius(), redLine)) {
        hearts--;
        ball.SetSpeed({0,0});
        state = hearts > 0 ? PAUSE_LOSE_HEART : GAME_OVER;
    }
}

void Game::Reset() {
    int bx = config["ball"]["init_x"];
    int by = config["ball"]["init_y"];
    ball.SetPosition({(float)bx, (float)by});
    ball.SetSpeed({0,0});
    hearts = config["game"]["initial_hearts"];
    score = 0;
    gameStarted = false;
    state = START_SCREEN;
    ResetBricks();
}

void Game::Update(float dt) {
    if (state != PLAYING) return;

    ball.Move();
    ball.BounceEdge(GetScreenWidth(), GetScreenHeight());
    ball.CheckCollisionPaddle(paddle);
    ball.CheckCollisionBricks(bricks, score);

    if (IsKeyDown(KEY_LEFT)) paddle.MoveLeft(paddleMoveSpeed);
    if (IsKeyDown(KEY_RIGHT)) paddle.MoveRight(paddleMoveSpeed);

    CheckBallHitRedLine();
    CheckGameVictory();
}

void Game::HandleInput(Vector2 m) {
    switch (state) {
        case START_SCREEN:
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(m, startButton)) {
                state = PLAYING;
                ball.SetSpeed({config["ball"]["speed_x"], config["ball"]["speed_y"]});
            }
            break;

        case PAUSE_LOSE_HEART:
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (CheckCollisionPointRec(m, continueButton)) {
                    ball.SetPosition({paddle.GetRectangle().x + paddle.GetRectangle().width/2, paddle.GetRectangle().y - 10});
                    ball.SetSpeed({2, -5});
                    state = PLAYING;
                } else if (CheckCollisionPointRec(m, restartButton)) {
                    Reset();
                }
            }
            break;

        case GAME_OVER:
        case VICTORY:
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(m, gameOverRestartButton)) {
                Reset();
            }
            break;

        default: break;
    }
}

void Game::Draw() {
    if (bgLoaded) {
        DrawTexturePro(backgroundTex,
            {0,0,(float)backgroundTex.width,(float)backgroundTex.height},
            {0,0,(float)GetScreenWidth(),(float)GetScreenHeight()},
            {0,0},0,WHITE);
    } else {
        ClearBackground(RAYWHITE);
    }

    DrawRectangle(0,0,5,GetScreenHeight(),GRAY);
    DrawRectangle(GetScreenWidth()-5,0,5,GetScreenHeight(),GRAY);
    DrawRectangle(0,0,GetScreenWidth(),5,GRAY);
    DrawRectangle(0,GetScreenHeight()-5,GetScreenWidth(),5,GRAY);

    ball.Draw();

    if (paddleLoaded) {
        DrawTexturePro(paddleTex,
            {0,0,(float)paddleTex.width,(float)paddleTex.height},
            paddle.GetRectangle(), {0,0},0,WHITE);
    } else {
        paddle.Draw();
    }

    for (auto& b : bricks) b.Draw();

    if (state == PLAYING || state == PAUSE_LOSE_HEART)
        DrawRectangleRec(redLine, RED);

    DrawText(TextFormat("SCORE: %d", score), 10,10,20,BLUE);
    DrawText(TextFormat("LIVES: %d", hearts),10,40,20,RED);

    switch (state) {
        case START_SCREEN:
            DrawRectangleRec(startButton, CheckCollisionPointRec(GetMousePosition(),startButton)? DARKGREEN:GREEN);
            DrawText("START", startButton.x+25, startButton.y+12,20,BLACK);
            break;
        case PAUSE_LOSE_HEART:
            DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),Fade(BLACK,0.7f));
            DrawRectangleRec(continueButton, GREEN);
            DrawText("CONTINUE", continueButton.x+5, continueButton.y+12,15,BLACK);
            DrawRectangleRec(restartButton, GREEN);
            DrawText("RESTART", restartButton.x+10, restartButton.y+12,15,BLACK);
            break;
        case GAME_OVER:
            DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),Fade(BLACK,0.7f));
            DrawText("GAME OVER", GetScreenWidth()/2-70, GetScreenHeight()/2-40,30,RED);
            DrawRectangleRec(gameOverRestartButton, GREEN);
            DrawText("RESTART", gameOverRestartButton.x+20, gameOverRestartButton.y+12,20,BLACK);
            break;
        case VICTORY:
            DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),Fade(BLACK,0.7f));
            DrawText("VICTORY", GetScreenWidth()/2-60, GetScreenHeight()/2-40,30,GREEN);
            DrawRectangleRec(victoryRestartButton, GREEN);
            DrawText("RESTART", victoryRestartButton.x+20, victoryRestartButton.y+12,20,BLACK);
            break;
        default: break;
    }
}

bool Game::IsGameRunning() const {
    return !WindowShouldClose();
}