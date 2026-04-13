#include "game.h"
#include <random>

Game::Game() : score(0), lives(3), state(GameState::MENU), pauseCause(PauseCause::MANUAL_PAUSE),
               currentLevel(1), unlockedLevel(1), totalBricks(0), isTextureLoaded(false) {}

Game::~Game() {
    if (isTextureLoaded) UnloadTexture(backgroundTex);
}

void Game::Init(const json& config) {
    screenWidth = config["screen"]["width"];
    screenHeight = config["screen"]["height"];
    bottomLineY = screenHeight - 50;

    for (int i = 0; i < 3; i++) {
        levelConfigs[i] = config["levels"][i];
    }

    backgroundTex = LoadTexture("resources/background.png");
    isTextureLoaded = true;

    LoadLevel(1);
}

void Game::LoadLevel(int level) {
    currentLevel = level;
    json cfg = levelConfigs[level - 1];

    score = 0;
    lives = cfg["game"]["lives"];
    // 修复JSON报错：显式转换为int
    int rows = cfg["bricks"]["rows"].get<int>();
    int cols = cfg["bricks"]["cols"].get<int>();
    totalBricks = rows * cols;

    // 初始化小球/挡板
    ball.Init(cfg["ball"]);
    paddle.Init(cfg["paddle"]);
    bricks.clear();

    float w = cfg["bricks"]["width"].get<float>();
    float h = cfg["bricks"]["height"].get<float>();
    float spacing = cfg["bricks"]["spacing"].get<float>();
    float startX = (screenWidth - (cols * (w + spacing))) / 2;
    float startY = cfg["bricks"]["startY"].get<float>();
    Color colors[5] = {RED, ORANGE, YELLOW, GREEN, BLUE};

    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            float x = startX + j * (w + spacing);
            float y = startY + i * (h + spacing);
            bricks.emplace_back(x, y, w, h, colors[i % 5]);
        }
    }
    state = GameState::PLAYING;
}

void Game::ResetCurrentLevel() {
    LoadLevel(currentLevel);
}

void Game::ResetGame() {
    unlockedLevel = 1;
    LoadLevel(1);
}

void Game::CheckLevelComplete() {
    if (state != GameState::PLAYING) return;
    int activeBricks = 0;
    for (auto& brick : bricks) if (brick.IsActive()) activeBricks++;
    if (activeBricks == 0) {
        if (currentLevel < 3 && currentLevel == unlockedLevel) {
            unlockedLevel = currentLevel + 1;
        }
        if (currentLevel == 3)
            state = GameState::GAME_VICTORY;
        else
            state = GameState::LEVEL_VICTORY;
    }
}

void Game::HandleInput() {
    Vector2 mouse = GetMousePosition();

    if (state == GameState::MENU) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mouse, {300, 250, 200, 50})) {
            state = GameState::LEVEL_SELECT;
        }
    }
    else if (state == GameState::LEVEL_SELECT) {
        if (unlockedLevel >= 1 && CheckCollisionPointRec(mouse, {200, 200, 150, 80}) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            LoadLevel(1);
        if (unlockedLevel >= 2 && CheckCollisionPointRec(mouse, {325, 200, 150, 80}) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            LoadLevel(2);
        if (unlockedLevel >= 3 && CheckCollisionPointRec(mouse, {450, 200, 150, 80}) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            LoadLevel(3);
        if (CheckCollisionPointRec(mouse, {300, 300, 200, 50}) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            state = GameState::MENU;
    }
    else if (state == GameState::PLAYING) {
        paddle.Move();
        if (IsKeyPressed(KEY_SPACE)) {
            state = GameState::PAUSED;
            pauseCause = PauseCause::MANUAL_PAUSE;
        }
        if (ball.GetPosition().y + ball.GetRadius() > bottomLineY) {
            lives--;
            state = GameState::PAUSED;
            pauseCause = PauseCause::LIFE_LOSS_PAUSE;
        }
        if (lives <= 0) state = GameState::GAME_OVER;
    }
    else if (state == GameState::PAUSED) {
        if (IsKeyPressed(KEY_SPACE)) {
            state = GameState::PLAYING;
            if (pauseCause == PauseCause::LIFE_LOSS_PAUSE) ball.Reset();
        }
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mouse, {300, 250, 200, 50})) {
            state = GameState::PLAYING;
            if (pauseCause == PauseCause::LIFE_LOSS_PAUSE) ball.Reset();
        }
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mouse, {300, 320, 200, 50})) {
            ResetCurrentLevel();
        }
    }
    else if (state == GameState::GAME_OVER) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mouse, {300, 250, 200, 50})) {
            ResetCurrentLevel();
        }
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mouse, {300, 320, 200, 50})) {
            state = GameState::LEVEL_SELECT;
        }
    }
    else if (state == GameState::LEVEL_VICTORY) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mouse, {300, 250, 200, 50})) {
            LoadLevel(currentLevel + 1);
        }
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mouse, {300, 320, 200, 50})) {
            state = GameState::LEVEL_SELECT;
        }
    }
    else if (state == GameState::GAME_VICTORY) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mouse, {300, 250, 200, 50})) {
            ResetGame();
            state = GameState::LEVEL_SELECT;
        }
    }
}

void Game::Update() {
    if (state == GameState::PLAYING) {
        ball.Update();
        ball.CheckCollisionPaddle(paddle);
        for (auto& brick : bricks) {
            if (brick.IsActive() && ball.CheckCollisionBrick(brick)) {
                score += 10;
                break;
            }
        }
        CheckLevelComplete();
    }
}

void Game::Draw() {
    DrawTexture(backgroundTex, 0, 0, WHITE);
    if (state == GameState::MENU) {
        DrawText("BRICK BREAKER", 250, 100, 50, WHITE);
        DrawRectangle(300, 250, 200, 50, BLUE);
        DrawText("START GAME", 320, 260, 30, WHITE);
    }
    else if (state == GameState::LEVEL_SELECT) DrawLevelSelect();
    else {
        paddle.Draw();
        ball.Draw();
        for (auto& brick : bricks) brick.Draw();
        DrawUI();
        if (state == GameState::PAUSED) DrawPauseMenu();
        if (state == GameState::GAME_OVER) DrawGameOver();
        if (state == GameState::LEVEL_VICTORY) DrawLevelVictory();
        if (state == GameState::GAME_VICTORY) DrawAllVictory();
    }
}

void Game::DrawLevelSelect() {
    DrawText("SELECT LEVEL", 250, 100, 50, WHITE);
    DrawRectangle(200, 200, 150, 80, unlockedLevel>=1 ? GREEN : GRAY);
    DrawText("LEVEL 1", 220, 220, 25, WHITE);
    DrawRectangle(325, 200, 150, 80, unlockedLevel>=2 ? GREEN : GRAY);
    DrawText("LEVEL 2", 345, 220, 25, WHITE);
    DrawRectangle(450, 200, 150, 80, unlockedLevel>=3 ? GREEN : GRAY);
    DrawText("LEVEL 3", 470, 220, 25, WHITE);
    DrawRectangle(300, 300, 200, 50, BLUE);
    DrawText("BACK MENU", 320, 310, 30, WHITE);
}

void Game::DrawLevelVictory() {
    DrawText("LEVEL CLEAR!", 250, 150, 50, GREEN);
    DrawRectangle(300, 250, 200, 50, BLUE);
    DrawText("NEXT LEVEL", 320, 260, 25, WHITE);
    DrawRectangle(300, 320, 200, 50, BLUE);
    DrawText("LEVEL SELECT", 310, 330, 20, WHITE);
}

void Game::DrawAllVictory() {
    DrawText("ALL LEVELS CLEAR!", 180, 150, 50, GOLD);
    DrawText("CONGRATULATIONS!", 200, 220, 40, WHITE);
    DrawRectangle(300, 300, 200, 50, BLUE);
    DrawText("PLAY AGAIN", 320, 310, 30, WHITE);
}

void Game::DrawPauseMenu() {
    DrawRectangle(300, 250, 200, 50, BLUE);
    DrawText("CONTINUE", 330, 260, 30, WHITE);
    DrawRectangle(300, 320, 200, 50, BLUE);
    DrawText("RESTART", 330, 330, 30, WHITE);
}

void Game::DrawGameOver() {
    DrawText("GAME OVER", 280, 150, 50, RED);
    DrawRectangle(300, 250, 200, 50, BLUE);
    DrawText("TRY AGAIN", 330, 260, 30, WHITE);
    DrawRectangle(300, 320, 200, 50, BLUE);
    DrawText("LEVEL SELECT", 310, 330, 20, WHITE);
}

void Game::DrawUI() {
    DrawText(TextFormat("SCORE: %d", score), 10, 10, 20, WHITE);
    DrawText(TextFormat("LIVES: %d", lives), 10, 40, 20, WHITE);
    DrawText(TextFormat("LEVEL: %d", currentLevel), 680, 10, 20, WHITE);
    DrawLine(0, bottomLineY, screenWidth, bottomLineY, RED);
}