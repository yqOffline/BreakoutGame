#ifndef GAME_H
#define GAME_H

#include "raylib.h"
#include "Ball.h"
#include "Paddle.h"
#include "Brick.h"
#include <vector>

// 游戏状态枚举（复用原枚举，移到此处供外部使用）
enum GameState {
    START_SCREEN,       // 显示 START 按钮
    PLAYING,            // 游戏进行中
    PAUSE_LOSE_HEART,   // 失去一条命，暂停并显示 continue/restart
    GAME_OVER,          // 游戏结束（三颗心用完）
    VICTORY             // 胜利（所有砖块消除）
};

// 游戏核心类：封装所有游戏数据和逻辑
class Game {
private:
    // 游戏对象
    Ball ball;
    Paddle paddle;
    std::vector<Brick> bricks;
    
    // 游戏参数
    int score;
    int hearts;
    bool gameStarted;
    GameState state;
    
    // 砖块配置
    int brickRows;
    int brickCols;
    float brickWidth;
    float brickHeight;
    float brickSpacing;
    float brickStartY;
    std::vector<Color> brickRowColors;
    
    // UI 元素（按钮、红线）
    Rectangle redLine;
    Rectangle startButton;
    Rectangle continueButton;
    Rectangle restartButton;
    Rectangle gameOverRestartButton;
    Rectangle victoryRestartButton;
    
    // 纹理（游戏内管理纹理加载/卸载）
    Texture2D backgroundTex;
    Texture2D paddleTex;
    bool bgLoaded;
    bool paddleLoaded;

    // 私有方法：内部逻辑封装
    void ResetBricks();          // 重置砖块生成
    void CheckGameVictory();     // 检测胜利条件
    void CheckBallHitRedLine();  // 检测球碰到红线（扣血）

public:
    // 构造函数：初始化游戏参数和对象
    Game(int screenWidth, int screenHeight);
    
    // 析构函数：释放纹理资源
    ~Game();
    void Reset();
    
    // 核心逻辑方法
    void Update(float deltaTime);  // 游戏逻辑更新（每帧调用）
    void Draw();                   // 游戏内容绘制（每帧调用）
    void HandleInput(Vector2 mousePos); // 处理输入（鼠标/键盘）
    
    // 辅助方法
    bool IsGameRunning() const;    // 判断游戏是否需要继续运行（窗口关闭检测）
    GameState GetState() const { return state; }
};

#endif