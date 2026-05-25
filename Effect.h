// Effect.h (完整文件)
#ifndef EFFECT_H
#define EFFECT_H

#include "raylib.h"
#include <string>
#include <memory>

class Game;

enum class EffectType {
    PaddleExtend,
    BallEnlarge,
    BallShrink,
    Explosion,
    Invincible,
    Split
};

class Effect {
public:
    Effect(float duration, const std::string& name = "Effect");
    virtual ~Effect() = default;
    
    virtual void Apply(Game* game) = 0;
    virtual void Revert(Game* game) = 0;
    virtual bool Update(float dt) { timer -= dt; return timer > 0.0f; }
    virtual EffectType GetType() const = 0;
    
    float GetRemainingTime() const { return timer; }
    const std::string& GetName() const { return name; }
    
    // ★ 新增：允许外部设置剩余时间（用于网络同步）
    void SetRemainingTime(float t) { timer = t; }
    
protected:
    float timer;
    std::string name;
};

// 以下各具体效果类保持不变...
class PaddleExtendEffect : public Effect {
public:
    PaddleExtendEffect(float duration, float factor);
    void Apply(Game* game) override;
    void Revert(Game* game) override;
    EffectType GetType() const override { return EffectType::PaddleExtend; }
private:
    float extendFactor;
};

class BallEnlargeEffect : public Effect {
public:
    BallEnlargeEffect(float duration, float factor);
    void Apply(Game* game) override;
    void Revert(Game* game) override;
    EffectType GetType() const override { return EffectType::BallEnlarge; }
private:
    float enlargeFactor;
};

class BallShrinkEffect : public Effect {
public:
    BallShrinkEffect(float duration, float factor);
    void Apply(Game* game) override;
    void Revert(Game* game) override;
    EffectType GetType() const override { return EffectType::BallShrink; }
private:
    float shrinkFactor;
};

class ExplosionEffect : public Effect {
public:
    ExplosionEffect(float duration);
    void Apply(Game* game) override;
    void Revert(Game* game) override;
    bool Update(float dt) override;
    EffectType GetType() const override { return EffectType::Explosion; }
};

class InvincibleEffect : public Effect {
public:
    InvincibleEffect(float duration);
    void Apply(Game* game) override;
    void Revert(Game* game) override;
    EffectType GetType() const override { return EffectType::Invincible; }
};

class SplitEffect : public Effect {
public:
    SplitEffect();
    void Apply(Game* game) override;
    void Revert(Game* game) override {}
    bool Update(float dt) override { return false; }
    EffectType GetType() const override { return EffectType::Split; }
};

#endif