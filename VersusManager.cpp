#include "VersusManager.h"
#include "EffectFactory.h"
#include <iostream>
#include <cstring>

VersusManager::VersusManager(int ww, int wh, const json& cfg, bool asHost, const std::string& ip, uint16_t port)
    : winWidth(ww), winHeight(wh), config(cfg),
      game(ww, wh, cfg), isHost(asHost), running(true), gameStarted(false), networkError(false),
      host(nullptr), peer(nullptr), pendingSeed((unsigned int)time(nullptr)), drawResult(false)
{
    if (enet_initialize() != 0) {
        std::cerr << "Failed to init ENet" << std::endl;
        networkError = true;
        return;
    }

    game.OnLifeLost = [this]() { OnLifeLost(); };
    game.OnBallLaunched = [this](bool up) { OnBallLaunched(up); };
    game.OnEffectApplied = [this](EffectType t, bool up) { OnEffectApplied(t, up); };
    game.OnEffectRemoved = [this](EffectType t, bool up) { OnEffectRemoved(t, up); };
    game.OnSkillBallSpawned = [this](const SkillBall& sb) { OnSkillBallSpawned(sb); };

    game.LoadLevel(pendingSeed);

    InitNetwork(asHost, ip, port);

    startBtn = { winWidth/2.0f - 60, winHeight/2.0f + 30, 120, 50 };
    restartBtn = { winWidth/2.0f - 60, winHeight/2.0f + 40, 120, 50 };
    backBtn = { winWidth/2.0f - 60, (float)(winHeight - 80), 120, 50 };
}

VersusManager::~VersusManager() {
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
    ENetPacket* packet = enet_packet_create(&snap, sizeof(snap), ENET_PACKET_FLAG_UNSEQUENCED);
    enet_peer_send(peer, 0, packet);
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

void VersusManager::HandlePacket(ENetPacket* packet) {
    if (packet->dataLength == sizeof(GameStateSnapshot)) {
        if (!isHost) {
            GameStateSnapshot* snap = (GameStateSnapshot*)packet->data;
            game.ApplySnapshot(*snap);
        }
        return;
    }
    if (packet->dataLength != sizeof(VersusNetMessage)) return;
    VersusNetMessage* msg = (VersusNetMessage*)packet->data;

    switch (msg->type) {
        case VersusMsgType::SEED:
            if (!isHost) {
                game.LoadLevel((unsigned int)msg->data1);
            }
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
            if (!isHost) {
                gameStarted = true;
            }
            break;
        case VersusMsgType::LIFE_LOST: break;
        case VersusMsgType::BALL_LAUNCHED: break;
        case VersusMsgType::EFFECT_APPLIED: {
            EffectType etype = (EffectType)msg->data1;
            bool upper = (msg->data2 != 0);
            // 注意 SkillType 与 EffectType 数值一致，可直接转换
            auto effect = EffectFactory::CreateEffect(static_cast<SkillType>(etype));
            if (effect) game.ApplyEffectToPlayer(std::move(effect), upper);
            break;
        }
        case VersusMsgType::EFFECT_REMOVED: break;
        case VersusMsgType::SKILLBALL_SPAWN: break;
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
            game.UpdateEffects(dt);
            if (game.GetUpperLives() <= 0 || game.GetLowerLives() <= 0) {
                // 结果将由 Host 发送
            }
        }
    }
}

void VersusManager::HandleInput() {
    if (drawResult) {
        if (IsKeyPressed(KEY_R)) {
            gameStarted = false;
            drawResult = false;
            pendingSeed = (unsigned int)time(nullptr);
            game.LoadLevel(pendingSeed);
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
        // Host 可以按 S 开始游戏，并通知 Guest
        if (isHost && peer) {
            if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_SPACE)) {
                gameStarted = true;
                VersusNetMessage startMsg{ VersusMsgType::CONTROL_START, 0, 0, 0 };
                SendMessage(startMsg);
                // 开始后，Host 需要处理自己的挡板？直接开始，球等待发射。
            }
        }
        if (IsKeyPressed(KEY_B)) {
            running = false;
        }
        return;
    }

    // 游戏进行中
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
    game.Draw();

    if (!gameStarted && !drawResult) {
        DrawRectangle(0, 0, winWidth, winHeight, Fade(BLACK, 0.5f));
        const char* status;
        if (isHost) {
            status = peer ? "Press S to start" : "Waiting for opponent...";
        } else {
            status = peer ? "WAIT HOST TO START" : "Connecting to host...";
        }
        int tw = MeasureText(status, 30);
        DrawText(status, winWidth/2 - tw/2, winHeight/2 - 50, 30, WHITE);
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

// 回调实现（不变）
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
void VersusManager::OnSkillBallSpawned(const SkillBall& sb) {
    VersusNetMessage msg{ VersusMsgType::SKILLBALL_SPAWN, (int32_t)sb.GetPosition().y, (int32_t)sb.skillType, sb.GetPosition().x };
    SendMessage(msg);
}
