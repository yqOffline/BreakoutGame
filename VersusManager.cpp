#include "VersusManager.h"
#include "TextureCache.h"
#include "EffectFactory.h"
#include <iostream>
#include <cstring>
#include <enet/enet.h>          // 只在 .cpp 中包含

// ---------- 序列化辅助 ----------
template <typename T>
static std::vector<uint8_t> PackData(const T& obj) {
    std::vector<uint8_t> vec(sizeof(T));
    std::memcpy(vec.data(), &obj, sizeof(T));
    return vec;
}

template <typename T>
static T UnpackData(const std::vector<uint8_t>& data) {
    T obj{};
    if (data.size() >= sizeof(T))
        std::memcpy(&obj, data.data(), sizeof(T));
    return obj;
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
        SkillBallSpawnMsg msg;
        msg.type = VersusMsgType::SKILLBALL_SPAWN;
        msg.posX = sb.GetPosition().x;
        msg.posY = sb.GetPosition().y;
        msg.speedY = sb.GetSpeed().y;
        msg.skillType = (int32_t)sb.skillType;
        SendMessage(PackData(msg), false);
    };

    game.OnParticleSpawned = [this](Vector2 pos, Color col, int count, bool isBreak) {
        ParticleSpawnMsg pmsg;
        pmsg.type = VersusMsgType::PARTICLE_SPAWN;
        pmsg.posX = pos.x; pmsg.posY = pos.y;
        pmsg.r = col.r; pmsg.g = col.g; pmsg.b = col.b; pmsg.a = col.a;
        pmsg.count = count;
        pmsg.isBreak = isBreak ? 1 : 0;
        SendMessage(PackData(pmsg), false);
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
                        if (asHost) {
                            VersusNetMessage seedMsg{ VersusMsgType::SEED, (int32_t)pendingSeed, 0, 0 };
                            ENetPacket* pkt = enet_packet_create(&seedMsg, sizeof(seedMsg), ENET_PACKET_FLAG_RELIABLE);
                            enet_peer_send(peer, 0, pkt);
                        }
                        break;

                    case ENET_EVENT_TYPE_RECEIVE: {
                        std::vector<uint8_t> data(event.packet->dataLength);
                        std::memcpy(data.data(), event.packet->data, event.packet->dataLength);
                        incomingQueue.push(std::move(data));
                        enet_packet_destroy(event.packet);
                        break;
                    }

                    case ENET_EVENT_TYPE_DISCONNECT:
                        peer = nullptr;
                        isConnected = false;
                        incomingQueue.push({});   // 空包表示断开
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

// ---------- 发送消息（主线程安全） ----------
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
    SendMessage(PackData(snap), false);
}

// ---------- 主线程处理接收消息 ----------
void VersusManager::ProcessIncomingMessages() {
    std::vector<uint8_t> data;
    while (incomingQueue.try_pop(data)) {
        if (data.empty()) {
            // 断开连接信号
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

// ---------- 解析并处理收到的数据包 ----------
void VersusManager::HandleIncomingPacket(const std::vector<uint8_t>& data) {
    if (data.size() == sizeof(GameStateSnapshot)) {
        if (!isHost) {
            GameStateSnapshot snap = UnpackData<GameStateSnapshot>(data);
            AddSnapshotToBuffer(snap);
        }
        return;
    }
    if (data.size() == sizeof(SkillBallSpawnMsg)) {
        SkillBallSpawnMsg smsg = UnpackData<SkillBallSpawnMsg>(data);
        if (smsg.type == VersusMsgType::SKILLBALL_SPAWN) {
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
                default: glow = WHITE; break;
            }
            SkillBall sb(pos, type, radius, velocity, glow);
            game.AddSkillBall(sb);
        }
        return;
    }
    if (data.size() == sizeof(ParticleSpawnMsg)) {
        ParticleSpawnMsg pmsg = UnpackData<ParticleSpawnMsg>(data);
        if (pmsg.type == VersusMsgType::PARTICLE_SPAWN) {
            Vector2 pos = { pmsg.posX, pmsg.posY };
            Color col = { pmsg.r, pmsg.g, pmsg.b, pmsg.a };
            game.GetParticleSystem().EmitExplosion(pos, col, pmsg.count);
        }
        return;
    }
    if (data.size() != sizeof(VersusNetMessage)) return;
    VersusNetMessage msg = UnpackData<VersusNetMessage>(data);

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
        // 修改点：Guest 收到 CONTROL_START 时初始化并发射球
        case VersusMsgType::CONTROL_START:
            if (!isHost) {
                gameStarted = true;
                game.SetBallAttachedToUpper(false);
                game.TryLaunchBall(false);
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
                SendMessage(PackData(msg));
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
                SendMessage(PackData(seedMsg));
            }
        }
        if (IsKeyPressed(KEY_B)) running = false;
        return;
    }

    if (!gameStarted) {
        if (isHost && isConnected) {
            if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_SPACE)) {
                gameStarted = true;
                game.TryLaunchBall(true);   // 修改点：Host 发射球
                VersusNetMessage startMsg{ VersusMsgType::CONTROL_START };
                SendMessage(PackData(startMsg));
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
        VersusNetMessage inputMsg{ VersusMsgType::INPUT, dir, launch ? 1 : 0, 0 };
        SendMessage(PackData(inputMsg), false);
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
        DrawRectangle(0, winHeight/2 - 50, winWidth, 80, Fade(BLACK, 0.4f));
        const char* status;
        if (isHost) {
            status = (isConnected && threadRunning) ? "Press S to start" : "Waiting for opponent...";
        } else {
            status = (isConnected && threadRunning) ? "WAIT HOST TO START" : "Connecting to host...";
        }
        int tw = MeasureText(status, 30);
        DrawText(status, winWidth/2 - tw/2, winHeight/2 - 35, 30, WHITE);
        if (isHost && isConnected && threadRunning) {
            DrawRectangleRec(startBtn, GREEN);
            DrawText("START (S)", startBtn.x + 15, startBtn.y + 15, 20, BLACK);
        }
        DrawRectangleRec(backBtn, GRAY);
        DrawText("BACK (B)", backBtn.x + 20, backBtn.y + 15, 20, WHITE);
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
    SendMessage(PackData(msg));
}
void VersusManager::OnEffectApplied(EffectType type, bool upper) {
    VersusNetMessage msg{ VersusMsgType::EFFECT_APPLIED, (int32_t)type, upper ? 1 : 0, 0 };
    SendMessage(PackData(msg));
}
void VersusManager::OnEffectRemoved(EffectType type, bool upper) {
    VersusNetMessage msg{ VersusMsgType::EFFECT_REMOVED, (int32_t)type, upper ? 1 : 0, 0 };
    SendMessage(PackData(msg));
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
}