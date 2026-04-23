#include "RacePlayer.h"
#include "EffectFactory.h"
#include <algorithm>
#include <cmath>

RacePlayer::RacePlayer(int sw, int sh, const json& cfg, bool left)
    : screenWidth(sw), screenHeight(sh), isLeftSide(left), config(cfg),
      score(0), hearts(cfg["game"]["initial_hearts"]),
      state(RacePlayerState::PLAYING), timerRunning(false),
      gameTimer(0.0f), totalDeaths(0), opponentPaddleX(0.0f), hasOpponentPaddle(false)
{
    // 暂不加载关卡，等待种子设置后由外部调用 LoadLevel
    float br = config["ball"]["radius"];
    float spx = config["ball"]["speed_x"];
    float spy = config["ball"]["speed_y"];
    balls.emplace_back(Vector2{0,0}, Vector2{spx, spy}, br);
    ballTrails.emplace_back();

    float pw = config["paddle"]["width"];
    float ph = config["paddle"]["height"];
    float px = (screenWidth - pw) / 2.0f;      // 在自己的半屏居中
    float py = config["paddle"]["y_pos"];
    paddle = Paddle(px, py, pw, ph);
    paddleMoveSpeed = config["paddle"]["speed"];

    levelManager.LoadConfig(config);

    skillDropChance = config["skill_ball"].value("drop_chance", 0.3f);
    skillBallSpeedY = config["skill_ball"].value("speed_y", 45.0f);
    skillBallRadius = config["skill_ball"].value("radius", 6.0f);
    particlesPerBrick = config["particles"].value("count_per_brick", 12);
    particleGravity = config["particles"].value("gravity", 300.0f);
    maxTrailLength = config["trail"].value("max_length", 10);

    totalLevels = levelManager.GetLevelCount();
    originalPaddleWidth = paddle.GetWidth();
    originalBallRadius = br;

    redLine = { 0.0f, paddle.GetRectangle().y + paddle.GetRectangle().height + 5.0f,
                (float)screenWidth, 3.0f };
}

RacePlayer::~RacePlayer() {}

void RacePlayer::LoadLevel(int index) {
    if (index >= totalLevels) {
        state = RacePlayerState::VICTORY;
        timerRunning = false;
        return;
    }
    currentLevel = index;
    hearts = config["game"].value("initial_hearts", 3);

    float brickWidth, startX;
    levelManager.LoadLevel(index, bricks, brickWidth, startX, screenWidth, screenHeight);

    balls.clear();
    ballTrails.clear();
    Vector2 initPos = { (float)config["ball"]["init_x"], (float)config["ball"]["init_y"] };
    Vector2 initSpeed = { 0.0f, 0.0f };
    float initRadius = config["ball"]["radius"];
    balls.emplace_back(initPos, initSpeed, initRadius);
    ballTrails.emplace_back();

    skillBalls.clear();
    particleSystem.Clear();

    for (auto& effect : activeEffects) effect->Revert(nullptr);
    activeEffects.clear();
    paddle.SetWidth(originalPaddleWidth);
    for (auto& ball : balls) ball.SetRadius(originalBallRadius);
}

void RacePlayer::ResetForNewGame() {
    currentLevel = 0;
    LoadLevel(0);
    score = 0;
    hearts = config["game"]["initial_hearts"];
    totalDeaths = 0;
    gameTimer = 0.0f;
    timerRunning = false;
    state = RacePlayerState::PLAYING;
    opponentPaddleX = 0.0f;
    hasOpponentPaddle = false;

    balls.clear();
    ballTrails.clear();
    Vector2 initPos = { (float)config["ball"]["init_x"], (float)config["ball"]["init_y"] };
    Vector2 initSpeed = { 0.0f, 0.0f };
    float initRadius = config["ball"]["radius"];
    balls.emplace_back(initPos, initSpeed, initRadius);
    ballTrails.emplace_back();

    skillBalls.clear();
    particleSystem.Clear();
    for (auto& effect : activeEffects) effect->Revert(nullptr);
    activeEffects.clear();
    paddle.SetWidth(originalPaddleWidth);
    for (auto& ball : balls) ball.SetRadius(originalBallRadius);
}

void RacePlayer::SetRandomSeed(unsigned int seed) {
    levelManager.SetSeed(seed);
}

void RacePlayer::StartGame() {
    for (auto& b : balls) b.SetSpeed({ config["ball"]["speed_x"], config["ball"]["speed_y"] });
    timerRunning = true;
    state = RacePlayerState::PLAYING;
}

void RacePlayer::SetPaused(bool paused) {
    if (paused && state == RacePlayerState::PLAYING) {
        state = RacePlayerState::PAUSED;
        timerRunning = false;
    } else if (!paused && state == RacePlayerState::PAUSED) {
        state = RacePlayerState::PLAYING;
        timerRunning = true;
    }
}

void RacePlayer::ForceGameOver() {
    state = RacePlayerState::GAME_OVER;
    timerRunning = false;
}

void RacePlayer::ForceVictory() {
    state = RacePlayerState::VICTORY;
    timerRunning = false;
}

void RacePlayer::SetOpponentPaddleX(float x) {
    opponentPaddleX = x;
    hasOpponentPaddle = true;
    // 直接将右侧玩家的挡板移动到远端位置，用于镜像
    paddle.SetPosition(x, paddle.GetRectangle().y);
}

// ★ 关键：本地挡板移动，使用自己的 screenWidth 作为边界
void RacePlayer::HandleInput() {
    if (state != RacePlayerState::PLAYING) return;

    float newX = paddle.GetRectangle().x;
    if (IsKeyDown(KEY_LEFT)||IsKeyDown(KEY_A))  newX -= paddleMoveSpeed;
    if (IsKeyDown(KEY_RIGHT)||IsKeyDown(KEY_D)) newX += paddleMoveSpeed;

    if (newX < 0) newX = 0;
    float maxX = screenWidth - paddle.GetRectangle().width;
    if (newX > maxX) newX = maxX;

    paddle.SetPosition(newX, paddle.GetRectangle().y);
}

void RacePlayer::CheckBallHitRedLine() {
    std::vector<size_t> ballsToRemove;
    for (size_t i = 0; i < balls.size(); ++i) {
        Ball& ball = balls[i];
        if (CheckCollisionCircleRec(ball.GetPosition(), ball.GetRadius(), redLine)) {
            if (HasEffectOfType("Invincible")) {
                Vector2 sp = ball.GetSpeed();
                sp.y *= -1;
                ball.SetSpeed(sp);
                ball.SetPosition({ ball.GetPosition().x, redLine.y - ball.GetRadius() });
                continue;
            }
            ballsToRemove.push_back(i);
            particleSystem.EmitExplosion(ball.GetPosition(), RED, 15);
        }
    }
    if (ballsToRemove.empty()) return;

    for (auto it = ballsToRemove.rbegin(); it != ballsToRemove.rend(); ++it) {
        balls.erase(balls.begin() + *it);
        ballTrails.erase(ballTrails.begin() + *it);
    }

    if (balls.empty()) {
        hearts--;
        totalDeaths++;
        if (hearts <= 0) {
            state = RacePlayerState::GAME_OVER;
            timerRunning = false;
            soundManager.PlayGameOver();
        } else {
            // ★ 不暂停，立刻发射新球继续游戏
            float paddleCenterX = paddle.GetRectangle().x + paddle.GetRectangle().width / 2;
            float paddleTopY = paddle.GetRectangle().y - originalBallRadius - 2;
            balls.emplace_back(Vector2{paddleCenterX, paddleTopY},
                              Vector2{config["ball"]["speed_x"], config["ball"]["speed_y"]},
                              originalBallRadius);
            ballTrails.emplace_back();
        }
    }
}

void RacePlayer::ApplyEffect(std::unique_ptr<Effect> effect) {
    if (!effect) return;
    EffectType newType = effect->GetType();
    for (auto it = activeEffects.begin(); it != activeEffects.end(); ++it) {
        if ((*it)->GetType() == newType) {
            (*it)->Revert(nullptr);
            activeEffects.erase(it);
            break;
        }
    }
    effect->Apply(nullptr);
    activeEffects.push_back(std::move(effect));
}

void RacePlayer::UpdateEffects(float dt) {
    for (auto it = activeEffects.begin(); it != activeEffects.end(); ) {
        bool stillActive = (*it)->Update(dt);
        if (!stillActive) {
            soundManager.PlayPowerupEnd();
            (*it)->Revert(nullptr);
            it = activeEffects.erase(it);
        } else {
            ++it;
        }
    }
}

bool RacePlayer::HasEffectOfType(const std::string& typeName) const {
    for (const auto& effect : activeEffects)
        if (effect->GetName() == typeName) return true;
    return false;
}

void RacePlayer::HandleBallCollisions() {
    for (size_t i = 0; i < balls.size(); ++i) {
        for (size_t j = i + 1; j < balls.size(); ++j) {
            Ball& a = balls[i];
            Ball& b = balls[j];
            Vector2 posA = a.GetPosition();
            Vector2 posB = b.GetPosition();
            float radiusSum = a.GetRadius() + b.GetRadius();
            float dx = posA.x - posB.x;
            float dy = posA.y - posB.y;
            float dist = sqrtf(dx*dx + dy*dy);
            if (dist < radiusSum && dist > 0.001f) {
                soundManager.PlayHitSound();
                Vector2 spA = a.GetSpeed();
                Vector2 spB = b.GetSpeed();
                Vector2 normal = { dx / dist, dy / dist };
                Vector2 tangent = { -normal.y, normal.x };
                float v1n = normal.x * spA.x + normal.y * spA.y;
                float v1t = tangent.x * spA.x + tangent.y * spA.y;
                float v2n = normal.x * spB.x + normal.y * spB.y;
                float v2t = tangent.x * spB.x + tangent.y * spB.y;
                float v1nAfter = v2n;
                float v2nAfter = v1n;
                Vector2 newSpA = { normal.x * v1nAfter + tangent.x * v1t,
                                   normal.y * v1nAfter + tangent.y * v1t };
                Vector2 newSpB = { normal.x * v2nAfter + tangent.x * v2t,
                                   normal.y * v2nAfter + tangent.y * v2t };
                a.SetSpeed(newSpA);
                b.SetSpeed(newSpB);
                float overlap = radiusSum - dist;
                Vector2 separation = { normal.x * overlap * 0.5f, normal.y * overlap * 0.5f };
                a.SetPosition({ posA.x + separation.x, posA.y + separation.y });
                b.SetPosition({ posB.x - separation.x, posB.y - separation.y });
            }
        }
    }
}

void RacePlayer::CheckLevelTransition() {
    bool allInactive = true;
    for (const auto& b : bricks) {
        if (b.IsActive()) { allInactive = false; break; }
    }
    if (allInactive && state == RacePlayerState::PLAYING) {
        if (currentLevel + 1 < totalLevels) {
            state = RacePlayerState::LEVEL_CLEAR;
            soundManager.PlayLevelComplete();
            for (auto& b : balls) b.SetSpeed({0, 0});
        } else {
            state = RacePlayerState::VICTORY;
            timerRunning = false;
            soundManager.PlayGameVictory();
            for (auto& b : balls) b.SetSpeed({0, 0});
        }
    }
}

void RacePlayer::Update(float dt) {
    // 只有在 PLAYING 且未暂停时才走时
    if (state == RacePlayerState::PLAYING && timerRunning)
        gameTimer += dt;

    if (state != RacePlayerState::PLAYING) return;

    // 拖尾
    for (size_t i = 0; i < balls.size(); ++i) {
        ballTrails[i].push_back(balls[i].GetPosition());
        if ((int)ballTrails[i].size() > maxTrailLength) ballTrails[i].pop_front();
    }

    // 球移动与边界/挡板碰撞
    for (auto& ball : balls) {
        ball.Move();
        if (ball.BounceEdge(screenWidth, screenHeight))
            soundManager.PlayHitSound();
        if (ball.CheckCollisionPaddle(paddle))
            soundManager.PlayHitSound();
    }

    // 砖块碰撞
    for (auto& ball : balls) {
        for (auto& brick : bricks) {
            if (brick.IsActive() && CheckCollisionCircleRec(ball.GetPosition(), ball.GetRadius(), brick.GetRectangle())) {
                soundManager.PlayHitSound();
                int damage = HasEffectOfType("Explosion") ? 2 : 1;
                bool destroyed = false;
                for (int d = 0; d < damage; ++d) {
                    if (brick.TakeDamage()) { destroyed = true; break; }
                }
                if (destroyed) {
                    score++;
                    particleSystem.EmitBrickBreak(brick.GetRectangle(), brick.GetColor(), particlesPerBrick);
                    if (brick.ShouldDropSkill(skillDropChance)) {
                        Vector2 spawnPos = { brick.GetRectangle().x + brick.GetRectangle().width / 2,
                                             brick.GetRectangle().y + brick.GetRectangle().height / 2 };
                        SkillType type = static_cast<SkillType>(GetRandomValue(0, 5));
                        Color glow = WHITE;
                        switch (type) {
                            case SkillType::PADDLE_EXTEND: glow = BLUE; break;
                            case SkillType::BALL_ENLARGE:  glow = GREEN; break;
                            case SkillType::BALL_SHRINK:   glow = RED; break;
                            case SkillType::EXPLOSION:     glow = ORANGE; break;
                            case SkillType::INVINCIBLE:    glow = GOLD; break;
                            case SkillType::SPLIT:         glow = SKYBLUE; break;
                        }
                        skillBalls.emplace_back(spawnPos, type, skillBallRadius, Vector2{0, skillBallSpeedY}, glow);
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
    }

    HandleBallCollisions();

    for (auto& sb : skillBalls) {
        sb.Update(dt);
        if (sb.active && CheckCollisionCircleRec(sb.GetPosition(), sb.GetRadius(), paddle.GetRectangle())) {
            soundManager.PlayPowerupGet();
            auto effect = EffectFactory::CreateEffect(sb.skillType);
            if (effect) ApplyEffect(std::move(effect));
            sb.active = false;
        }
    }
    skillBalls.erase(std::remove_if(skillBalls.begin(), skillBalls.end(),
        [](const SkillBall& sb) { return !sb.active; }), skillBalls.end());

    particleSystem.Update(dt, particleGravity);
    UpdateEffects(dt);
    CheckBallHitRedLine();
    CheckLevelTransition();
}

void RacePlayer::Draw() {
    // 拖尾
    for (size_t i = 0; i < balls.size(); ++i) {
        for (size_t j = 0; j < ballTrails[i].size(); ++j) {
            float alpha = 0.3f * (float)j / ballTrails[i].size();
            float size = balls[i].GetRadius() * (0.5f + 0.5f * j / ballTrails[i].size());
            DrawCircleV(ballTrails[i][j], size, Fade(RED, alpha));
        }
    }
    for (auto& b : balls) b.Draw();
    paddle.Draw();

    for (auto& b : bricks) b.Draw();
    for (auto& sb : skillBalls) sb.Draw();
    particleSystem.Draw();
    DrawRectangleRec(redLine, RED);

    DrawText(TextFormat("SCORE: %d", score), 10, 10, 20, BLUE);
    DrawText(TextFormat("LIVES: %d", hearts), 10, 40, 20, RED);
    DrawText(TextFormat("LEVEL: %d", currentLevel + 1), 10, 70, 20, DARKGREEN);
    DrawText(TextFormat("TIME: %.1f", gameTimer), 10, 100, 20, DARKPURPLE);
    DrawText(TextFormat("DEATHS: %d", totalDeaths), 10, 130, 20, MAROON);

    int effectY = 160;
    for (const auto& effect : activeEffects) {
        DrawText(TextFormat("%s: %.1fs", effect->GetName().c_str(), effect->GetRemainingTime()), 10, effectY, 20, GREEN);
        effectY += 20;
    }
}