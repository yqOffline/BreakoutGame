#include "Ball.h"
#include "Paddle.h"
#include "Brick.h"
#include <raylib.h>

// 构造函数
Ball::Ball(Vector2 pos, Vector2 sp, float r) : position(pos), speed(sp), radius(r) {}

// ===================== 缺失的JSON初始化函数（修复报错） =====================
void Ball::Init(const json& cfg) {
    radius = cfg["radius"];
    position = { 400.0f, 500.0f };
    speed.x = cfg["speedX"];
    speed.y = cfg["speedY"];
}

// ===================== 缺失的重置函数（修复报错） =====================
void Ball::Reset() {
    position = { 400.0f, 500.0f };
    speed.x = 4.0f;
    speed.y = -4.0f;
}

// ===================== 缺失的Update函数（修复报错） =====================
void Ball::Update() {
    position.x += speed.x;
    position.y += speed.y;
    BounceEdge(800, 600);
}

// 移动函数
void Ball::Move() {
    position.x += speed.x;
    position.y += speed.y;
}

// 绘制
void Ball::Draw() {
    DrawCircleV(position, radius, WHITE);
}

// 边界反弹
void Ball::BounceEdge(int screenWidth, int screenHeight) {
    if (position.x - radius <= 0 || position.x + radius >= screenWidth) {
        speed.x *= -1;
    }
    if (position.y - radius <= 0) {
        speed.y *= -1;
    }
}

// 挡板碰撞
void Ball::CheckCollisionPaddle(Paddle& paddle) {
    if (CheckCollisionCircleRec(position, radius, paddle.GetRect())) {
        speed.y *= -1;
        if (speed.y > 0) {
            position.y = paddle.GetRect().y - radius;
        }
        float hitPos = position.x - paddle.GetRect().x;
        float normalized = hitPos / paddle.GetRect().width;
        speed.x = (normalized - 0.5f) * 8.0f;
    }
}

// ===================== 缺失的单砖块碰撞函数（修复报错） =====================
bool Ball::CheckCollisionBrick(Brick& brick) {
    if (brick.IsActive() && CheckCollisionCircleRec(position, radius, brick.GetRectangle())) {
        speed.y *= -1;
        brick.SetActive(false);
        return true;
    }
    return false;
}

// 批量砖块碰撞
void Ball::CheckCollisionBricks(std::vector<Brick>& bricks, int& score) {
    for (auto& brick : bricks) {
        if (CheckCollisionBrick(brick)) {
            score += 10;
            break;
        }
    }
}