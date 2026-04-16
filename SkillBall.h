#ifndef SKILL_BALL_H
#define SKILL_BALL_H

#include "Ball.h"
#include "Paddle.h"

enum class SkillType {
    PADDLE_EXTEND,  // 板变长 (白色)
    BALL_ENLARGE,   // 球变大 (绿色)
    BALL_SHRINK     // 球变小 (红色)
};

class SkillBall : public Ball {
public:
    SkillType skillType;
    bool active;
    
    SkillBall(Vector2 pos, SkillType type, float radius, Vector2 speed);
    
    void Update(float dt);
    void Draw();
    
    // 应用效果到挡板和主球，返回效果持续时间（秒）
    float ApplyEffect(Paddle& paddle, Ball& mainBall) const;
    
    // 获取对应颜色
    Color GetColor() const;
};

#endif // SKILL_BALL_H