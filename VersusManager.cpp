#include "VersusManager.h"
#include "EffectFactory.h"
#include "VersusNetMessage.h"
#include <iostream>
#include <cstring>

VersusManager::VersusManager(int ww, int wh, const json& cfg, bool asHost, const std::string& ip, uint16_t port)
    : winWidth(ww), winHeight(wh), config(cfg),
      game(ww, wh, cfg), isHost(asHost), running(true), gameStarted(false), networkError(false),
      host(nullptr), peer(nullptr), pendingSeed((unsigned int)time(nullptr)), drawResult(false)
{
    background = LoadTexture("1.png");
    if (enet_initialize() != 0) {
        std::cerr << "Failed to init ENet" << std::endl;
        networkError = true;
        return;
    }

    game.OnLifeLost = [this]() { OnLifeLost(); };
    game.OnBallLaunched = [this](bool up) { OnBallLaunched(up); };
    game.OnEffectApplied = [this](EffectType t, bool up) { OnEffectApplied(t, up); };
    game.OnEffectRemoved = [this](EffectType t, bool up) { OnEffectRemoved(t, up); };

    game.OnSkillBallSpawned = [this](const SkillBall& sb) {
        if (!peer) return;
        SkillBallSpawnMsg msg;
        msg.type = VersusMsgType::SKILLBALL_SPAWN;
        msg.posX = sb.GetPosition().x;
        msg.posY = sb.GetPosition().y;
        msg.speedY = sb.GetSpeed().y;
        msg.skillType = (int32_t)sb.skillType;
        ENetPacket* packet = enet_packet_create(&msg, sizeof(msg), ENET_PACKET_FLAG_UNSEQUENCED);
        enet_peer_send(peer, 0, packet);
    };

    game.OnParticleSpawned = [this](Vector2 pos, Color col, int count, bool isBreak) {
        if (!peer) return;
        ParticleSpawnMsg pmsg;
        pmsg.type = VersusMsgType::PARTICLE_SPAWN;
        pmsg.posX = pos.x;
        pmsg.posY = pos.y;
        pmsg.r = col.r; pmsg.g = col.g; pmsg.b = col.b; pmsg.a = col.a;
        pmsg.count = count;
        pmsg.isBreak = isBreak ? 1 : 0;
        ENetPacket* packet = enet_packet_create(&pmsg, sizeof(pmsg), ENET_PACKET_FLAG_UNSEQUENCED);
        enet_peer_send(peer, 0, packet);
    };

    game.LoadLevel(pendingSeed);
    InitNetwork(asHost, ip, port);

    startBtn = { winWidth/2.0f - 60, winHeight/2.0f + 30, 120, 50 };
    restartBtn = { winWidth/2.0f - 60, winHeight/2.0f + 40, 120, 50 };
    backBtn = { winWidth/2.0f - 60, (float)(winHeight - 80), 120, 50 };
}

VersusManager::~VersusManager() {
    UnloadTexture(background);
    if (host) enet_host_destroy(host);
    enet_deinitialize();
}

void VersusManager::InitNetwork(bool asHost, const std::string& ip, uint16_t port) {
    if (asHost) {
        ENetAddress addr;
        addr.host = ENET_HOST_ANY;
        addr.port = port;
        host = enet_host_create(&addr, 2, 2, 0, 0);
        if (!host) { networkError = true; return; }
        std::cout << "VERSUS Host on port " << port << std::endl;
    } else {
        host = enet_host_create(nullptr, 1, 2, 0, 0);
        if (!host) { networkError = true; return; }
        ENetAddress addr;
        enet_address_set_host(&addr, ip.c_str());
        addr.port = port;
        peer = enet_host_connect(host, &addr, 2, 0);
        if (!peer) { networkError = true; return; }

        ENetEvent event;
        if (enet_host_service(host, &event, 3000) > 0 && event.type == ENET_EVENT_TYPE_CONNECT) {
            peer = event.peer;
            std::cout << "Connected to host" << std::endl;
        } else {
            networkError = true;
        }
    }
}

void VersusManager::SendMessage(const VersusNetMessage& msg, bool reliable) {
    if (!peer) return;
    ENetPacket* packet = enet_packet_create(&msg, sizeof(msg),
         reliable ? ENET_PACKET_FLAG_RELIABLE : ENET_PACKET_FLAG_UNSEQUENCED);
    enet_peer_send(peer, 0, packet);
}

void VersusManager::SendGameState() {
    GameStateSnapshot snap = game.GetSnapshot();
    // 丢包模拟（仅Host端生效）
    if (isHost && simulatePacketLoss) {
        float roll = (float)(GetRandomValue(0, 1000)) / 1000.0f;
        if (roll < packetLossRate)
            return;   // 模拟丢包，不发送
    }
    ENetPacket* packet = enet_packet_create(&snap, sizeof(snap), ENET_PACKET_FLAG_UNSEQUENCED);
    if (peer) enet_peer_send(peer, 0, packet);
}

void VersusManager::ProcessNetworkEvents() {
    if (networkError) return;
    ENetEvent event;
    while (enet_host_service(host, &event, 0) > 0) {
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT:
                peer = event.peer;
                if (isHost) {
                    VersusNetMessage seedMsg{ VersusMsgType::SEED, (int32_t)pendingSeed, 0, 0 };
                    SendMessage(seedMsg);
                }
                break;
            case ENET_EVENT_TYPE_RECEIVE:
                HandlePacket(event.packet);
                enet_packet_destroy(event.packet);
                break;
            case ENET_EVENT_TYPE_DISCONNECT:
                peer = nullptr;
                if (!drawResult) {
                    drawResult = true;
                    resultText = isHost ? "YOU WIN! OPPONENT LEFT" : "CONNECTION LOST";
                }
                break;
            default: break;
        }
    }
}

void VersusManager::AddSnapshotToBuffer(const GameStateSnapshot& snap) {
    // 插入快照并保持时间戳递增
    if (!snapshotBuffer.empty() && snap.hostGameTime <= snapshotBuffer.back().hostGameTime)
        return; // 忽略乱序或重复
    snapshotBuffer.push_back(snap);

    // 保持缓冲区大小（仅保留最近6帧，可根据实际情况调整）
    while (snapshotBuffer.size() > 6)
        snapshotBuffer.pop_front();
    
    if (snap.hostGameTime > latestHostTime)
        latestHostTime = snap.hostGameTime;
}

void VersusManager::ApplyInterpolation(float renderTime) {
    // 如果缓冲区中没有足够数据，则直接使用最新快照（无插值）
    if (snapshotBuffer.size() < 2) {
        if (!snapshotBuffer.empty())
            game.ApplySnapshot(snapshotBuffer.back());
        return;
    }

    // 查找两个相邻快照，满足 prev.timestamp <= renderTime <= next.timestamp
    const GameStateSnapshot* prev = nullptr;
    const GameStateSnapshot* next = nullptr;
    for (size_t i = 0; i < snapshotBuffer.size() - 1; ++i) {
        if (snapshotBuffer[i].hostGameTime <= renderTime &&
            snapshotBuffer[i+1].hostGameTime >= renderTime) {
            prev = &snapshotBuffer[i];
            next = &snapshotBuffer[i+1];
            break;
        }
    }

    if (!prev || !next) {
        // 如果找不到合适的区间（比如 renderTime 超前或滞后），则使用最新快照
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

void VersusManager::HandlePacket(ENetPacket* packet) {
    if (packet->dataLength == sizeof(GameStateSnapshot)) {
        if (!isHost) {
            GameStateSnapshot* snap = (GameStateSnapshot*)packet->data;
            AddSnapshotToBuffer(*snap);   // 存入缓冲区，不再直接 ApplySnapshot
        }
        return;
    }
    if (packet->dataLength == sizeof(SkillBallSpawnMsg)) {
        SkillBallSpawnMsg* smsg = (SkillBallSpawnMsg*)packet->data;
        if (smsg->type == VersusMsgType::SKILLBALL_SPAWN) {
            SkillType type = static_cast<SkillType>(smsg->skillType);
            Vector2 pos = { smsg->posX, smsg->posY };
            Vector2 velocity = { 0.0f, smsg->speedY };
            float radius = config["skill_ball"]["radius"].get<float>();
            Color glow = (type == SkillType::PADDLE_EXTEND) ? BLUE :
                         (type == SkillType::BALL_ENLARGE) ? GREEN :
                         (type == SkillType::BALL_SHRINK) ? RED :
                         (type == SkillType::EXPLOSION) ? ORANGE :
                         (type == SkillType::INVINCIBLE) ? GOLD : SKYBLUE;
            SkillBall sb(pos, type, radius, velocity, glow);
            game.AddSkillBall(sb);
        }
        return;
    }
    if (packet->dataLength == sizeof(ParticleSpawnMsg)) {
        ParticleSpawnMsg* pmsg = (ParticleSpawnMsg*)packet->data;
        if (pmsg->type == VersusMsgType::PARTICLE_SPAWN) {
            Vector2 pos = { pmsg->posX, pmsg->posY };
            Color col = { pmsg->r, pmsg->g, pmsg->b, pmsg->a };
            game.GetParticleSystem().EmitExplosion(pos, col, pmsg->count);
        }
        return;
    }
    if (packet->dataLength != sizeof(VersusNetMessage)) return;
    VersusNetMessage* msg = (VersusNetMessage*)packet->data;

    switch (msg->type) {
        case VersusMsgType::SEED:
            if (!isHost) game.LoadLevel((unsigned int)msg->data1);
            break;
        case VersusMsgType::INPUT: {
            int dir = msg->data1;
            bool launch = (msg->data2 != 0);
            if (isHost) {
                game.MovePaddle(false, dir);
                if (launch) game.TryLaunchBall(false);
            }
            break;
        }
        case VersusMsgType::CONTROL_START:
            if (!isHost) gameStarted = true;
            break;
        case VersusMsgType::EFFECT_APPLIED: {
            EffectType etype = (EffectType)msg->data1;
            bool upper = (msg->data2 != 0);
            auto effect = EffectFactory::CreateEffect(static_cast<SkillType>(etype));
            if (effect) game.ApplyEffectToPlayer(std::move(effect), upper);
            break;
        }
        case VersusMsgType::GAME_OVER: {
            drawResult = true;
            resultText = (msg->data1 == 1) ? "YOU LOSE!" : "YOU WIN!";
            gameStarted = false;
            break;
        }
        default: break;
    }
}

void VersusManager::Update(float dt) {
    if (!running || networkError) return;
    ProcessNetworkEvents();
    if (gameStarted) {
        if (isHost) {
            game.Update(dt);
            SendGameState();
            if (game.GetUpperLives() <= 0 || game.GetLowerLives() <= 0) {
                drawResult = true;
                int winner = (game.GetUpperLives() <= 0) ? 0 : 1;
                int hostWin = (isHost && winner == 1) || (!isHost && winner == 0) ? 1 : 0;
                VersusNetMessage msg{ VersusMsgType::GAME_OVER, hostWin, 0, 0 };
                SendMessage(msg);
                resultText = hostWin ? "YOU WIN!" : "YOU LOSE!";
                gameStarted = false;
            }
        } else {
            // Guest 端：插值更新视觉
            game.UpdateEffects(dt);
            game.UpdateVisuals(dt);

            // 计算目标渲染时间 = 最新主机时间 - 插值延迟
            float renderTime = latestHostTime - interpolationDelay;
            ApplyInterpolation(renderTime);
        }
    }
}

void VersusManager::HandleInput() {
    // ---------- 丢包模拟控制（仅 Host 端有效） ----------
    if (isHost && IsKeyPressed(KEY_K)) {
        simulatePacketLoss = !simulatePacketLoss;
        std::cout << "Packet loss simulation: " << (simulatePacketLoss ? "ON" : "OFF") << std::endl;
    }
    if (isHost && IsKeyPressed(KEY_L)) {
        // 循环切换丢包率：0% -> 30% -> 50% -> 70% -> 100% -> 0%
        if (packetLossRate < 0.15f)        packetLossRate = 0.3f;
        else if (packetLossRate < 0.4f)    packetLossRate = 0.5f;
        else if (packetLossRate < 0.6f)    packetLossRate = 0.7f;
        else if (packetLossRate < 0.9f)    packetLossRate = 1.0f;
        else                               packetLossRate = 0.0f;
        std::cout << "Packet loss rate: " << packetLossRate * 100 << "%" << std::endl;
    }

    if (drawResult) {
        if (IsKeyPressed(KEY_R)) {
            gameStarted = false;
            drawResult = false;
            pendingSeed = (unsigned int)time(nullptr);
            game.LoadLevel(pendingSeed);
            snapshotBuffer.clear();   // 清空插值缓冲区
            latestHostTime = 0.0f;
            if (isHost) {
                VersusNetMessage seedMsg{ VersusMsgType::SEED, (int32_t)pendingSeed, 0, 0 };
                SendMessage(seedMsg);
            }
        }
        if (IsKeyPressed(KEY_B)) {
            running = false;
        }
        return;
    }

    if (!gameStarted) {
        if (isHost && peer) {
            if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_SPACE)) {
                gameStarted = true;
                VersusNetMessage startMsg{ VersusMsgType::CONTROL_START, 0, 0, 0 };
                SendMessage(startMsg);
            }
        }
        if (IsKeyPressed(KEY_B)) {
            running = false;
        }
        return;
    }

    bool isUpper = isHost;
    int dir = 0;
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  dir = -1;
    else if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) dir = 1;
    bool launch = IsKeyPressed(KEY_SPACE) && game.IsBallAttached() &&
                   ((isUpper && game.IsUpperWaitingLaunch()) || (!isUpper && game.IsLowerWaitingLaunch()));

    if (isHost) {
        game.MovePaddle(isUpper, dir);
        if (launch) game.TryLaunchBall(isUpper);
    } else {
        VersusNetMessage inputMsg{ VersusMsgType::INPUT, dir, launch ? 1 : 0, 0 };
        SendMessage(inputMsg, false);
    }
}

void VersusManager::Draw() {
    DrawTexturePro(background, {0,0,(float)background.width,(float)background.height},
                   {0,0,(float)winWidth,(float)winHeight}, {0,0}, 0, WHITE);    
    game.Draw();

    // 显示丢包/插值状态信息
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
            status = peer ? "Press S to start" : "Waiting for opponent...";
        } else {
            status = peer ? "WAIT HOST TO START" : "Connecting to host...";
        }
        int tw = MeasureText(status, 30);
        DrawText(status, winWidth/2 - tw/2, winHeight/2 - 35, 30, WHITE);
        if (isHost && peer) {
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

void VersusManager::OnLifeLost() { }
void VersusManager::OnBallLaunched(bool isUpper) {
    VersusNetMessage msg{ VersusMsgType::BALL_LAUNCHED, isUpper ? 1 : 0, 0, 0 };
    SendMessage(msg);
}
void VersusManager::OnEffectApplied(EffectType type, bool upper) {
    VersusNetMessage msg{ VersusMsgType::EFFECT_APPLIED, (int32_t)type, upper ? 1 : 0, 0 };
    SendMessage(msg);
}
void VersusManager::OnEffectRemoved(EffectType type, bool upper) {
    VersusNetMessage msg{ VersusMsgType::EFFECT_REMOVED, (int32_t)type, upper ? 1 : 0, 0 };
    SendMessage(msg);
}