#include "VersusManager.h"
#include "TextureCache.h"
#include "EffectFactory.h"
#include "VersusNetMessage.h"
#include <iostream>
#include <cstring>
#include <enet/enet.h>

// 辅助 UI 函数（与 game.cpp 中一致）
static void DrawRoundedRect(Rectangle rect, float radius, Color color) {
    if (radius <= 0) {
        DrawRectangleRec(rect, color);
        return;
    }
    radius = fmin(radius, fmin(rect.width / 2.0f, rect.height / 2.0f));
    DrawRectangle(rect.x + radius, rect.y, rect.width - radius * 2, rect.height, color);
    DrawRectangle(rect.x, rect.y + radius, rect.width, rect.height - radius * 2, color);
    DrawCircle(rect.x + radius, rect.y + radius, radius, color);
    DrawCircle(rect.x + rect.width - radius, rect.y + radius, radius, color);
    DrawCircle(rect.x + radius, rect.y + rect.height - radius, radius, color);
    DrawCircle(rect.x + rect.width - radius, rect.y + rect.height - radius, radius, color);
}

static bool IsPointInRect(Vector2 point, Rectangle rect) {
    return point.x >= rect.x && point.x <= rect.x + rect.width &&
           point.y >= rect.y && point.y <= rect.y + rect.height;
}

static void DrawButton(Rectangle rect, const char* text, int fontSize, Color normal, Color hover, Color pressed, bool isHover, bool isPressed) {
    Color drawColor = normal;
    if (isPressed) drawColor = pressed;
    else if (isHover) drawColor = hover;
    DrawRoundedRect(rect, 10.0f, drawColor);
    DrawRectangleLinesEx(rect, 2, Fade(BLACK, 0.3f));
    int tw = MeasureText(text, fontSize);
    float tx = rect.x + (rect.width - tw) / 2;
    float ty = rect.y + (rect.height - fontSize) / 2;
    DrawText(text, tx, ty, fontSize, BLACK);
}

// ---------- 构造函数 ----------
VersusManager::VersusManager(int ww, int wh, const json& cfg, bool asHost, const std::string& ip, uint16_t port)
    : winWidth(ww), winHeight(wh), config(cfg),
      game(ww, wh, cfg), isHost(asHost), running(true), gameStarted(false), networkError(false),
      pendingSeed((unsigned int)time(nullptr)), drawResult(false)
{
    background = TextureCache::Instance().GetTexture("1.png");

    // 游戏回调
    game.OnLifeLost = [this]() { OnLifeLost(); };
    game.OnBallLaunched = [this](bool up) { OnBallLaunched(up); };
    game.OnEffectApplied = [this](EffectType t, bool up) { OnEffectApplied(t, up); };
    game.OnEffectRemoved = [this](EffectType t, bool up) { OnEffectRemoved(t, up); };

    game.OnSkillBallSpawned = [this](const SkillBall& sb) {
        SkillBallSpawnMsg spawnMsg;
        spawnMsg.type = VersusMsgType::SKILLBALL_SPAWN;
        spawnMsg.posX = sb.GetPosition().x;
        spawnMsg.posY = sb.GetPosition().y;
        spawnMsg.speedY = sb.GetSpeed().y;
        spawnMsg.skillType = static_cast<int32_t>(sb.skillType);
        SendMessage(Serialize(spawnMsg), false);
    };

    game.OnParticleSpawned = [this](Vector2 pos, Color col, int count, bool isBreak) {
        ParticleSpawnMsg particleMsg;
        particleMsg.type = VersusMsgType::PARTICLE_SPAWN;
        particleMsg.posX = pos.x;
        particleMsg.posY = pos.y;
        particleMsg.r = col.r;
        particleMsg.g = col.g;
        particleMsg.b = col.b;
        particleMsg.a = col.a;
        particleMsg.count = count;
        particleMsg.isBreak = isBreak ? 1 : 0;
        SendMessage(Serialize(particleMsg), false);
    };

    game.LoadLevel(pendingSeed);

    // 启动网络线程
    threadRunning = true;
    quitThread = false;
    std::string remoteIP = ip;

    networkThread = std::thread([this, asHost, remoteIP, port]() {
        if (enet_initialize() != 0) {
            networkError = true;
            threadRunning = false;
            return;
        }

        ENetHost* host = nullptr;
        ENetPeer* peer = nullptr;

        if (asHost) {
            ENetAddress addr;
            addr.host = ENET_HOST_ANY;
            addr.port = port;
            host = enet_host_create(&addr, 2, 2, 0, 0);
            if (!host) { networkError = true; threadRunning = false; return; }
        } else {
            host = enet_host_create(nullptr, 1, 2, 0, 0);
            if (!host) { networkError = true; threadRunning = false; return; }
            ENetAddress addr;
            enet_address_set_host(&addr, remoteIP.c_str());
            addr.port = port;
            peer = enet_host_connect(host, &addr, 2, 0);
            if (!peer) { networkError = true; threadRunning = false; return; }
        }

        while (!quitThread) {
            ENetEvent event;
            while (enet_host_service(host, &event, 0) > 0) {
                switch (event.type) {
                    case ENET_EVENT_TYPE_CONNECT:
                        peer = event.peer;
                        isConnected = true;
                        (LOG_INFO, "Guest: Connected to host");
                        if (asHost) {
                            VersusNetMessage seedMsg{ VersusMsgType::SEED, (int32_t)pendingSeed, 0, 0 };
                            auto data = Serialize(seedMsg);
                            ENetPacket* pkt = enet_packet_create(data.data(), data.size(), ENET_PACKET_FLAG_RELIABLE);
                            enet_peer_send(peer, 0, pkt);
                        }
                        break;

                    case ENET_EVENT_TYPE_RECEIVE: {
                        (LOG_INFO, "Guest: Received packet, size=%d", event.packet->dataLength);
                        std::vector<uint8_t> data(event.packet->dataLength);
                        std::memcpy(data.data(), event.packet->data, event.packet->dataLength);
                        incomingQueue.push(std::move(data));
                        enet_packet_destroy(event.packet);
                        break;
                    }

                    case ENET_EVENT_TYPE_DISCONNECT:
                        peer = nullptr;
                        isConnected = false;
                        (LOG_INFO, "Guest: Disconnected");
                        incomingQueue.push({});
                        break;

                    default: break;
                }
            }

            // 发送队列
            SendItem item;
            while (outgoingQueue.try_pop(item)) {
                if (peer) {
                    ENetPacket* pkt = enet_packet_create(item.data.data(), item.data.size(),
                        item.reliable ? ENET_PACKET_FLAG_RELIABLE : ENET_PACKET_FLAG_UNSEQUENCED);
                    enet_peer_send(peer, 0, pkt);
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        if (host) enet_host_destroy(host);
        enet_deinitialize();
        threadRunning = false;
    });

    startBtn = { winWidth/2.0f - 60, winHeight/2.0f + 30, 120, 50 };
    restartBtn = { winWidth/2.0f - 60, winHeight/2.0f + 40, 120, 50 };
    backBtn = { winWidth/2.0f - 60, (float)(winHeight - 80), 120, 50 };
}

// ---------- 析构 ----------
VersusManager::~VersusManager() {
    quitThread = true;
    if (networkThread.joinable())
        networkThread.join();
}

// ---------- 发送消息 ----------
void VersusManager::SendMessage(const std::vector<uint8_t>& data, bool reliable) {
    if (!isConnected) return;
    outgoingQueue.push({data, reliable});
}

void VersusManager::SendGameState() {
    GameStateSnapshot snap = game.GetSnapshot();
    if (isHost && simulatePacketLoss) {
        float roll = (float)(GetRandomValue(0, 1000)) / 1000.0f;
        if (roll < packetLossRate) return;
    }
    auto data = Serialize(snap);
    SendMessage(data, false);
}

// ---------- 处理接收消息 ----------
void VersusManager::ProcessIncomingMessages() {
    std::vector<uint8_t> data;
    while (incomingQueue.try_pop(data)) {
        if (data.empty()) {
            if (!drawResult) {
                drawResult = true;
                resultText = isHost ? "YOU WIN! OPPONENT LEFT" : "CONNECTION LOST";
            }
            isConnected = false;
            continue;
        }
        HandleIncomingPacket(data);
    }
}

// 成员函数实现
void VersusManager::HandleIncomingPacket(const std::vector<uint8_t>& data) {
    // 尝试反序列化为 GameStateSnapshot
    GameStateSnapshot snap;
    if (Deserialize(data.data(), data.size(), snap)) {
        if (!isHost && !gameStarted && snap.hostGameTime > 0.0f) {
            (LOG_INFO, "Guest: Auto-starting on first snapshot with positive game time");
            gameStarted = true;
        }
        if (!isHost) {
            AddSnapshotToBuffer(snap);
        }
        return;
    }
    // 尝试反序列化为 SkillBallSpawnMsg
    SkillBallSpawnMsg smsg;
    if (Deserialize(data.data(), data.size(), smsg) && smsg.type == VersusMsgType::SKILLBALL_SPAWN) {
        (LOG_INFO, "Guest: Received VersusNetMessage type = %d", (int)smsg.type);
        SkillType type = static_cast<SkillType>(smsg.skillType);
        Vector2 pos = { smsg.posX, smsg.posY };
        Vector2 velocity = { 0.0f, smsg.speedY };
        float radius = config["skill_ball"]["radius"].get<float>();
        Color glow;
        switch (type) {
            case SkillType::PADDLE_EXTEND: glow = BLUE; break;
            case SkillType::BALL_ENLARGE:  glow = GREEN; break;
            case SkillType::BALL_SHRINK:   glow = RED; break;
            case SkillType::EXPLOSION:     glow = ORANGE; break;
            case SkillType::INVINCIBLE:    glow = GOLD; break;
            case SkillType::SPLIT:         glow = SKYBLUE; break;
            default: glow = WHITE;
        }
        SkillBall sb(pos, type, radius, velocity, glow);
        game.AddSkillBall(sb);
        return;
    }
    // 尝试反序列化为 ParticleSpawnMsg
    ParticleSpawnMsg pmsg;
    if (Deserialize(data.data(), data.size(), pmsg) && pmsg.type == VersusMsgType::PARTICLE_SPAWN) {
        Vector2 pos = { pmsg.posX, pmsg.posY };
        Color col = { pmsg.r, pmsg.g, pmsg.b, pmsg.a };
        game.GetParticleSystem().EmitExplosion(pos, col, pmsg.count);
        return;
    }
    // 尝试反序列化为 VersusNetMessage
    VersusNetMessage msg;
    if (Deserialize(data.data(), data.size(), msg)) {
        switch (msg.type) {
            case VersusMsgType::SEED:
                if (!isHost) game.LoadLevel((unsigned int)msg.data1);
                break;
            case VersusMsgType::INPUT: {
                int dir = msg.data1;
                bool launch = (msg.data2 != 0);
                if (isHost) {
                    game.MovePaddle(false, dir);
                    if (launch) game.TryLaunchBall(false);
                }
                break;
            }
            case VersusMsgType::CONTROL_START:
                if (!isHost) {
                    void HandleIncomingPacket(const std::vector<uint8_t>& data);
                    gameStarted = true;
                }
                break;
            case VersusMsgType::EFFECT_APPLIED: {
                EffectType etype = (EffectType)msg.data1;
                bool upper = (msg.data2 != 0);
                auto effect = EffectFactory::CreateEffect(static_cast<SkillType>(etype));
                if (effect) game.ApplyEffectToPlayer(std::move(effect), upper);
                break;
            }
            case VersusMsgType::GAME_OVER: {
                drawResult = true;
                resultText = (msg.data1 == 1) ? "YOU LOSE!" : "YOU WIN!";
                gameStarted = false;
                break;
            }
            default: break;
        }
        return;
    }
}

// ---------- Update ----------
void VersusManager::Update(float dt) {
    if (!running || networkError) return;
    ProcessIncomingMessages();
    if (gameStarted) {
        if (isHost) {
            game.Update(dt);
            SendGameState();
            if (game.GetUpperLives() <= 0 || game.GetLowerLives() <= 0) {
                drawResult = true;
                int winner = (game.GetUpperLives() <= 0) ? 0 : 1;
                int hostWin = (winner == 1) ? 1 : 0;
                VersusNetMessage msg{ VersusMsgType::GAME_OVER, hostWin, 0, 0 };
                SendMessage(Serialize(msg), true);
                resultText = hostWin ? "YOU WIN!" : "YOU LOSE!";
                gameStarted = false;
            }
        } else {
            // Guest 端：更新效果和视觉
            game.UpdateEffects(dt);
            game.UpdateVisuals(dt);

            float renderTime = latestHostTime - interpolationDelay;
            ApplyInterpolation(renderTime);
        }
    }
}

// ---------- HandleInput ----------
void VersusManager::HandleInput() {
    // 丢包模拟开关
    if (isHost && IsKeyPressed(KEY_K)) {
        simulatePacketLoss = !simulatePacketLoss;
    }
    if (isHost && IsKeyPressed(KEY_L)) {
        if (packetLossRate < 0.15f)      packetLossRate = 0.3f;
        else if (packetLossRate < 0.4f)  packetLossRate = 0.5f;
        else if (packetLossRate < 0.6f)  packetLossRate = 0.7f;
        else if (packetLossRate < 0.9f)  packetLossRate = 1.0f;
        else                             packetLossRate = 0.0f;
    }

    if (drawResult) {
        if (IsKeyPressed(KEY_R)) {
            gameStarted = false;
            drawResult = false;
            pendingSeed = (unsigned int)time(nullptr);
            game.LoadLevel(pendingSeed);
            snapshotBuffer.clear();
            latestHostTime = 0.0f;
            if (isHost) {
                VersusNetMessage seedMsg{ VersusMsgType::SEED, (int32_t)pendingSeed, 0, 0 };
                SendMessage(Serialize(seedMsg), true);
            }
        }
        if (IsKeyPressed(KEY_B)) running = false;
        return;
    }

    if (!gameStarted) {
        if (isHost && isConnected) {
            if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_SPACE)) {
                (LOG_INFO, "Host: Sending CONTROL_START");
                gameStarted = true;
                VersusNetMessage startMsg{ VersusMsgType::CONTROL_START };
                SendMessage(Serialize(startMsg), true);
            }
        }
        if (IsKeyPressed(KEY_B)) running = false;
        return;
    }

    bool upper = isHost;
    int dir = 0;
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  dir = -1;
    else if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) dir = 1;
    bool launch = IsKeyPressed(KEY_SPACE) && game.IsBallAttached() &&
                  ((upper && game.IsUpperWaitingLaunch()) || (!upper && game.IsLowerWaitingLaunch()));

    if (isHost) {
        game.MovePaddle(upper, dir);
        if (launch) game.TryLaunchBall(upper);
    } else {
        VersusNetMessage inputMsg;
        inputMsg.type = VersusMsgType::INPUT;
        inputMsg.data1 = dir;
        inputMsg.data2 = launch ? 1 : 0;
        inputMsg.floatData = 0.0f;
        SendMessage(Serialize(inputMsg), false);
    }
}

// ---------- Draw ----------
void VersusManager::Draw() {
    DrawTexturePro(background, {0,0,(float)background.width,(float)background.height},
                   {0,0,(float)winWidth,(float)winHeight}, {0,0}, 0, WHITE);
    game.Draw();

    if (isHost && simulatePacketLoss) {
        DrawText(TextFormat("PACKET LOSS: %d%%", (int)(packetLossRate * 100)), 10, winHeight - 50, 20, RED);
    }
    if (!isHost) {
        DrawText(TextFormat("INTERP DELAY: %.0fms", interpolationDelay * 1000), 10, winHeight - 50, 20, YELLOW);
        DrawText(TextFormat("BUFFER: %d", (int)snapshotBuffer.size()), 10, winHeight - 30, 20, YELLOW);
    }

    if (!gameStarted && !drawResult) {
        int sw = winWidth;
        int sh = winHeight;

        // 背景图片
        Texture2D bg = TextureCache::Instance().GetTexture("1.png");
        if (bg.id != 0) {
            DrawTexturePro(bg, {0,0,(float)bg.width,(float)bg.height},
                        {0,0,(float)sw,(float)sh}, {0,0}, 0, WHITE);
        } else {
            ClearBackground(DARKGRAY);
        }

        // 半透明白色圆角面板
        Rectangle panel = { sw/2.0f - 250, sh/2.0f - 150, 500, 300 };
        DrawRectangleRounded(panel, 0.2f, 10, Fade(WHITE, 0.85f));
        DrawRectangleLinesEx(panel, 2, BLACK);

        DrawText("VERSUS LOBBY", sw/2 - 120, sh/2 - 110, 40, DARKBLUE);

        // 角色
        const char* roleText = isHost ? "HOST" : "GUEST";
        Color roleColor = isHost ? BLUE : RED;
        DrawText(TextFormat("You are: %s", roleText), sw/2 - 80, sh/2 - 60, 24, roleColor);

        // 连接状态
        bool connected = isConnected && threadRunning;
        const char* status = connected ? (isHost ? "Opponent connected" : "Connected to host") : "Connecting...";
        Color statusColor = connected ? GREEN : YELLOW;
        DrawText(status, sw/2 - 100, sh/2 - 20, 22, statusColor);
        DrawCircle(sw/2 - 120, sh/2 - 12, 8, statusColor);

        // 开始按钮（仅 Host）
        if (isHost && connected) {
            Rectangle startBtnRect = { sw/2.0f - 80, sh/2.0f + 40, 160, 50 };
            bool hover = CheckCollisionPointRec(GetMousePosition(), startBtnRect);
            Color btnColor = hover ? DARKGREEN : GREEN;
            DrawRectangleRounded(startBtnRect, 0.2f, 8, btnColor);
            DrawRectangleLinesEx(startBtnRect, 2, BLACK);
            DrawText("START GAME", startBtnRect.x + 20, startBtnRect.y + 12, 24, WHITE);
            DrawText("Press S to start", sw/2 - 80, sh/2 + 100, 18, DARKGRAY);
        } else if (!isHost && connected) {
            DrawText("Waiting for host to start...", sw/2 - 140, sh/2 + 50, 20, DARKGRAY);
        }

        // 返回按钮
        Rectangle backBtnRect = { sw/2.0f - 60, (float)sh - 70, 120, 50 };
        bool hoverBack = CheckCollisionPointRec(GetMousePosition(), backBtnRect);
        Color backColor = hoverBack ? DARKGRAY : GRAY;
        DrawRectangleRounded(backBtnRect, 0.2f, 8, backColor);
        DrawRectangleLinesEx(backBtnRect, 2, BLACK);
        DrawText("BACK", backBtnRect.x + 35, backBtnRect.y + 12, 24, WHITE);
    }

    if (drawResult) {
        DrawRectangle(0, 0, winWidth, winHeight, Fade(BLACK, 0.8f));
        int tw = MeasureText(resultText.c_str(), 50);
        DrawText(resultText.c_str(), winWidth/2 - tw/2, winHeight/2 - 60, 50,
                resultText.find("WIN") != std::string::npos ? GOLD : RED);
        DrawRectangleRec(restartBtn, GREEN);
        DrawText("RESTART (R)", restartBtn.x + 10, restartBtn.y + 15, 20, BLACK);
        DrawRectangleRec(backBtn, GRAY);
        DrawText("BACK (B)", backBtn.x + 20, backBtn.y + 15, 20, WHITE);
    }
}

// ---------- 回调 ----------
void VersusManager::OnLifeLost() {}

void VersusManager::OnBallLaunched(bool isUpper) {
    VersusNetMessage msg{ VersusMsgType::BALL_LAUNCHED, isUpper ? 1 : 0, 0, 0 };
    SendMessage(Serialize(msg), true);
}

void VersusManager::OnEffectApplied(EffectType type, bool upper) {
    VersusNetMessage msg{ VersusMsgType::EFFECT_APPLIED, (int32_t)type, upper ? 1 : 0, 0 };
    SendMessage(Serialize(msg), true);
}

void VersusManager::OnEffectRemoved(EffectType type, bool upper) {
    VersusNetMessage msg{ VersusMsgType::EFFECT_REMOVED, (int32_t)type, upper ? 1 : 0, 0 };
    SendMessage(Serialize(msg), true);
}

// ---------- 插值相关 ----------
void VersusManager::AddSnapshotToBuffer(const GameStateSnapshot& snap) {
    if (!snapshotBuffer.empty() && snap.hostGameTime <= snapshotBuffer.back().hostGameTime)
        return;
    snapshotBuffer.push_back(snap);
    while (snapshotBuffer.size() > 6)
        snapshotBuffer.pop_front();
    if (snap.hostGameTime > latestHostTime)
        latestHostTime = snap.hostGameTime;
}

void VersusManager::ApplyInterpolation(float renderTime) {
    if (snapshotBuffer.size() < 2) {
        if (!snapshotBuffer.empty())
            game.ApplySnapshot(snapshotBuffer.back());
        return;
    }
    const GameStateSnapshot* prev = nullptr;
    const GameStateSnapshot* next = nullptr;
    for (size_t i = 0; i < snapshotBuffer.size() - 1; ++i) {
        if (snapshotBuffer[i].hostGameTime <= renderTime && snapshotBuffer[i+1].hostGameTime >= renderTime) {
            prev = &snapshotBuffer[i];
            next = &snapshotBuffer[i+1];
            break;
        }
    }
    if (!prev || !next) {
        game.ApplySnapshot(snapshotBuffer.back());
        return;
    }
    float delta = next->hostGameTime - prev->hostGameTime;
    float t = 0.0f;
    if (delta > 0.0001f) {
        t = (renderTime - prev->hostGameTime) / delta;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
    }
    game.ApplyInterpolatedState(*prev, *next, t);
    game.ApplyEffectsAndSkillBalls(*next);
}