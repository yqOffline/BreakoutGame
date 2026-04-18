#include "SkillBall.h"
#include "raymath.h"

SkillBall::SkillBall(Vector2 pos, SkillType type, float radius, Vector2 speed, Color glow)
    : Ball(pos, speed, radius), skillType(type), active(true), glowColor(glow), glowIntensity(1.0f) {}

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
    
    Vector2 center = GetPosition();
    float radius = GetRadius();
    
    // 光晕效果：绘制多层半透明渐变圆
    // 外层光晕
    DrawCircleGradient((int)center.x, (int)center.y, radius * 2.5f,
                       Fade(glowColor, 0.0f), Fade(glowColor, 0.3f));
    DrawCircleGradient((int)center.x, (int)center.y, radius * 2.0f,
                       Fade(glowColor, 0.2f), Fade(glowColor, 0.5f));
    DrawCircleGradient((int)center.x, (int)center.y, radius * 1.5f,
                       Fade(glowColor, 0.4f), Fade(glowColor, 0.8f));
    
    // 主体球（根据技能类型显示不同颜色）
    Color mainColor;
    switch (skillType) {
        case SkillType::PADDLE_EXTEND: mainColor = WHITE; break;
        case SkillType::BALL_ENLARGE:  mainColor = GREEN; break;
        case SkillType::BALL_SHRINK:   mainColor = RED; break;
        case SkillType::EXPLOSION:     mainColor = ORANGE; break;
        case SkillType::INVINCIBLE:    mainColor = GOLD; break;
        case SkillType::SPLIT:         mainColor = SKYBLUE; break;
        default: mainColor = GRAY; break;
    }
    DrawCircleV(center, radius, mainColor);
    
    // 内层高光
    DrawCircleV({center.x - radius*0.2f, center.y - radius*0.2f}, radius*0.3f, Fade(WHITE, 0.6f));
}