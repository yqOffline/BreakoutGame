#include "VersusGame.h"
#include "VersusNetMessage.h"
#include "EffectFactory.h"
#include <algorithm>
#include <cmath>

Color VersusGame::skillBallColorForType(SkillType type) const {
    switch (type) {
        case SkillType::PADDLE_EXTEND: return BLUE;
        case SkillType::BALL_ENLARGE:  return GREEN;
        case SkillType::BALL_SHRINK:   return RED;
        case SkillType::EXPLOSION:     return ORANGE;
        case SkillType::INVINCIBLE:    return GOLD;
        case SkillType::SPLIT:         return SKYBLUE;
        default: return WHITE;
    }
}

VersusGame::VersusGame(int sw, int sh, const json& cfg)
    : screenWidth(sw), screenHeight(sh), config(cfg),
      ball({sw/2.0f, sh/2.0f}, {0,0}, cfg["ball"]["radius"].get<float>()),
      upperPaddle(sw/2.0f - cfg["paddle"]["width"].get<float>()/2.0f,
                  50,
                  cfg["paddle"]["width"].get<float>(),
                  cfg["paddle"]["height"].get<float>()),
      lowerPaddle(sw/2.0f - cfg["paddle"]["width"].get<float>()/2.0f,
                  sh - 50 - cfg["paddle"]["height"].get<float>(),
                  cfg["paddle"]["width"].get<float>(),
                  cfg["paddle"]["height"].get<float>()),
      ballColor(WHITE),
      upperRedLine({0, 0, (float)sw, 3.0f}),
      lowerRedLine({0, sh - 3.0f, (float)sw, 3.0f}),
      upperLives(cfg["game"]["initial_hearts"].get<int>()),
      lowerLives(cfg["game"]["initial_hearts"].get<int>()),
      waitingForLaunch(true),
      ballAttachedToUpper(true),
      rng(std::random_device{}())
{
    ballTrails.emplace_back();

    ballInitSpeedX = cfg["ball"]["speed_x"].get<float>();
    ballInitSpeedY = cfg["ball"]["speed_y"].get<float>();
    paddleMoveSpeed = cfg["paddle"]["speed"].get<float>();
    skillBallSpeedY = cfg["skill_ball"]["speed_y"].get<float>();
    skillBallRadius = cfg["skill_ball"]["radius"].get<float>();
    skillDropChance = cfg["skill_ball"].value("drop_chance", 0.3f);
    particlesPerBrick = cfg["particles"].value("count_per_brick", 12);
    particleGravity = cfg["particles"].value("gravity", 300.0f);
    maxTrailLength = cfg["trail"].value("max_length", 10);

    ballColor = BLUE;
    ball.SetPosition({ upperPaddle.GetRectangle().x + upperPaddle.GetRectangle().width/2,
                       upperPaddle.GetRectangle().y + upperPaddle.GetRectangle().height + ball.GetRadius() });
}

void VersusGame::LoadLevel(unsigned int seed) {
    rng.seed(seed);

    bricks.clear();
    skillBalls.clear();
    particleSystem.Clear();
    ballTrails[0].clear();

    const int cols = 7, rows = 4;
    const float brickHeight = 50.0f;
    const float spacing = 15.0f;
    const float margin = 80.0f;

    float bw = (screenWidth - 2 * margin - (cols - 1) * spacing) / cols;
    bw *= 1.2f;

    float totalBricksHeight = rows * brickHeight + (rows - 1) * spacing;
    float startY = (screenHeight - totalBricksHeight) / 2.0f;
    float startX = (screenWidth - (cols * bw + (cols - 1) * spacing)) / 2.0f;

    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            float x = startX + c * (bw + spacing);
            float y = startY + r * (brickHeight + spacing);
            int hp = (r == 0 || r == 3) ? 1 : 2;
            bricks.emplace_back(x, y, bw, brickHeight, hp);
        }
    }

    int invincibleCount = 6;
    std::vector<int> candidates;
    for (int i = 7; i < 21; ++i) candidates.push_back(i);
    std::shuffle(candidates.begin(), candidates.end(), rng);
    for (int i = 0; i < invincibleCount; ++i) {
        int idx = candidates[i];
        bricks[idx].SetInvincible(true);
        bricks[idx].SetActive(true);
    }

    waitingForLaunch = true;
    ballAttachedToUpper = true;
    ball.SetPosition({ upperPaddle.GetRectangle().x + upperPaddle.GetRectangle().width/2,
                       upperPaddle.GetRectangle().y + upperPaddle.GetRectangle().height + ball.GetRadius() });
    ball.SetSpeed({0, 0});
    ballColor = BLUE;
    ballTrails[0].clear();
}

void VersusGame::MovePaddle(bool isUpper, int direction) {
    float dx = direction * paddleMoveSpeed;
    if (isUpper) {
        float newX = upperPaddle.GetRectangle().x + dx;
        if (newX < 0) newX = 0;
        float maxX = screenWidth - upperPaddle.GetRectangle().width;
        if (newX > maxX) newX = maxX;
        upperPaddle.SetPosition(newX, upperPaddle.GetRectangle().y);
    } else {
        float newX = lowerPaddle.GetRectangle().x + dx;
        if (newX < 0) newX = 0;
        float maxX = screenWidth - lowerPaddle.GetRectangle().width;
        if (newX > maxX) newX = maxX;
        lowerPaddle.SetPosition(newX, lowerPaddle.GetRectangle().y);
    }
}

void VersusGame::TryLaunchBall(bool isUpper) {
    if (!waitingForLaunch) return;
    if ((isUpper && ballAttachedToUpper) || (!isUpper && !ballAttachedToUpper)) {
        Vector2 sp = { ballInitSpeedX, ballInitSpeedY };
        if (isUpper) sp.y = fabs(ballInitSpeedY);
        else         sp.y = -fabs(ballInitSpeedY);
        ball.SetSpeed(sp);
        waitingForLaunch = false;
        ballTrails[0].clear();
        if (OnBallLaunched) OnBallLaunched(isUpper);
        soundManager.PlayHitSound();
    }
}

void VersusGame::Update(float dt) {
    UpdateEffects(dt);

    for (auto& sb : skillBalls) sb.Update(dt);

    if (waitingForLaunch) {
        if (ballAttachedToUpper)
            ball.SetPosition({ upperPaddle.GetRectangle().x + upperPaddle.GetRectangle().width/2,
                               upperPaddle.GetRectangle().y + upperPaddle.GetRectangle().height + ball.GetRadius() });
        else
            ball.SetPosition({ lowerPaddle.GetRectangle().x + lowerPaddle.GetRectangle().width/2,
                               lowerPaddle.GetRectangle().y - ball.GetRadius() });
        skillBalls.erase(std::remove_if(skillBalls.begin(), skillBalls.end(),
            [](const SkillBall& sb) { return !sb.active; }), skillBalls.end());
        particleSystem.Update(dt, particleGravity);
        return;
    }

    ballTrails[0].push_back(ball.GetPosition());
    if ((int)ballTrails[0].size() > maxTrailLength)
        ballTrails[0].pop_front();

    ball.Move();
    HandleBallEdgeBounce();
    HandlePaddleCollision(upperPaddle, true);
    HandlePaddleCollision(lowerPaddle, false);
    HandleBrickCollision();
    HandleRedLine();

    for (auto& sb : skillBalls) {
        sb.Update(dt);
        if (sb.active && CheckCollisionCircleRec(sb.GetPosition(), sb.GetRadius(),
                                                  upperPaddle.GetRectangle())) {
            soundManager.PlayPowerupGet();
            auto effect = EffectFactory::CreateEffect(sb.skillType);
            EffectType etype = effect ? effect->GetType() : EffectType::PaddleExtend;
            if (effect) {
                ApplyEffectToPlayer(std::move(effect), true);
                if (OnEffectApplied) OnEffectApplied(etype, true);
            }
            sb.active = false;
        }
        if (sb.active && CheckCollisionCircleRec(sb.GetPosition(), sb.GetRadius(),
                                                  lowerPaddle.GetRectangle())) {
            soundManager.PlayPowerupGet();
            auto effect = EffectFactory::CreateEffect(sb.skillType);
            EffectType etype = effect ? effect->GetType() : EffectType::PaddleExtend;
            if (effect) {
                ApplyEffectToPlayer(std::move(effect), false);
                if (OnEffectApplied) OnEffectApplied(etype, false);
            }
            sb.active = false;
        }
    }
    skillBalls.erase(std::remove_if(skillBalls.begin(), skillBalls.end(),
        [](const SkillBall& sb) { return !sb.active; }), skillBalls.end());

    particleSystem.Update(dt, particleGravity);
}

void VersusGame::HandleBallEdgeBounce() {
    Vector2 pos = ball.GetPosition();
    float r = ball.GetRadius();
    Vector2 sp = ball.GetSpeed();
    if (pos.x - r <= 0 || pos.x + r >= screenWidth) {
        sp.x *= -1;
        ball.SetSpeed(sp);
        soundManager.PlayHitSound();
    }
}

void VersusGame::HandlePaddleCollision(Paddle& paddle, bool isUpper) {
    Rectangle rect = paddle.GetRectangle();
    if (!CheckCollisionCircleRec(ball.GetPosition(), ball.GetRadius(), rect)) return;

    Vector2 sp = ball.GetSpeed();
    if (isUpper && sp.y > 0) return;
    if (!isUpper && sp.y < 0) return;

    sp.y *= -1;
    if (isUpper)
        ball.SetPosition({ ball.GetPosition().x, rect.y + rect.height + ball.GetRadius() });
    else
        ball.SetPosition({ ball.GetPosition().x, rect.y - ball.GetRadius() });

    ballColor = isUpper ? BLUE : RED;

    float hitPos = ball.GetPosition().x - rect.x;
    float normalized = hitPos / rect.width;
    float angle = (normalized - 0.5f) * 1.5f;
    sp.x = 10.0f * angle;
    if (sp.x > -6 && sp.x < 6) sp.x = (sp.x > 0) ? 6 : -6;

    ball.SetSpeed(sp);
    soundManager.PlayHitSound();
}

void VersusGame::HandleBrickCollision() {
    for (auto& brick : bricks) {
        if (!brick.IsActive()) continue;
        if (!CheckCollisionCircleRec(ball.GetPosition(), ball.GetRadius(), brick.GetRectangle())) continue;

        if (brick.IsInvincible()) {
            Vector2 sp = ball.GetSpeed();
            sp.y *= -1;
            ball.SetSpeed(sp);
            if (sp.y > 0)
                ball.SetPosition({ ball.GetPosition().x, brick.GetRectangle().y + brick.GetRectangle().height + ball.GetRadius() });
            else
                ball.SetPosition({ ball.GetPosition().x, brick.GetRectangle().y - ball.GetRadius() });
            soundManager.PlayHitSound();
            break;
        }

        soundManager.PlayHitSound();
        bool destroyed = brick.TakeDamage();
        if (destroyed) {
            particleSystem.EmitBrickBreak(brick.GetRectangle(), brick.GetColor(), particlesPerBrick);
            bool belongsToUpper = (ballColor.r == 0 && ballColor.g == 0 && ballColor.b == 255);
            if (brick.ShouldDropSkill(skillDropChance)) {
                Vector2 spawnPos = { brick.GetRectangle().x + brick.GetRectangle().width/2,
                                     brick.GetRectangle().y + brick.GetRectangle().height/2 };
                SkillType type = static_cast<SkillType>(GetRandomValue(0, 5));
                float dir = belongsToUpper ? -1.0f : 1.0f;
                SkillBall sb(spawnPos, type, skillBallRadius,
                             Vector2{0, skillBallSpeedY * dir},
                             skillBallColorForType(type));
                skillBalls.push_back(sb);
                if (OnSkillBallSpawned) OnSkillBallSpawned(sb);
            }
        }
        Vector2 sp = ball.GetSpeed();
        sp.y *= -1;
        ball.SetSpeed(sp);
        if (sp.y > 0)
            ball.SetPosition({ ball.GetPosition().x, brick.GetRectangle().y + brick.GetRectangle().height + ball.GetRadius() });
        else
            ball.SetPosition({ ball.GetPosition().x, brick.GetRectangle().y - ball.GetRadius() });
        break;
    }
}

void VersusGame::HandleRedLine() {
    if (ball.GetPosition().y - ball.GetRadius() <= upperRedLine.y + upperRedLine.height) {
        upperLives--;
        soundManager.PlayLevelOver();
        waitingForLaunch = true;
        ballAttachedToUpper = true;
        ball.SetPosition({ upperPaddle.GetRectangle().x + upperPaddle.GetRectangle().width/2,
                           upperPaddle.GetRectangle().y + upperPaddle.GetRectangle().height + ball.GetRadius() });
        ball.SetSpeed({0,0});
        ballColor = BLUE;
        ballTrails[0].clear();
        particleSystem.EmitExplosion(ball.GetPosition(), RED, 15);
        if (OnLifeLost) OnLifeLost();
    } else if (ball.GetPosition().y + ball.GetRadius() >= lowerRedLine.y) {
        lowerLives--;
        soundManager.PlayLevelOver();
        waitingForLaunch = true;
        ballAttachedToUpper = false;
        ball.SetPosition({ lowerPaddle.GetRectangle().x + lowerPaddle.GetRectangle().width/2,
                           lowerPaddle.GetRectangle().y - ball.GetRadius() });
        ball.SetSpeed({0,0});
        ballColor = RED;
        ballTrails[0].clear();
        particleSystem.EmitExplosion(ball.GetPosition(), RED, 15);
        if (OnLifeLost) OnLifeLost();
    }
}

void VersusGame::ApplyEffectToPlayer(std::unique_ptr<Effect> effect, bool upper) {
    if (!effect) return;
    EffectType newType = effect->GetType();
    auto& effects = upper ? upperEffects : lowerEffects;
    for (auto it = effects.begin(); it != effects.end(); ++it) {
        if ((*it)->GetType() == newType) {
            if (OnEffectRemoved) OnEffectRemoved(newType, upper);
            effects.erase(it);
            break;
        }
    }
    effects.push_back(std::move(effect));
}

void VersusGame::UpdateEffects(float dt) {
    for (auto it = upperEffects.begin(); it != upperEffects.end(); ) {
        if (!(*it)->Update(dt)) {
            EffectType type = (*it)->GetType();
            if (OnEffectRemoved) OnEffectRemoved(type, true);
            it = upperEffects.erase(it);
        } else ++it;
    }
    for (auto it = lowerEffects.begin(); it != lowerEffects.end(); ) {
        if (!(*it)->Update(dt)) {
            EffectType type = (*it)->GetType();
            if (OnEffectRemoved) OnEffectRemoved(type, false);
            it = lowerEffects.erase(it);
        } else ++it;
    }

    float upperWidthMult = 1.0f, lowerWidthMult = 1.0f;
    float upperBallMult = 1.0f, lowerBallMult = 1.0f;

    for (const auto& eff : upperEffects) {
        switch (eff->GetType()) {
            case EffectType::PaddleExtend: upperWidthMult = std::max(upperWidthMult, 1.5f); break;
            case EffectType::BallEnlarge:  upperBallMult = std::max(upperBallMult, 1.5f); break;
            case EffectType::BallShrink:   upperBallMult = std::min(upperBallMult, 0.7f); break;
            default: break;
        }
    }
    for (const auto& eff : lowerEffects) {
        switch (eff->GetType()) {
            case EffectType::PaddleExtend: lowerWidthMult = std::max(lowerWidthMult, 1.5f); break;
            case EffectType::BallEnlarge:  lowerBallMult = std::max(lowerBallMult, 1.5f); break;
            case EffectType::BallShrink:   lowerBallMult = std::min(lowerBallMult, 0.7f); break;
            default: break;
        }
    }

    static float origUpperWidth = upperPaddle.GetWidth();
    static float origLowerWidth = lowerPaddle.GetWidth();
    static float origBallRadius = ball.GetRadius();

    upperPaddle.SetWidth(origUpperWidth * upperWidthMult);
    lowerPaddle.SetWidth(origLowerWidth * lowerWidthMult);

    float ballMult = (ballColor.r == 0 && ballColor.g == 0 && ballColor.b == 255) ? upperBallMult : lowerBallMult;
    ball.SetRadius(origBallRadius * ballMult);
}

GameStateSnapshot VersusGame::GetSnapshot() const {
    GameStateSnapshot snap;
    snap.ballX = ball.GetPosition().x;
    snap.ballY = ball.GetPosition().y;
    snap.ballSpeedX = ball.GetSpeed().x;
    snap.ballSpeedY = ball.GetSpeed().y;
    snap.upperPaddleX = upperPaddle.GetRectangle().x;
    snap.lowerPaddleX = lowerPaddle.GetRectangle().x;
    snap.upperLives = upperLives;
    snap.lowerLives = lowerLives;
    snap.ballR = ballColor.r;
    snap.ballG = ballColor.g;
    snap.ballB = ballColor.b;
    snap.ballA = ballColor.a;
    snap.ballAttached = waitingForLaunch ? (ballAttachedToUpper ? 1 : 2) : 0;

    uint32_t bits = 0;
    for (size_t i = 0; i < bricks.size(); ++i)
        if (bricks[i].IsActive()) bits |= (1u << i);
    snap.brickActiveBits = bits;
    return snap;
}

void VersusGame::ApplySnapshot(const GameStateSnapshot& snap) {
    ball.SetPosition({ snap.ballX, snap.ballY });
    ball.SetSpeed({ snap.ballSpeedX, snap.ballSpeedY });
    upperPaddle.SetPosition(snap.upperPaddleX, upperPaddle.GetRectangle().y);
    lowerPaddle.SetPosition(snap.lowerPaddleX, lowerPaddle.GetRectangle().y);
    upperLives = snap.upperLives;
    lowerLives = snap.lowerLives;
    ballColor = { snap.ballR, snap.ballG, snap.ballB, snap.ballA };
    waitingForLaunch = (snap.ballAttached != 0);
    ballAttachedToUpper = (snap.ballAttached == 1);

    for (size_t i = 0; i < bricks.size(); ++i)
        bricks[i].SetActive((snap.brickActiveBits & (1u << i)) != 0);
}

void VersusGame::Draw() {
    DrawRectangleRec(upperRedLine, RED);
    DrawRectangleRec(lowerRedLine, RED);

    for (auto& b : bricks) b.Draw();

    for (size_t j = 0; j < ballTrails[0].size(); ++j) {
        float alpha = 0.3f * (float)j / ballTrails[0].size();
        float size = ball.GetRadius() * (0.5f + 0.5f * j / ballTrails[0].size());
        DrawCircleV(ballTrails[0][j], size, Fade(ballColor, alpha));
    }

    DrawCircleV(ball.GetPosition(), ball.GetRadius(), ballColor);
    DrawRectangleRec(upperPaddle.GetRectangle(), BLUE);
    DrawRectangleRec(lowerPaddle.GetRectangle(), RED);

    for (auto& sb : skillBalls) sb.Draw();
    particleSystem.Draw();

    DrawText(TextFormat("UPPER: %d", upperLives), 10, 10, 20, BLUE);
    DrawText(TextFormat("LOWER: %d", lowerLives), screenWidth - 150, 10, 20, RED);

    int y = 40;
    for (const auto& eff : upperEffects) {
        DrawText(TextFormat("UP: %s %.1f", eff->GetName().c_str(), eff->GetRemainingTime()), 10, y, 20, BLUE);
        y += 20;
    }
    y = screenHeight - 60;
    for (const auto& eff : lowerEffects) {
        DrawText(TextFormat("DN: %s %.1f", eff->GetName().c_str(), eff->GetRemainingTime()), 10, y, 20, RED);
        y -= 20;
    }
}