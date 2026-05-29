#include "Ball.h"
#include "Brick.h"
#include "TextureCache.h"
#include <cmath>

Texture2D Ball::ballTexture = { 0 };

void Ball::GenerateTexture() {
    if (ballTexture.id != 0) return;
    int size = 64;
    Image img = GenImageColor(size, size, BLANK);
    // 径向渐变：中心白 -> 边缘橙红 -> 外围透明
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            float dx = x - size/2.0f;
            float dy = y - size/2.0f;
            float dist = sqrtf(dx*dx + dy*dy);
            float r = dist / (size/2.0f);
            if (r <= 1.0f) {
                float intensity = 1.0f - r * 0.8f; // 中心亮边缘暗
                Color c = { (unsigned char)(255 * intensity),
                            (unsigned char)(100 * intensity),
                            (unsigned char)(50 * intensity),
                            255 };
                ImageDrawPixel(&img, x, y, c);
            } else {
                ImageDrawPixel(&img, x, y, BLANK);
            }
        }
    }
    ballTexture = LoadTextureFromImage(img);
    UnloadImage(img);
}

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
    if (ballTexture.id != 0) {
        Rectangle srcRect = { 0, 0, (float)ballTexture.width, (float)ballTexture.height };
        Rectangle destRect = { position.x - radius, position.y - radius, radius*2, radius*2 };
        DrawTexturePro(ballTexture, srcRect, destRect, {0,0}, 0, WHITE);
    } else {
        DrawCircleV(position, radius, RED);
    }
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