#include "SkillBall.h"

SkillBall::SkillBall(Vector2 pos, SkillType type, float radius, Vector2 speed)
    : Ball(pos, speed, radius), skillType(type), active(true) {}

void SkillBall::Update(float dt) {
    if (!active) return;
    Vector2 pos = GetPosition();
    Vector2 sp = GetSpeed();
    pos.x += sp.x * dt;
    pos.y += sp.y * dt;
    SetPosition(pos);
    
    if (pos.y > GetScreenHeight() + GetRadius()) {
        active = false;
    }
}

void SkillBall::Draw() {
    if (!active) return;
    DrawCircleV(GetPosition(), GetRadius(), GetColor());
}


float SkillBall::ApplyEffect(Paddle& paddle, Ball& mainBall) const {
    switch (skillType) {
        case SkillType::PADDLE_EXTEND: {
            float currentW = paddle.GetRectangle().width;
            paddle.SetWidth(currentW * 1.5f);
            return 5.0f; // 持续时间
        }
        case SkillType::BALL_ENLARGE: {
            float currentR = mainBall.GetRadius();
            mainBall.SetRadius(currentR * 1.5f);
            return 5.0f;
        }
        case SkillType::BALL_SHRINK: {
            float currentR = mainBall.GetRadius();
            mainBall.SetRadius(currentR * 0.7f);
            return 5.0f;
        }
        default: return 0.0f;
    }
}

Color SkillBall::GetColor() const {
    switch (skillType) {
        case SkillType::PADDLE_EXTEND: return WHITE;
        case SkillType::BALL_ENLARGE:  return GREEN;
        case SkillType::BALL_SHRINK:   return RED;
        default: return GRAY;
    }
}