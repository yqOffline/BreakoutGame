#include "EffectFactory.h"

// 静态成员定义
float EffectFactory::paddleExtendDuration = 5.0f;
float EffectFactory::paddleExtendFactor = 1.5f;
float EffectFactory::ballEnlargeDuration = 5.0f;
float EffectFactory::ballEnlargeFactor = 1.5f;
float EffectFactory::ballShrinkDuration = 5.0f;
float EffectFactory::ballShrinkFactor = 0.7f;
float EffectFactory::explosionDuration = 5.0f;
float EffectFactory::invincibleDuration = 5.0f;

void EffectFactory::LoadConfig(const json& config) {
    if (config.contains("skill_ball")) {
        const auto& sb = config["skill_ball"];
        // 全局参数
        // 可为每种技能单独配置，这里示例扩展配置结构
        if (sb.contains("paddle_extend")) {
            const auto& pe = sb["paddle_extend"];
            paddleExtendDuration = pe.value("duration", 5.0f);
            paddleExtendFactor = pe.value("factor", 1.5f);
        } else {
            paddleExtendDuration = sb.value("buff_duration", 5.0f);
            paddleExtendFactor = sb.value("paddle_extend_factor", 1.5f);
        }
        if (sb.contains("ball_enlarge")) {
            const auto& be = sb["ball_enlarge"];
            ballEnlargeDuration = be.value("duration", 5.0f);
            ballEnlargeFactor = be.value("factor", 1.5f);
        } else {
            ballEnlargeDuration = sb.value("buff_duration", 5.0f);
            ballEnlargeFactor = sb.value("ball_enlarge_factor", 1.5f);
        }
        if (sb.contains("ball_shrink")) {
            const auto& bs = sb["ball_shrink"];
            ballShrinkDuration = bs.value("duration", 5.0f);
            ballShrinkFactor = bs.value("factor", 0.7f);
        } else {
            ballShrinkDuration = sb.value("buff_duration", 5.0f);
            ballShrinkFactor = sb.value("ball_shrink_factor", 0.7f);
        }
        if (sb.contains("explosion")) {
            explosionDuration = sb["explosion"].value("duration", 5.0f);
        } else {
            explosionDuration = sb.value("buff_duration", 5.0f);
        }
        if (sb.contains("invincible")) {
            invincibleDuration = sb["invincible"].value("duration", 5.0f);
        } else {
            invincibleDuration = sb.value("buff_duration", 5.0f);
        }
    }
}

std::unique_ptr<Effect> EffectFactory::CreateEffect(SkillType type) {
    switch (type) {
        case SkillType::PADDLE_EXTEND:
            return std::make_unique<PaddleExtendEffect>(paddleExtendDuration, paddleExtendFactor);
        case SkillType::BALL_ENLARGE:
            return std::make_unique<BallEnlargeEffect>(ballEnlargeDuration, ballEnlargeFactor);
        case SkillType::BALL_SHRINK:
            return std::make_unique<BallShrinkEffect>(ballShrinkDuration, ballShrinkFactor);
        case SkillType::EXPLOSION:
            return std::make_unique<ExplosionEffect>(explosionDuration);
        case SkillType::INVINCIBLE:
            return std::make_unique<InvincibleEffect>(invincibleDuration);
        case SkillType::SPLIT:
            return std::make_unique<SplitEffect>();
        default:
            return nullptr;
    }
}