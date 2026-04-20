#include "Ball.h"
#include "Brick.h"

Ball::Ball(Vector2 pos, Vector2 sp, float r) {
    position = pos;
    speed = sp;
    radius = r;
}

void Ball::Move() {
    position.x += speed.x;
    position.y += speed.y;
}

void Ball::Draw() {
    DrawCircleV(position, radius, RED);
}


bool Ball::BounceEdge(int screenWidth, int screenHeight) {
    bool bounced = false;
    // 左右边界
    if (position.x - radius <= 0 || position.x + radius >= screenWidth) {
        speed.x *= -1;
    }
    // 上边界
    if (position.y - radius <= 0) {
        speed.y *= -1;
    }
    // 下边界（新增）
    if (position.y + radius >= screenHeight) {
        speed.y *= -1;
    }
    return bounced;
}

bool Ball::CheckCollisionPaddle(Paddle& paddle)
{
    if (CheckCollisionCircleRec(position,radius,paddle.GetRectangle()))
    {
       if (speed.y > 0) { // 只在球下落时反弹
            speed.y *= -1;
            // 将球置于挡板正上方，防止卡入
            position.y = paddle.GetRectangle().y - radius;

            // 可选：根据碰撞点偏移改变 x 速度
            float hitPos = position.x - paddle.GetRectangle().x;
            float normalized = hitPos / paddle.GetRectangle().width; // 0..1
            float angle = (normalized - 0.5f) * 1.5f;           // -0.75..0.75
            speed.x = 10 * angle;                                // 范围约 -3..3
            if (speed.x > -6 && speed.x < 6) {
                speed.x = (speed.x > 0) ? 6 : -6;
            }
       }
       return true;
    }
    return false;
}

void Ball::CheckCollisionBricks(std::vector<Brick>& bricks, int& score)
{
    for (auto& brick : bricks) {
        if (brick.IsActive() && CheckCollisionCircleRec(position, radius, brick.GetRectangle())) {
            brick.SetActive(false);
            score++;

            // 简单反转 y 速度
            speed.y *= -1;

            // 调整球的位置，防止卡入砖块
            if (speed.y > 0) {
                position.y = brick.GetRectangle().y + brick.GetRectangle().height + radius;
            } else {
                position.y = brick.GetRectangle().y - radius;
            }

            break; // 每帧只处理一个砖块，避免一帧内多碰撞
        }
    }
}