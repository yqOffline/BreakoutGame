#include "RaceManager.h"
#include <iostream>
#include <cstring>
#include "rlgl.h"

RaceManager::RaceManager(int baseWidth, int baseHeight, const json& cfg, bool asHost, const std::string& ip, uint16_t port)
    : winWidth(baseWidth * 2), winHeight(baseHeight), gameWidth(baseWidth), gameHeight(baseHeight), config(cfg),
      leftPlayer(gameWidth, gameHeight, config, true),
      rightPlayer(gameWidth, gameHeight, config, false),
      role(asHost ? NetworkRole::HOST : NetworkRole::GUEST),
      running(true), gameStarted(false), gamePaused(false),
      localResult(RaceResult::NONE), host(nullptr), peer(nullptr),
      remoteIP(ip), remotePort(port), pendingSeed(0), networkError(false)
{
    if (enet_initialize() != 0) {
        std::cerr << "Failed to initialize ENet!" << std::endl;
        networkError = true;
        return;
    }

    // 先设置相同种子，再加载关卡
    pendingSeed = (unsigned int)time(nullptr);
    leftPlayer.SetRandomSeed(pendingSeed);
    rightPlayer.SetRandomSeed(pendingSeed);
    leftPlayer.LoadLevel(0);
    rightPlayer.LoadLevel(0);

    InitNetwork(asHost, ip, port);

    continueBtn = { winWidth / 2.0f - 60, winHeight / 2.0f - 25, 120, 50 };
    restartBtn  = { winWidth / 2.0f - 60, winHeight / 2.0f + 40, 120, 50 };
}

RaceManager::~RaceManager() {
    if (host) enet_host_destroy(host);
    enet_deinitialize();
}

void RaceManager::InitNetwork(bool asHost, const std::string& ip, uint16_t port) {
    if (asHost) {
        ENetAddress address;
        address.host = ENET_HOST_ANY;
        address.port = port;
        host = enet_host_create(&address, 2, 2, 0, 0);
        if (!host) {
            std::cerr << "Failed to create host!" << std::endl;
            networkError = true;
            return;
        }
        std::cout << "Host created on port " << port << std::endl;
    } else {
        host = enet_host_create(nullptr, 1, 2, 0, 0);
        if (!host) {
            std::cerr << "Failed to create client host!" << std::endl;
            networkError = true;
            return;
        }
        ENetAddress address;
        enet_address_set_host(&address, ip.c_str());
        address.port = port;
        peer = enet_host_connect(host, &address, 2, 0);
        if (!peer) {
            std::cerr << "Failed to initiate connection!" << std::endl;
            networkError = true;
            return;
        }
        std::cout << "Connecting to " << ip << ":" << port << std::endl;

        ENetEvent event;
        if (enet_host_service(host, &event, 3000) > 0 && event.type == ENET_EVENT_TYPE_CONNECT) {
            std::cout << "Connected to host!" << std::endl;
            peer = event.peer;
        } else {
            std::cerr << "Connection timeout!" << std::endl;
            networkError = true;
        }
    }
}

void RaceManager::SendMessage(const NetMessage& msg, bool reliable) {
    if (!peer) return;
    ENetPacket* packet = enet_packet_create(&msg, sizeof(msg),
        reliable ? ENET_PACKET_FLAG_RELIABLE : ENET_PACKET_FLAG_UNSEQUENCED);
    enet_peer_send(peer, 0, packet);
}

void RaceManager::ProcessNetworkEvents() {
    if (networkError) return;
    ENetEvent event;
    while (enet_host_service(host, &event, 0) > 0) {
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT:
                std::cout << "Peer connected!" << std::endl;
                peer = event.peer;
                if (role == NetworkRole::HOST) {
                    NetMessage seedMsg{ NetMsgType::RANDOM_SEED, pendingSeed, 0, 0.0f };
                    SendMessage(seedMsg);
                }
                break;
            case ENET_EVENT_TYPE_RECEIVE:
                HandlePacket(event.packet);
                enet_packet_destroy(event.packet);
                break;
            case ENET_EVENT_TYPE_DISCONNECT:
                std::cout << "Peer disconnected!" << std::endl;
                peer = nullptr;
                if (localResult == RaceResult::NONE) {
                    localResult = RaceResult::WIN;
                    gameStarted = false;
                    soundManager.PlayGameVictory();
                }
                break;
            default: break;
        }
    }
}

void RaceManager::HandlePacket(ENetPacket* packet) {
    if (packet->dataLength != sizeof(NetMessage)) return;
    NetMessage* msg = (NetMessage*)packet->data;

    switch (msg->type) {
        case NetMsgType::CONTROL_START:   StartGame(); break;
        case NetMsgType::CONTROL_PAUSE:   SetPaused(true); break;
        case NetMsgType::CONTROL_RESUME:  SetPaused(false); break;
        case NetMsgType::PADDLE_POSITION: rightPlayer.SetOpponentPaddleX(msg->floatData); break;
        case NetMsgType::GAME_OVER_NOTIFY: rightPlayer.ForceGameOver(); break;
        case NetMsgType::VICTORY_NOTIFY:   rightPlayer.ForceVictory(); break;
        case NetMsgType::RESULT_NOTIFY:
            localResult = (msg->data1 == 1) ? RaceResult::LOSE : RaceResult::WIN;
            gameStarted = false;
            if (localResult == RaceResult::WIN) soundManager.PlayGameVictory();
            else soundManager.PlayGameOver();
            break;
        case NetMsgType::RANDOM_SEED:
            leftPlayer.SetRandomSeed(msg->data1);
            rightPlayer.SetRandomSeed(msg->data1);
            leftPlayer.LoadLevel(0);
            rightPlayer.LoadLevel(0);
            break;
        default: break;
    }
}

void RaceManager::StartGame() {
    if (networkError) return;
    leftPlayer.StartGame();
    rightPlayer.StartGame();
    gameStarted = true;
    gamePaused = false;
}

void RaceManager::SetPaused(bool paused) {
    leftPlayer.SetPaused(paused);
    rightPlayer.SetPaused(paused);
    gamePaused = paused;
}

void RaceManager::CheckGameEndCondition() {
    if (networkError || role != NetworkRole::HOST) return;

    bool leftOver    = leftPlayer.IsGameOver();
    bool rightOver   = rightPlayer.IsGameOver();
    bool leftVictory = leftPlayer.IsVictory();
    bool rightVictory= rightPlayer.IsVictory();

    RaceResult hostResult = RaceResult::NONE;
    NetMessage resultMsg{ NetMsgType::RESULT_NOTIFY, 0, 0, 0.0f };

    if (leftOver && !rightOver) {
        hostResult = RaceResult::LOSE;
        resultMsg.data1 = 0;
    } else if (rightOver && !leftOver) {
        hostResult = RaceResult::WIN;
        resultMsg.data1 = 1;
    } else if (leftVictory && rightVictory) {
        float lt = leftPlayer.GetGameTime(), rt = rightPlayer.GetGameTime();
        int ld = leftPlayer.GetDeaths(), rd = rightPlayer.GetDeaths();
        if (lt < rt || (lt == rt && ld < rd)) {
            hostResult = RaceResult::WIN;
            resultMsg.data1 = 1;
        } else {
            hostResult = RaceResult::LOSE;
            resultMsg.data1 = 0;
        }
    } else if (leftOver && rightOver) {
        int ld = leftPlayer.GetDeaths(), rd = rightPlayer.GetDeaths();
        float lt = leftPlayer.GetGameTime(), rt = rightPlayer.GetGameTime();
        if (ld < rd || (ld == rd && lt < rt)) {
            hostResult = RaceResult::WIN;
            resultMsg.data1 = 1;
        } else {
            hostResult = RaceResult::LOSE;
            resultMsg.data1 = 0;
        }
    }

    if (hostResult != RaceResult::NONE && localResult == RaceResult::NONE) {
        localResult = hostResult;
        gameStarted = false;
        SendMessage(resultMsg);
        if (hostResult == RaceResult::WIN) soundManager.PlayGameVictory();
        else soundManager.PlayGameOver();
    }
}

void RaceManager::Update(float dt) {
    if (!running) return;

    ProcessNetworkEvents();

    if (gameStarted && !gamePaused) {
        leftPlayer.Update(dt);
        rightPlayer.Update(dt);

        float paddleX = leftPlayer.GetPaddleX();
        NetMessage msg{ NetMsgType::PADDLE_POSITION, 0, 0, paddleX };
        SendMessage(msg, false);

        if (role == NetworkRole::HOST) {
            CheckGameEndCondition();
        }
    }
}

void RaceManager::HandleInput() {
    Vector2 mousePos = GetMousePosition();

    // ★ 结果界面（最高优先级）
    if (localResult != RaceResult::NONE) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, restartBtn)) {
            ResetGame();
        }
        return;
    }

    // 网络错误时只允许退出
    if (networkError) {
        if (IsKeyPressed(KEY_BACKSPACE)) running = false;
        return;
    }

    // ───────── LOBBY 阶段 ─────────
    if (!gameStarted) {
        // 按 BACKSPACE 退出竞速模式
        if (IsKeyPressed(KEY_BACKSPACE)) {
            running = false;
            return;
        }

        // Host 且已连接时，可以点击 START 按钮开始游戏
        if (role == NetworkRole::HOST && peer) {
            Rectangle startBtn = { winWidth / 2.0f - 60, winHeight / 2.0f + 30, 120, 50 };
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, startBtn)) {
                StartGame();
                NetMessage msg{ NetMsgType::CONTROL_START };
                SendMessage(msg);
                return;
            }
        }
        return;   // Lobby 不处理其他输入
    }

    // ───────── 游戏进行中 ─────────
    // 本地挡板移动
    leftPlayer.HandleInput();

    // 暂停/继续（仅 Host 可通过空格键切换）
    if (role == NetworkRole::HOST && IsKeyPressed(KEY_SPACE)) {
        SetPaused(!gamePaused);
        NetMessage ctrl{ gamePaused ? NetMsgType::CONTROL_PAUSE : NetMsgType::CONTROL_RESUME };
        SendMessage(ctrl);
    }

    // 暂停时 Host 的继续按钮（鼠标）
    if (gamePaused && role == NetworkRole::HOST) {
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, continueBtn)) {
            SetPaused(false);
            NetMessage msg{ NetMsgType::CONTROL_RESUME };
            SendMessage(msg);
        }
    }
}

void RaceManager::ResetGame() {
    leftPlayer.ResetForNewGame();
    rightPlayer.ResetForNewGame();
    localResult = RaceResult::NONE;
    gameStarted = false;
    running = false;   // 退出到 LOBBY
}

void RaceManager::Draw() {
    ClearBackground(RAYWHITE);

    BeginScissorMode(0, 0, gameWidth, gameHeight);
    leftPlayer.Draw();
    EndScissorMode();

    BeginScissorMode(gameWidth, 0, gameWidth, gameHeight);
    rlPushMatrix();
    rlTranslatef(gameWidth, 0, 0);
    rightPlayer.Draw();
    rlPopMatrix();
    EndScissorMode();

    DrawLine(gameWidth, 0, gameWidth, winHeight, WHITE);
    DrawLine(gameWidth - 2, 0, gameWidth - 2, winHeight, GRAY);

    if (networkError) {
        DrawRectangle(0, 0, winWidth, winHeight, Fade(BLACK, 0.7f));
        DrawText("NETWORK ERROR", winWidth/2 - 150, winHeight/2 - 30, 40, RED);
        DrawText("Press BACKSPACE to return", winWidth/2 - 180, winHeight/2 + 30, 30, WHITE);
        return;
    }

    // 结果界面最高优先级
    if (localResult != RaceResult::NONE) {
        DrawResultScreen();
        return;
    }

    if (!gameStarted)
        DrawLobby();
    else if (gamePaused)
        DrawPauseScreen();
}

void RaceManager::DrawLobby() {
    DrawRectangle(0, 0, winWidth, winHeight, Fade(BLACK, 0.5f));
    const char* statusText = nullptr;
    if (role == NetworkRole::HOST) {
        statusText = peer ? "Client connected! Press START to begin." : "Waiting for client...";
    } else {
        statusText = peer ? "Connected to host. Waiting for start..." : "Connecting to host... (Press BACKSPACE to cancel)";
    }
    int tw = MeasureText(statusText, 30);
    DrawText(statusText, winWidth/2 - tw/2, winHeight/2 - 15, 30, WHITE);

    // Host 且已连接时显示 START 按钮（纯绘制，交互在 HandleInput 中）
    if (role == NetworkRole::HOST && peer) {
        Rectangle startBtn = { winWidth/2.0f - 60, winHeight/2.0f + 30, 120, 50 };
        DrawRectangleRec(startBtn, CheckCollisionPointRec(GetMousePosition(), startBtn) ? DARKGREEN : GREEN);
        DrawText("START", startBtn.x + 30, startBtn.y + 15, 20, BLACK);
    }
}

void RaceManager::DrawPauseScreen() {
    DrawRectangle(0, 0, winWidth, winHeight, Fade(BLACK, 0.7f));
    DrawText("PAUSED", winWidth/2 - 70, winHeight/2 - 60, 50, WHITE);
    if (role == NetworkRole::HOST) {
        DrawRectangleRec(continueBtn, CheckCollisionPointRec(GetMousePosition(), continueBtn) ? DARKBLUE : BLUE);
        DrawText("CONTINUE", continueBtn.x + 20, continueBtn.y + 15, 20, WHITE);
    } else {
        DrawText("Waiting for host...", winWidth/2 - 120, winHeight/2 + 20, 30, GRAY);
    }
}

void RaceManager::DrawResultScreen() {
    DrawRectangle(0, 0, winWidth, winHeight, Fade(BLACK, 0.8f));
    const char* text = (localResult == RaceResult::WIN) ? "YOU WIN!" : "YOU LOSE!";
    int fontSize = 70;
    int tw = MeasureText(text, fontSize);
    Color tc = (localResult == RaceResult::WIN) ? GOLD : RED;
    DrawText(text, winWidth/2 - tw/2, winHeight/2 - 60, fontSize, tc);
    DrawRectangleRec(restartBtn, CheckCollisionPointRec(GetMousePosition(), restartBtn) ? DARKGREEN : GREEN);
    DrawText("RESTART", restartBtn.x + 20, restartBtn.y + 15, 20, BLACK);
}