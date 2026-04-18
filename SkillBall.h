#ifndef SKILL_BALL_H
#define SKILL_BALL_H

#include "Ball.h"
#include "Paddle.h"

enum class SkillType {
    PADDLE_EXTEND,  // 板变长
    BALL_ENLARGE,   // 球变大
    BALL_SHRINK,    // 球变小
    EXPLOSION,      // 爆炸球
    INVINCIBLE,     // 无敌球
    SPLIT           // 分裂球
};

class SkillBall : public Ball {
public:
    SkillType skillType;
    bool active;
    Color glowColor;        // 光晕颜色（从配置读取）
    float glowIntensity;    // 光晕强度（可选）
    
    SkillBall(Vector2 pos, SkillType type, float radius, Vector2 speed, Color glow = WHITE);
    
    void Update(float dt);
    void Draw() override;   // 重写绘制，增加光晕
    
    // 移除 ApplyEffect 方法，效果管理交给工厂模式
};

#endif // SKILL_BALL_H