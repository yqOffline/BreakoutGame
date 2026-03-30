#ifndef GAME_H
#define GAME_H

#include "raylib.h"
#include "Ball.h"
#include "Paddle.h"
#include "Brick.h"
#include <vector>
#include <fstream>       // 必须加！
#include "json.hpp"

using json = nlohmann::json;

enum GameState {
    START_SCREEN,
    PLAYING,
    PAUSE_LOSE_HEART,
    GAME_OVER,
    VICTORY
};

class Game {
private:
    json config;

    Ball ball;
    Paddle paddle;
    std::vector<Brick> bricks;

    int score;
    int hearts;
    bool gameStarted;
    GameState state;
    int paddleMoveSpeed;

    int brickRows;
    int brickCols;
    float brickWidth;
    float brickHeight;
    float brickSpacing;
    float brickStartY;
    std::vector<Color> brickRowColors;

    Rectangle redLine;
    Rectangle startButton;
    Rectangle continueButton;
    Rectangle restartButton;
    Rectangle gameOverRestartButton;
    Rectangle victoryRestartButton;

    Texture2D backgroundTex;
    Texture2D paddleTex;
    bool bgLoaded;
    bool paddleLoaded;

    void ResetBricks();
    void CheckGameVictory();
    void CheckBallHitRedLine();

public:
    Game(int screenWidth, int screenHeight);
    ~Game();
    void Reset();
    void Update(float deltaTime);
    void Draw();
    void HandleInput(Vector2 mousePos);
    bool IsGameRunning() const;
    GameState GetState() const { return state; }
};

#endif