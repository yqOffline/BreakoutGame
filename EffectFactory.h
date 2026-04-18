#ifndef EFFECT_FACTORY_H
#define EFFECT_FACTORY_H

#include "Effect.h"
#include "SkillBall.h"
#include "json.hpp"
#include <memory>

using json = nlohmann::json;

class EffectFactory {
public:
    // 从配置加载参数
    static void LoadConfig(const json& config);
    
    // 根据技能类型创建效果对象
    static std::unique_ptr<Effect> CreateEffect(SkillType type);
    
private:
    // 配置参数静态存储
    static float paddleExtendDuration;
    static float paddleExtendFactor;
    static float ballEnlargeDuration;
    static float ballEnlargeFactor;
    static float ballShrinkDuration;
    static float ballShrinkFactor;
    static float explosionDuration;
    static float invincibleDuration;
};

#endif // EFFECT_FACTORY_H