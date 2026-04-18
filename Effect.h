#ifndef EFFECT_H
#define EFFECT_H

#include "raylib.h"
#include <string>
#include <functional>
#include <memory>

class Game;  // 前向声明

// 效果基类
class Effect {
public:
    Effect(float duration, const std::string& name = "Effect");
    virtual ~Effect() = default;
    
    // 应用效果（在激活时调用一次）
    virtual void Apply(Game* game) = 0;
    
    // 撤销效果（在持续时间结束或被覆盖时调用）
    virtual void Revert(Game* game) = 0;
    
    // 每帧更新（可选），返回是否仍有效
    virtual bool Update(float dt) { timer -= dt; return timer > 0.0f; }
    
    float GetRemainingTime() const { return timer; }
    const std::string& GetName() const { return name; }
    
protected:
    float timer;
    std::string name;
};

// 具体效果类
class PaddleExtendEffect : public Effect {
public:
    PaddleExtendEffect(float duration, float factor);
    void Apply(Game* game) override;
    void Revert(Game* game) override;
private:
    float extendFactor;
};

class BallEnlargeEffect : public Effect {
public:
    BallEnlargeEffect(float duration, float factor);
    void Apply(Game* game) override;
    void Revert(Game* game) override;
private:
    float enlargeFactor;
};

class BallShrinkEffect : public Effect {
public:
    BallShrinkEffect(float duration, float factor);
    void Apply(Game* game) override;
    void Revert(Game* game) override;
private:
    float shrinkFactor;
};

class ExplosionEffect : public Effect {
public:
    ExplosionEffect(float duration);
    void Apply(Game* game) override;
    void Revert(Game* game) override;
    bool Update(float dt) override;  // 每帧更新（可留空）
};

class InvincibleEffect : public Effect {
public:
    InvincibleEffect(float duration);
    void Apply(Game* game) override;
    void Revert(Game* game) override;
};

class SplitEffect : public Effect {
public:
    SplitEffect();  // 瞬时效果，duration=0
    void Apply(Game* game) override;
    void Revert(Game* game) override {}
    bool Update(float dt) override { return false; } // 瞬时结束
};

#endif // EFFECT_H