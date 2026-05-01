#include "RaceManager.h"
#include "TextureCache.h"
#include <iostream>
#include <ctime>
#include "rlgl.h"

RaceManager::RaceManager(int baseWidth, int baseHeight, const json& cfg, bool asHost, const std::string& ip, uint16_t port)
    : winWidth(baseWidth * 2), winHeight(baseHeight), gameWidth(baseWidth), gameHeight(baseHeight), config(cfg),
      leftPlayer(gameWidth, gameHeight, config, true),
      rightPlayer(gameWidth, gameHeight, config, false),
      role(asHost ? NetworkRole::HOST : NetworkRole::GUEST),
      running(true), gameStarted(false), gamePaused(false),
      localResult(RaceResult::NONE), networkError(false),
      m_backToLobby(false)
{
    bgLeft  = TextureCache::Instance().GetTexture("1.png");
    bgRight = TextureCache::Instance().GetTexture("3.png");

    // 设置随机种子
    unsigned int seed = (unsigned int)time(nullptr);
    leftPlayer.SetRandomSeed(seed);
    rightPlayer.SetRandomSeed(seed);
    leftPlayer.LoadLevel(0);
    rightPlayer.LoadLevel(0);

    // 创建并启动网络线程
    networkThread = std::make_unique<NetworkThread>(asHost, ip, port);
    networkThread->Start();

    continueBtn = { winWidth / 2.0f - 60, winHeight / 2.0f - 25, 120, 50 };
    restartBtn  = { winWidth / 2.0f - 60, winHeight / 2.0f + 40, 120, 50 };
}

RaceManager::~RaceManager() {

}

void RaceManager::SendNetMessage(const NetMessage& msg, bool reliable) {
    if (networkThread && networkThread->IsRunning()) {
        networkThread->SendMessage(msg, reliable);
    }
}

void RaceManager::ProcessIncomingMessages() {
    if (!networkThread || !networkThread->IsRunning()) {
        if (!networkError) {
            networkError = networkThread->HasError();
            if (networkError) std::cerr << "Network thread error!" << std::endl;
        }
        return;
    }

    NetMessage msg;
    while (networkThread->TryRecvMessage(msg)) {
        switch (msg.type) {
            case NetMsgType::CONTROL_START:   StartGame(); break;
            case NetMsgType::CONTROL_PAUSE:   SetPaused(true); break;
            case NetMsgType::CONTROL_RESUME:  SetPaused(false); break;
            case NetMsgType::PADDLE_POSITION: rightPlayer.SetOpponentPaddleX(msg.floatData); break;
            case NetMsgType::GAME_OVER_NOTIFY: rightPlayer.ForceGameOver(); break;
            case NetMsgType::VICTORY_NOTIFY:   rightPlayer.ForceVictory(); break;
            case NetMsgType::RESULT_NOTIFY:
                localResult = (msg.data1 == 1) ? RaceResult::LOSE : RaceResult::WIN;
                gameStarted = false;
                if (localResult == RaceResult::WIN) soundManager.PlayGameVictory();
                else soundManager.PlayGameOver();
                break;
            case NetMsgType::RANDOM_SEED:
                leftPlayer.SetRandomSeed(msg.data1);
                rightPlayer.SetRandomSeed(msg.data1);
                leftPlayer.LoadLevel(0);
                rightPlayer.LoadLevel(0);
                break;
            default: break;
        }
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
        SendNetMessage(resultMsg);
        if (localResult == RaceResult::WIN) soundManager.PlayGameVictory();
        else soundManager.PlayGameOver();
    }
}

void RaceManager::Update(float dt) {
    if (!running) return;

    ProcessIncomingMessages();

    if (gameStarted && !gamePaused) {
        leftPlayer.Update(dt);
        rightPlayer.Update(dt);

        // 发送挡板位置
        float paddleX = leftPlayer.GetPaddleX();
        NetMessage msg{ NetMsgType::PADDLE_POSITION, 0, 0, paddleX };
        SendNetMessage(msg, false);

        if (role == NetworkRole::HOST) CheckGameEndCondition();
    }

    if (networkThread && networkThread->HasError()) {
        networkError = true;
        running = false;
    }
}

void RaceManager::HandleInput() {
    Vector2 mousePos = GetMousePosition();

    if (localResult != RaceResult::NONE) {
        if ((IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, restartBtn)) || IsKeyPressed(KEY_R))
            ResetGame();
        return;
    }

    if (networkError) {
        if (IsKeyPressed(KEY_B)) running = false;
        return;
    }

    if (!gameStarted) {
        if (IsKeyPressed(KEY_B)) {
            m_backToLobby = true;
            running = false;
            return;
        }
        Rectangle backBtn = { winWidth / 2.0f - 60, winHeight / 2.0f + 90, 120, 50 };
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, backBtn)) {
            m_backToLobby = true;
            running = false;
            return;
        }

        if (role == NetworkRole::HOST && networkThread && networkThread->IsRunning()) {
            Rectangle startBtn = { winWidth / 2.0f - 60, winHeight / 2.0f + 30, 120, 50 };
            if ((IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, startBtn)) || IsKeyPressed(KEY_S)) {
                StartGame();
                NetMessage msg{ NetMsgType::CONTROL_START };
                SendNetMessage(msg);
                return;
            }
        }
        return;
    }

    leftPlayer.HandleInput();
    if (role == NetworkRole::HOST && IsKeyPressed(KEY_SPACE)) {
        SetPaused(!gamePaused);
        NetMessage ctrl{ gamePaused ? NetMsgType::CONTROL_PAUSE : NetMsgType::CONTROL_RESUME };
        SendNetMessage(ctrl);
    }
    if (gamePaused && role == NetworkRole::HOST) {
        if ((IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mousePos, continueBtn)) || IsKeyPressed(KEY_SPACE)) {
            SetPaused(false);
            NetMessage msg{ NetMsgType::CONTROL_RESUME };
            SendNetMessage(msg);
        }
    }
}

void RaceManager::ResetGame() {
    leftPlayer.ResetForNewGame();
    rightPlayer.ResetForNewGame();
    localResult = RaceResult::NONE;
    gameStarted = false;
    running = false;
}

// ---------- 绘制函数 ----------
void RaceManager::Draw() {
    // 左半屏背景 + 游戏元素
    BeginScissorMode(0, 0, gameWidth, gameHeight);
    DrawTexturePro(bgLeft, {0,0,(float)bgLeft.width,(float)bgLeft.height},
                   {0,0,(float)gameWidth,(float)gameHeight}, {0,0}, 0, WHITE);
    leftPlayer.Draw();
    EndScissorMode();

    // 右半屏背景 + 游戏元素
    BeginScissorMode(gameWidth, 0, gameWidth, gameHeight);
    DrawTexturePro(bgRight, {0,0,(float)bgRight.width,(float)bgRight.height},
                   {(float)gameWidth, 0, (float)gameWidth, (float)gameHeight}, {0,0}, 0, WHITE);
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
        DrawText("Press B to return", winWidth/2 - 180, winHeight/2 + 30, 30, WHITE);
        return;
    }

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
        statusText = networkThread && networkThread->IsRunning() ? "Client connected! Press START to begin." : "Waiting for client...";
    } else {
        statusText = networkThread && networkThread->IsRunning() ? "Connected to host. Waiting for start..." : "Connecting to host...";
    }
    int tw = MeasureText(statusText, 30);
    DrawText(statusText, winWidth/2 - tw/2, winHeight/2 - 15, 30, WHITE);

    // START 按钮（仅 Host 且已连接）
    if (role == NetworkRole::HOST && networkThread && networkThread->IsRunning()) {
        Rectangle startBtn = { winWidth / 2.0f - 60, winHeight / 2.0f + 30, 120, 50 };
        DrawRectangleRec(startBtn, (CheckCollisionPointRec(GetMousePosition(), startBtn) || IsKeyPressed(KEY_S)) ? DARKGREEN : GREEN);
        DrawText("START (S)", startBtn.x + 10, startBtn.y + 15, 20, BLACK);
    }

    // BACK 按钮（通用）
    Rectangle backBtn = { winWidth / 2.0f - 60, winHeight / 2.0f + 90, 120, 50 };
    DrawRectangleRec(backBtn, (CheckCollisionPointRec(GetMousePosition(), backBtn) || IsKeyPressed(KEY_B)) ? DARKGRAY : GRAY);
    DrawText("BACK (B)", backBtn.x + 20, backBtn.y + 15, 20, WHITE);
}

void RaceManager::DrawPauseScreen() {
    DrawRectangle(0, 0, winWidth, winHeight, Fade(BLACK, 0.7f));
    DrawText("PAUSED", winWidth/2 - 70, winHeight/2 - 60, 50, WHITE);
    if (role == NetworkRole::HOST) {
        DrawRectangleRec(continueBtn, (CheckCollisionPointRec(GetMousePosition(), continueBtn) || IsKeyPressed(KEY_SPACE)) ? DARKBLUE : BLUE);
        DrawText("CONTINUE (SPACE)", continueBtn.x - 10, continueBtn.y + 15, 20, WHITE);
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
    DrawRectangleRec(restartBtn, (CheckCollisionPointRec(GetMousePosition(), restartBtn) || IsKeyPressed(KEY_R)) ? DARKGREEN : GREEN);
    DrawText("RESTART (R)", restartBtn.x + 10, restartBtn.y + 15, 20, BLACK);
}