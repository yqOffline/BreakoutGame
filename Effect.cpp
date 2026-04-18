#include "Effect.h"
#include "Game.h"
#include "Ball.h"
#include "Paddle.h"

// ---------- Effect 基类 ----------
Effect::Effect(float duration, const std::string& name)
    : timer(duration), name(name) {}

// ---------- PaddleExtendEffect ----------
PaddleExtendEffect::PaddleExtendEffect(float duration, float factor)
    : Effect(duration, "Paddle Extend"), extendFactor(factor) {}

void PaddleExtendEffect::Apply(Game* game) {
    float originalWidth = game->GetOriginalPaddleWidth();
    game->GetPaddle().SetWidth(originalWidth * extendFactor);
}

void PaddleExtendEffect::Revert(Game* game) {
    game->GetPaddle().SetWidth(game->GetOriginalPaddleWidth());
}

// ---------- BallEnlargeEffect ----------
BallEnlargeEffect::BallEnlargeEffect(float duration, float factor)
    : Effect(duration, "Ball Enlarge"), enlargeFactor(factor) {}

void BallEnlargeEffect::Apply(Game* game) {
    float originalRadius = game->GetOriginalBallRadius();
    for (auto& ball : game->GetBalls()) {
        ball.SetRadius(originalRadius * enlargeFactor);
    }
}

void BallEnlargeEffect::Revert(Game* game) {
    float originalRadius = game->GetOriginalBallRadius();
    for (auto& ball : game->GetBalls()) {
        ball.SetRadius(originalRadius);
    }
}

// ---------- BallShrinkEffect ----------
BallShrinkEffect::BallShrinkEffect(float duration, float factor)
    : Effect(duration, "Ball Shrink"), shrinkFactor(factor) {}

void BallShrinkEffect::Apply(Game* game) {
    float originalRadius = game->GetOriginalBallRadius();
    for (auto& ball : game->GetBalls()) {
        ball.SetRadius(originalRadius * shrinkFactor);
    }
}

void BallShrinkEffect::Revert(Game* game) {
    float originalRadius = game->GetOriginalBallRadius();
    for (auto& ball : game->GetBalls()) {
        ball.SetRadius(originalRadius);
    }
}

// ---------- ExplosionEffect ----------
ExplosionEffect::ExplosionEffect(float duration)
    : Effect(duration, "Explosion") {}

void ExplosionEffect::Apply(Game* game) {
    // 实际效果在砖块碰撞时处理（球造成双倍伤害），此处仅标记状态
    // Game类中会检查 activeEffect 是否为 ExplosionEffect 来决定伤害倍数
}

void ExplosionEffect::Revert(Game* game) {
    // 无额外恢复操作
}

bool ExplosionEffect::Update(float dt) {
    timer -= dt;
    return timer > 0.0f;
}

// ---------- InvincibleEffect ----------
InvincibleEffect::InvincibleEffect(float duration)
    : Effect(duration, "Invincible") {}

void InvincibleEffect::Apply(Game* game) {
    // 效果在红线碰撞检测时生效
}

void InvincibleEffect::Revert(Game* game) {
    // 无额外恢复操作
}

// ---------- SplitEffect ----------
SplitEffect::SplitEffect()
    : Effect(0.0f, "Split") {}

void SplitEffect::Apply(Game* game) {
    // 分裂逻辑：复制当前所有球，新球速度 Y 分量取反
    std::vector<Ball>& balls = game->GetBalls();
    size_t count = balls.size();
    for (size_t i = 0; i < count; ++i) {
        Ball& original = balls[i];
        Vector2 pos = original.GetPosition();
        Vector2 sp = original.GetSpeed();
        float r = original.GetRadius();
        Ball newBall(pos, Vector2{sp.x, -sp.y}, r);
        balls.push_back(newBall);
        game->AddBallTrail();  // 需要在 Game 中提供添加拖尾队列的方法
    }
}