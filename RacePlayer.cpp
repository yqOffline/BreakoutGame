#include "RacePlayer.h"
#include "EffectFactory.h"
#include <algorithm>
#include <cmath>

// ======================== 构造函数 ========================
RacePlayer::RacePlayer(int sw, int sh, const json& cfg, bool left)
    : screenWidth(sw), screenHeight(sh), isLeftSide(left), config(cfg),
      score(0), hearts(cfg["game"]["initial_hearts"]),
      state(RacePlayerState::PLAYING), timerRunning(false),
      gameTimer(0.0f), totalDeaths(0), opponentPaddleX(0.0f), hasOpponentPaddle(false)
{
    // 从配置读取球参数
    float bx = config["ball"]["init_x"];
    float by = config["ball"]["init_y"];
    float br = config["ball"]["radius"];
    float spx = config["ball"]["speed_x"];
    float spy = config["ball"]["speed_y"];
    balls.emplace_back(Vector2{bx, by}, Vector2{spx, spy}, br);
    ballTrails.emplace_back();

    // 初始化板
    float pw = config["paddle"]["width"];
    float ph = config["paddle"]["height"];
    float px = (screenWidth - pw) / 2.0f;
    float py = config["paddle"]["y_pos"];
    paddle = Paddle(px, py, pw, ph);
    paddleMoveSpeed = config["paddle"]["speed"];

    // 初始化关卡管理器
    levelManager.LoadConfig(config);

    // 读取配置项
    skillDropChance = config["skill_ball"].value("drop_chance", 0.3f);
    skillBallSpeedY = config["skill_ball"].value("speed_y", 45.0f);
    skillBallRadius = config["skill_ball"].value("radius", 6.0f);
    particlesPerBrick = config["particles"].value("count_per_brick", 12);
    particleGravity = config["particles"].value("gravity", 300.0f);
    maxTrailLength = config["trail"].value("max_length", 10);

    totalLevels = levelManager.GetLevelCount();
    originalPaddleWidth = paddle.GetWidth();
    originalBallRadius = balls[0].GetRadius();

    // 红线位置
    redLine = { 0.0f, paddle.GetRectangle().y + paddle.GetRectangle().height + 5.0f,
                (float)screenWidth, 3.0f };

    // 加载第一关
    LoadLevel(0);
}

RacePlayer::~RacePlayer() {}

// ======================== 重置为新游戏 ========================
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

// ======================== 设置随机种子（供网络同步） ========================
void RacePlayer::SetRandomSeed(unsigned int seed) {
    // 由于 LevelManager 内部使用 std::mt19937，这里需要调用其设置种子的方法
    // 若 LevelManager 未提供，则在此处不做处理（竞速模式中主机将种子发送后，客户端应在 LoadLevel 前设置）
    // 简单起见，我们假设 LevelManager 提供了 SetSeed 方法。
    levelManager.SetSeed(seed);
}

// ======================== 加载关卡 ========================
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

    // 清除所有效果
    for (auto& effect : activeEffects) effect->Revert(nullptr);
    activeEffects.clear();
    paddle.SetWidth(originalPaddleWidth);
    for (auto& ball : balls) ball.SetRadius(originalBallRadius);
}

// ======================== 开始游戏（给球初速度） ========================
void RacePlayer::StartGame() {
    for (auto& b : balls) b.SetSpeed({ config["ball"]["speed_x"], config["ball"]["speed_y"] });
    timerRunning = true;
    state = RacePlayerState::PLAYING;
}

// ======================== 设置暂停状态 ========================
void RacePlayer::SetPaused(bool paused) {
    if (paused && state == RacePlayerState::PLAYING) {
        state = RacePlayerState::PAUSED;
        timerRunning = false;
    } else if (!paused && state == RacePlayerState::PAUSED) {
        state = RacePlayerState::PLAYING;
        timerRunning = true;
    }
}

// ======================== 强制结束（对方退出时调用） ========================
void RacePlayer::ForceGameOver() {
    state = RacePlayerState::GAME_OVER;
    timerRunning = false;
}

// ======================== 强制胜利（用于显示） ========================
void RacePlayer::ForceVictory() {
    state = RacePlayerState::VICTORY;
    timerRunning = false;
}

// ======================== 设置对手板位置（仅用于绘制） ========================
void RacePlayer::SetOpponentPaddleX(float x) {
    opponentPaddleX = x;
    hasOpponentPaddle = true;
    // 直接设置对手板的实际位置，以便物理计算正确
    paddle.SetPosition(x, paddle.GetRectangle().y);
}

// ======================== 处理本地玩家输入 ========================
void RacePlayer::HandleInput() {
    if (state != RacePlayerState::PLAYING) return;
    if (IsKeyDown(KEY_LEFT))  paddle.MoveLeft(paddleMoveSpeed);
    if (IsKeyDown(KEY_RIGHT)) paddle.MoveRight(paddleMoveSpeed);
}

// ======================== 红线碰撞检测 ========================
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

    // 从后向前删除，避免索引错乱
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
            state = RacePlayerState::PAUSED;
            soundManager.PlayLevelOver();

            // 在板中央重新生成球
            float paddleCenterX = paddle.GetRectangle().x + paddle.GetRectangle().width / 2;
            float paddleTopY = paddle.GetRectangle().y - originalBallRadius - 2;
            balls.emplace_back(Vector2{paddleCenterX, paddleTopY},
                              Vector2{config["ball"]["speed_x"], config["ball"]["speed_y"]},
                              originalBallRadius);
            ballTrails.emplace_back();
        }
    }
}

// ======================== 应用效果 ========================
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
    
    effect->Apply(nullptr);   // 注意：原 Apply 需要 Game* 参数，这里传入 nullptr，需修改 Effect 类使其支持 nullptr
    activeEffects.push_back(std::move(effect));
}

// ======================== 更新效果 ========================
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

// ======================== 判断是否拥有某效果 ========================
bool RacePlayer::HasEffectOfType(const std::string& typeName) const {
    for (const auto& effect : activeEffects) {
        if (effect->GetName() == typeName) return true;
    }
    return false;
}

// ======================== 球间碰撞处理 ========================
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

// ======================== 检查关卡过渡 ========================
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

// ======================== 更新逻辑 ========================
void RacePlayer::Update(float dt) {
    if (state == RacePlayerState::PLAYING && timerRunning)
        gameTimer += dt;

    if (state != RacePlayerState::PLAYING) return;

    // 1. 拖尾记录
    for (size_t i = 0; i < balls.size(); ++i) {
        ballTrails[i].push_back(balls[i].GetPosition());
        if ((int)ballTrails[i].size() > maxTrailLength) ballTrails[i].pop_front();
    }

    // 2. 球移动与边界/板碰撞
    for (auto& ball : balls) {
        ball.Move();
        if (ball.BounceEdge(screenWidth, screenHeight)) {
            soundManager.PlayHitSound();
        }
        if (ball.CheckCollisionPaddle(paddle)) {
            soundManager.PlayHitSound();
        }
    }

    // 3. 砖块碰撞处理
    for (auto& ball : balls) {
        for (auto& brick : bricks) {
            if (brick.IsActive() && CheckCollisionCircleRec(ball.GetPosition(), ball.GetRadius(), brick.GetRectangle())) {
                soundManager.PlayHitSound();
                int damage = HasEffectOfType("Explosion") ? 2 : 1;
                bool destroyed = false;
                for (int d = 0; d < damage; ++d) {
                    if (brick.TakeDamage()) {
                        destroyed = true;
                        break;
                    }
                }
                if (destroyed) {
                    score++;
                    particleSystem.EmitBrickBreak(brick.GetRectangle(), brick.GetColor(), particlesPerBrick);
                    if (brick.ShouldDropSkill(skillDropChance)) {
                        Vector2 spawnPos = { brick.GetRectangle().x + brick.GetRectangle().width / 2,
                                             brick.GetRectangle().y + brick.GetRectangle().height / 2 };
                        SkillType type = static_cast<SkillType>(GetRandomValue(0, 5));
                        Color glowColor = WHITE;
                        switch (type) {
                            case SkillType::PADDLE_EXTEND: glowColor = BLUE; break;
                            case SkillType::BALL_ENLARGE:  glowColor = GREEN; break;
                            case SkillType::BALL_SHRINK:   glowColor = RED; break;
                            case SkillType::EXPLOSION:     glowColor = ORANGE; break;
                            case SkillType::INVINCIBLE:    glowColor = GOLD; break;
                            case SkillType::SPLIT:         glowColor = SKYBLUE; break;
                            default: break;
                        }
                        skillBalls.emplace_back(spawnPos, type, skillBallRadius, Vector2{0, skillBallSpeedY}, glowColor);
                    }
                }
                // 球反弹
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

    // 4. 球间碰撞
    HandleBallCollisions();

    // 5. 技能球更新
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

    // 6. 粒子更新
    particleSystem.Update(dt, particleGravity);

    // 7. 效果更新
    UpdateEffects(dt);

    // 8. 红线碰撞检测
    CheckBallHitRedLine();

    // 9. 关卡过渡检查
    CheckLevelTransition();
}

// ======================== 绘制 ========================
void RacePlayer::Draw() {

    // 绘制拖尾
    for (size_t i = 0; i < balls.size(); ++i) {
        for (size_t j = 0; j < ballTrails[i].size(); ++j) {
            float alpha = 0.3f * (float)j / ballTrails[i].size();
            float size = balls[i].GetRadius() * (0.5f + 0.5f * j / ballTrails[i].size());
            DrawCircleV(ballTrails[i][j], size, Fade(RED, alpha));
        }
    }

    // 绘制球
    for (auto& b : balls) b.Draw();

    // 绘制板
    paddle.Draw();

    // 绘制对手板（如果有）
    if (hasOpponentPaddle) {
        Rectangle oppRect = paddle.GetRectangle();
        oppRect.x = opponentPaddleX;
        DrawRectangleRec(oppRect, Fade(BLUE, 0.5f));
    }

    // 绘制砖块
    for (auto& b : bricks) b.Draw();

    // 绘制技能球
    for (auto& sb : skillBalls) sb.Draw();

    // 绘制粒子
    particleSystem.Draw();

    // 绘制红线
    DrawRectangleRec(redLine, RED);

    // 绘制 UI 文字（分数、生命等）
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

    // 如果处于暂停或结束状态，可绘制半透明遮罩（具体由 RaceManager 负责，这里可不处理）
    DrawText(isLeftSide ? "LEFT" : "RIGHT", 10, 200, 20, RED);
}
