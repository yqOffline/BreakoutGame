#include "Paddle.h"

Paddle::Paddle(float x, float y, float w, float h) {
    rect = { x, y, w, h };
    speed = 8.0f;
}

// 修复：JSON初始化函数
void Paddle::Init(const json& cfg) {
    rect.width = cfg["width"];
    rect.height = cfg["height"];
    speed = cfg["speed"];
    rect.x = 400 - rect.width/2;
    rect.y = 550;
}

// 修复：Game类调用的统一Move函数
void Paddle::Move() {
    if (IsKeyDown(KEY_LEFT)) MoveLeft(speed);
    if (IsKeyDown(KEY_RIGHT)) MoveRight(speed);

    // 边界限制
    if (rect.x < 0) rect.x = 0;
    if (rect.x + rect.width > 800) rect.x = 800 - rect.width;
}

void Paddle::MoveLeft(float speed) {
    rect.x -= speed;
}

void Paddle::MoveRight(float speed) {
    rect.x += speed;
}

void Paddle::Draw() {
    DrawRectangleRec(rect, BLUE);
}