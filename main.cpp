#include "raylib.h"
#include "Ball.h"
#include "Paddle.h"
#include "Brick.h"
#include <vector>
using namespace std;

// 游戏状态枚举
enum GameState {
    START_SCREEN,       // 显示 START 按钮
    PLAYING,            // 游戏进行中
    PAUSE_LOSE_HEART,   // 失去一条命，暂停并显示 continue/restart
    GAME_OVER,          // 游戏结束（三颗心用完）
    VICTORY             // 胜利（所有砖块消除）
};

// 重置游戏（回到开始界面，所有数据重置）
void ResetGame(Ball& ball, Paddle& paddle, vector<Brick>& bricks,
    int& score, int& hearts, bool& gameStarted,
    const vector<Color>& rowColors, int rows, int cols,
    float brickWidth, float brickHeight, float brickSpacing,
    float startY) {
        // 重置球
        ball.SetPosition({400, 300});
        ball.SetSpeed({0, 0});
        
        // 重置分数和血量
        score = 0;
        hearts = 3;
        
        // 重置砖块（重新创建）
        bricks.clear();
        float totalWidth = cols * brickWidth + (cols - 1) * brickSpacing;
        float startX = (GetScreenWidth() - totalWidth) / 2.0f;
        for (int row = 0; row < rows; row++) {
            Color brickColor = rowColors[row % rowColors.size()];
            for (int col = 0; col < cols; col++) {
                float x = startX + col * (brickWidth + brickSpacing);
                float y = startY + row * (brickHeight + brickSpacing);
                bricks.emplace_back(x, y, brickWidth, brickHeight, brickColor);
            }
        }
        
        // 重置游戏开始标志（用于主循环）
        gameStarted = false;
    }
    
    int main() {
        const int screenWidth = 800;
        const int screenHeight = 600;
        InitWindow(screenWidth, screenHeight, "2DBREAKOUT！！！");
        
        Texture2D backgroundTex = LoadTexture("1.png");
        Texture2D paddleTex = LoadTexture("2.png");
        bool bgLoaded = backgroundTex.id != 0;
        bool paddleLoaded = paddleTex.id != 0;
        
        
        // 创建游戏对象
        Ball ball({400, 300}, {0, 0}, 10);
        Paddle paddle(350, 550, 100, 20);
        
        // 砖块参数
        int rows = 5;
        int cols = 5;
        float brickWidth = 70;
        float brickHeight = 25;
        float brickSpacing = 10;
        float startY = 80;
        vector<Color> rowColors = { RED, ORANGE, YELLOW, GREEN, BLUE };
        
        // 砖块容器
        vector<Brick> bricks;
        
        // 游戏状态变量
        GameState state = START_SCREEN;
        int score = 0;
        int hearts = 3;          // 初始三颗心
        bool gameStarted = false; // 辅助标志，PLAYING 状态下为 true
        
        // 红线（位于挡板下方）
        Rectangle redLine = { 0, paddle.GetRectangle().y + paddle.GetRectangle().height + 5,screenWidth, 3 }; // 高度3像素，便于点击
        
        // 按钮定义
        Rectangle startButton = { screenWidth/2 - 50, screenHeight/2 - 20, 100, 40 };
        Rectangle continueButton = { screenWidth/2 - 100, screenHeight/2 - 20, 80, 40 };
        Rectangle restartButton = { screenWidth/2 + 20, screenHeight/2 - 20, 80, 40 };
        Rectangle gameOverRestartButton = { screenWidth/2 - 50, screenHeight/2 + 20, 100, 40 };
        Rectangle victoryRestartButton = { screenWidth/2 - 50, screenHeight/2 + 20, 100, 40 };
        
        // 按钮悬停标志
        bool startHover = false, continueHover = false, restartHover = false;
        bool gameOverRestartHover = false, victoryRestartHover = false;
        
        
        SetTargetFPS(60);
        
        // 初始创建砖块（用于开始界面展示）
        ResetGame(ball, paddle, bricks, score, hearts, gameStarted,rowColors, rows, cols, brickWidth, brickHeight, brickSpacing, startY);
        // 注意 ResetGame 会重置 ball 的速度为 0，且 gameStarted = false，符合 START_SCREEN 状态
        
        while (!WindowShouldClose()) {
            // 获取鼠标位置
            Vector2 mousePos = GetMousePosition();
            
            // ---------- 根据当前状态处理输入和更新 ----------
            switch (state) {
                case START_SCREEN: {
                    // 检查 START 按钮
                    startHover = CheckCollisionPointRec(mousePos, startButton);
                    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && startHover) {
                        // 开始游戏
                        state = PLAYING;
                        gameStarted = true;
                        ball.SetSpeed({4, 5});           // 赋予初始速度
                        ball.SetPosition({400, 300});    // 确保球在合适位置
                    }
                    break;
                }
                
                case PLAYING: {
                    // 游戏更新
                    ball.Move();
                    ball.BounceEdge(screenWidth, screenHeight);
                    ball.CheckCollisionPaddle(paddle);
                    ball.CheckCollisionBricks(bricks, score);
                    
                    // 挡板移动
                    if (IsKeyDown(KEY_LEFT)) paddle.MoveLeft(5);
                    if (IsKeyDown(KEY_RIGHT)) paddle.MoveRight(5);
                    
                    // 检查是否碰到红线（扣血）
                    if (CheckCollisionCircleRec(ball.GetPosition(), ball.GetRadius(), redLine)) {
                        hearts--;
                        if (hearts > 0) {
                            // 失去一颗心，进入暂停菜单
                            state = PAUSE_LOSE_HEART;
                            // 停止球运动（速度置零）
                            ball.SetSpeed({0, 0});
                        } else {
                            // 三颗心用完，游戏结束
                            state = GAME_OVER;
                            ball.SetSpeed({0, 0});
                        }
                        // 可选：播放音效等，这里略
                    }
                    
                    // 胜利检测（所有砖块消失，分数==25）
                    if (score == 25) {
                        state = VICTORY;
                        ball.SetSpeed({0, 0});
                    }
                    break;
                }
                
                case PAUSE_LOSE_HEART: {
                    // 显示继续和重启按钮，等待选择
                    continueHover = CheckCollisionPointRec(mousePos, continueButton);
                    restartHover = CheckCollisionPointRec(mousePos, restartButton);
                    
                    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                        if (continueHover) {
                            // 继续游戏：重置球的位置和速度，恢复 PLAYING 状态
                            ball.SetPosition({paddle.GetRectangle().x + paddle.GetRectangle().width/2,
                                paddle.GetRectangle().y - ball.GetRadius()});
                                ball.SetSpeed({2, -5});
                                state = PLAYING;
                            } else if (restartHover) {
                                // 重启游戏：重置所有数据，回到 START_SCREEN
                                ResetGame(ball, paddle, bricks, score, hearts, gameStarted,
                                    rowColors, rows, cols, brickWidth, brickHeight, brickSpacing, startY);
                                    state = START_SCREEN;
                                }
                            }
                        }
                        
                        case GAME_OVER: {
                            gameOverRestartHover = CheckCollisionPointRec(mousePos, gameOverRestartButton);
                            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && gameOverRestartHover) {
                                // 重置游戏，回到开始界面
                                ResetGame(ball, paddle, bricks, score, hearts, gameStarted,rowColors, rows, cols, brickWidth, brickHeight, brickSpacing, startY);
                                state = START_SCREEN;
                            }
                            break;
                        }
                        
                        case VICTORY: {
                            victoryRestartHover = CheckCollisionPointRec(mousePos, victoryRestartButton);
                            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && victoryRestartHover) {
                                ResetGame(ball, paddle, bricks, score, hearts, gameStarted,
                                    rowColors, rows, cols, brickWidth, brickHeight, brickSpacing, startY);
                                    state = START_SCREEN;
                                }
                                break;
                            }
                        }
                        
                        // ---------- 绘制 ----------
                        BeginDrawing();
                        
                        if (backgroundTex.id != 0) {
                            DrawTexturePro(backgroundTex,
                                (Rectangle){0, 0, (float)backgroundTex.width, (float)backgroundTex.height},
                                (Rectangle){0, 0, (float)screenWidth, (float)screenHeight},
                                (Vector2){0, 0}, 0.0f, WHITE);
                            } else {
                                ClearBackground(RAYWHITE);  // 失败时使用纯色
                            }
                            
                            // 绘制墙体（始终绘制）
                            DrawRectangle(0, 0, 5, screenHeight, GRAY);
                            DrawRectangle(screenWidth-5, 0, 5, screenHeight, GRAY);
                            DrawRectangle(0, 0, screenWidth, 5, GRAY);
                            DrawRectangle(0, screenHeight-5, screenWidth, 5, GRAY);
                            
                            // 绘制游戏对象（所有状态都绘制，但球可能静止）
                            ball.Draw();
                            paddle.Draw();
                            for (auto& brick : bricks) brick.Draw();
                            
                            // 绘制红线（只在 PLAYING 状态显示，其他状态可省略，但为了一致也可以始终绘制）
                            if (state == PLAYING || state == PAUSE_LOSE_HEART) {
                                DrawRectangleRec(redLine, RED);
                            }
                            
                            if (paddleLoaded) {
                                Rectangle srcRect = { 0, 0, (float)paddleTex.width, (float)paddleTex.height };
                                DrawTexturePro(paddleTex, srcRect, paddle.GetRectangle(), {0, 0}, 0.0f, WHITE);
                            } else {
                                paddle.Draw();   // 回退到蓝色矩形
                            }
                            
                            // 绘制分数和血量（始终显示）
                            DrawText(TextFormat("SCORE: %d", score), 10, 10, 20, BLUE);
                            // 绘制三颗心（简单用文字表示）
                            DrawText(TextFormat("LIVES: %d", hearts), 10, 40, 20, RED);
                            
                            
                            
                            // 根据不同状态绘制不同菜单
                            switch (state) {
                                case START_SCREEN: {
                                    Color btnColor = startHover ? DARKGREEN : GREEN;
                                    DrawRectangleRec(startButton, btnColor);
                                    DrawRectangleLinesEx(startButton, 2, BLACK);
                                    DrawText("START", startButton.x + 25, startButton.y + 12, 20, BLACK);
                                    break;
                                }
                                case PAUSE_LOSE_HEART: {
                                    // 绘制半透明背景（可选）
                                    DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.7f));
                                    
                                    // 绘制继续按钮
                                    Color continueColor = continueHover ? DARKGREEN : GREEN;
                                    DrawRectangleRec(continueButton, continueColor);
                                    DrawRectangleLinesEx(continueButton, 2, BLACK);
                                    DrawText("CONTINUE", continueButton.x + 8, continueButton.y + 12, 15, BLACK);
                                    
                                    // 绘制重启按钮
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
                                    Color restartColor = gameOverRestartHover ? DARKGREEN : GREEN;
                                    DrawRectangleRec(gameOverRestartButton, restartColor);
                                    DrawRectangleLinesEx(gameOverRestartButton, 2, BLACK);
                                    DrawText("RESTART", gameOverRestartButton.x + 20, gameOverRestartButton.y + 12, 20, BLACK);
                                    break;
                                }
                                case VICTORY: {
                                    DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.7f));
                                    DrawText("VICTORY!", screenWidth/2 - 50, screenHeight/2 - 40, 30, GREEN);
                                    Color restartColor = victoryRestartHover ? DARKGREEN : GREEN;
                                    DrawRectangleRec(victoryRestartButton, restartColor);
                                    DrawRectangleLinesEx(victoryRestartButton, 2, BLACK);
                                    DrawText("RESTART", victoryRestartButton.x + 20, victoryRestartButton.y + 12, 20, BLACK);
                                    break;
                                }
                                default: break;
                            }
                            
                            EndDrawing();
                        }
                        UnloadTexture(backgroundTex);
                        UnloadTexture(paddleTex);
                        
                        CloseWindow();
                        return 0;
                    }