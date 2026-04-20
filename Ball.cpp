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
    if (position.x - radius <= 0 || position.x + radius >= screenWidth) {
        speed.x *= -1;
        bounced = true;
    }
    if (position.y - radius <= 0) {
        speed.y *= -1;
        bounced = true;
    }
    if (position.y + radius >= screenHeight) {
        speed.y *= -1;
        bounced = true;
    }
    return bounced;
}

bool Ball::CheckCollisionPaddle(Paddle& paddle) {
    if (CheckCollisionCircleRec(position, radius, paddle.GetRectangle())) {
        if (speed.y > 0) {
            speed.y *= -1;
            position.y = paddle.GetRectangle().y - radius;

            float hitPos = position.x - paddle.GetRectangle().x;
            float normalized = hitPos / paddle.GetRectangle().width;
            float angle = (normalized - 0.5f) * 1.5f;
            speed.x = 10 * angle;
            if (speed.x > -6 && speed.x < 6) {
                speed.x = (speed.x > 0) ? 6 : -6;
            }
        }
        return true;
    }
    return false;
}

void Ball::CheckCollisionBricks(std::vector<Brick>& bricks, int& score) {
    for (auto& brick : bricks) {
        if (brick.IsActive() && CheckCollisionCircleRec(position, radius, brick.GetRectangle())) {
            brick.SetActive(false);
            score++;

            speed.y *= -1;

            if (speed.y > 0) {
                position.y = brick.GetRectangle().y + brick.GetRectangle().height + radius;
            } else {
                position.y = brick.GetRectangle().y - radius;
            }

            break;
        }
    }
}