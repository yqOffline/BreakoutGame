#include "game.h"
#include "ball.h"
#include "paddle.h"
#include "brick.h"
#include <cstdio>
#include <iostream>

// ------------------------------
// 测试 1：球是否正确初始化
// ------------------------------
bool testBallInitialization() {
    Ball ball({400, 300}, {2, -3}, 10);
    Vector2 pos = ball.GetPosition();
    Vector2 spd = ball.GetSpeed();
    bool ok = true;

    if (pos.x != 400 || pos.y != 300) ok = false;
    if (spd.x != 2 || spd.y != -3) ok = false;
    if (ball.GetRadius() != 10) ok = false;

    printf("Test 1 (球初始化): %s\n", ok ? "PASS" : "FAIL");
    return ok;
}

// ------------------------------
// 测试 2：挡板是否正确初始化
// ------------------------------
bool testPaddleInitialization() {
    Paddle paddle(300, 500, 120, 20);
    Rectangle r = paddle.GetRectangle();
    bool ok = true;

    if (r.x != 300) ok = false;
    if (r.y != 500) ok = false;
    if (r.width != 120) ok = false;
    if (r.height != 20) ok = false;

    printf("Test 2 (挡板初始化): %s\n", ok ? "PASS" : "FAIL");
    return ok;
}

// ------------------------------
// 测试 3：球碰撞砖块 → 得分+砖块消失
// ------------------------------
bool testBallBrickCollision() {
    Ball ball({100, 100}, {0, 0}, 10);
    Brick brick(90, 90, 30, 20, RED);
    std::vector<Brick> bricks = {brick};
    int score = 0;

    ball.CheckCollisionBricks(bricks, score);
    bool ok = (score == 1 && bricks.empty());

    printf("Test 3 (球撞砖块得分): %s\n", ok ? "PASS" : "FAIL");
    return ok;
}

// ------------------------------
// 测试 4：掉球 → 生命减少
// ------------------------------
bool testLifeLoss() {
    Game game(800, 600);
    int lifeBefore = game.GetHearts(); // 你需要我帮你加一个 GetHearts()

    // 模拟球掉红线
    game.SimulateBallDrop();
    int lifeAfter = game.GetHearts();
    bool ok = (lifeAfter == lifeBefore - 1);

    printf("Test 4 (掉球扣心): %s\n", ok ? "PASS" : "FAIL");
    return ok;
}

// ------------------------------
// 主测试入口
// ------------------------------
int main() {
    printf("=== 游戏单元测试开始 ===\n\n");

    bool allPass = true;
    allPass &= testBallInitialization();
    allPass &= testPaddleInitialization();
    allPass &= testBallBrickCollision();
    // allPass &= testLifeLoss(); 需要加 Getter

    printf("\n=== 测试结果: %s ===\n", allPass ? "全部通过 ✅" : "存在失败 ❌");
    return 0;
}