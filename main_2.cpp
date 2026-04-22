#include "raylib.h"
#include "game.h"
#include "RaceManager.h"
#include <fstream>
#include <iostream>
#include <string>

int main() {
    const int baseWidth = 800;
    const int baseHeight = 600;
    
    InitWindow(baseWidth, baseHeight, "2DBREAKOUT!!!");
    InitAudioDevice();
    SetTargetFPS(60);

    json config;
    std::ifstream f("config.json");
    if (!f.is_open()) {
        std::cerr << "Failed to open config.json" << std::endl;
        CloseWindow();
        return -1;
    }
    f >> config;
    f.close();

    enum class ProgramState { SINGLE_PLAYER, RACE_LOBBY, RACE_PLAYING };
    ProgramState progState = ProgramState::SINGLE_PLAYER;

    Game* singleGame = new Game(baseWidth, baseHeight);
    RaceManager* raceManager = nullptr;

    bool raceAsHost = true;
    std::string remoteIP = "127.0.0.1";
    uint16_t port = 1234;

    // Lobby 按钮布局
    Rectangle hostBtn   = { baseWidth / 2.0f - 150, 250, 120, 50 };
    Rectangle guestBtn  = { baseWidth / 2.0f + 30, 250, 120, 50 };
    Rectangle readyBtn  = { baseWidth / 2.0f - 60, 350, 120, 50 };
    Rectangle backBtn   = { baseWidth / 2.0f - 60, baseHeight - 80, 120, 50 };

    while (!WindowShouldClose()) {
        Vector2 mousePos = GetMousePosition();

        // ---------- 单机模式 ----------
        if (progState == ProgramState::SINGLE_PLAYER) {
            singleGame->HandleInput(mousePos);
            singleGame->Update(GetFrameTime());

            BeginDrawing();
            ClearBackground(RAYWHITE);
            singleGame->Draw();
            EndDrawing();

            if (singleGame->ShouldExitToRaceLobby()) {
                delete singleGame;
                singleGame = nullptr;
                progState = ProgramState::RACE_LOBBY;
                raceAsHost = true;
            }
            if (!singleGame) continue;
        }

        // ---------- 竞速模式 Lobby ----------
        else if (progState == ProgramState::RACE_LOBBY) {
            // 处理输入
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (CheckCollisionPointRec(mousePos, hostBtn)) {
                    raceAsHost = true;
                }
                else if (CheckCollisionPointRec(mousePos, guestBtn)) {
                    raceAsHost = false;
                }
                else if (CheckCollisionPointRec(mousePos, readyBtn)) {
                    // 无论 Host 还是 Guest，点击 READY 都进入竞速游戏状态
                    SetWindowSize(baseWidth * 2, baseHeight);
                    raceManager = new RaceManager(baseWidth, baseHeight, config, raceAsHost, remoteIP, port);
                    progState = ProgramState::RACE_PLAYING;
                }
                else if (CheckCollisionPointRec(mousePos, backBtn)) {
                    progState = ProgramState::SINGLE_PLAYER;
                    singleGame = new Game(baseWidth, baseHeight);
                }
            }

            BeginDrawing();
            ClearBackground(RAYWHITE);
            DrawText("RACE MODE - LOBBY", baseWidth/2 - 150, 120, 40, DARKBLUE);
            DrawText("Select role and press READY", baseWidth/2 - 150, 180, 20, DARKGRAY);

            // Host 按钮
            Color hostColor = raceAsHost ? GREEN : BLUE;
            DrawRectangleRec(hostBtn, CheckCollisionPointRec(mousePos, hostBtn) ? DARKGREEN : hostColor);
            DrawText("HOST", hostBtn.x + 35, hostBtn.y + 15, 20, WHITE);

            // Guest 按钮
            Color guestColor = !raceAsHost ? GREEN : BLUE;
            DrawRectangleRec(guestBtn, CheckCollisionPointRec(mousePos, guestBtn) ? DARKGREEN : guestColor);
            DrawText("GUEST", guestBtn.x + 25, guestBtn.y + 15, 20, WHITE);

            // READY 按钮（所有人都可见）
            DrawRectangleRec(readyBtn, CheckCollisionPointRec(mousePos, readyBtn) ? DARKGREEN : GREEN);
            DrawText("READY", readyBtn.x + 30, readyBtn.y + 15, 20, BLACK);

            // BACK 按钮
            DrawRectangleRec(backBtn, CheckCollisionPointRec(mousePos, backBtn) ? DARKGRAY : GRAY);
            DrawText("BACK", backBtn.x + 35, backBtn.y + 15, 20, WHITE);

            DrawText("Host: wait for client, then click START.", baseWidth/2 - 200, 430, 20, DARKGRAY);
            DrawText("Guest: connect to host, wait for start.", baseWidth/2 - 200, 460, 20, DARKGRAY);
            EndDrawing();
        }

        // ---------- 竞速模式游戏中 ----------
        else if (progState == ProgramState::RACE_PLAYING) {
            if (raceManager) {
                raceManager->HandleInput();
                raceManager->Update(GetFrameTime());

                BeginDrawing();
                ClearBackground(RAYWHITE);
                raceManager->Draw();
                EndDrawing();

                if (!raceManager->IsRunning()) {
                    delete raceManager;
                    raceManager = nullptr;
                    SetWindowSize(baseWidth, baseHeight);
                    progState = ProgramState::SINGLE_PLAYER;
                    singleGame = new Game(baseWidth, baseHeight);
                }
            } else {
                SetWindowSize(baseWidth, baseHeight);
                progState = ProgramState::SINGLE_PLAYER;
                singleGame = new Game(baseWidth, baseHeight);
            }
        }
    }

    if (singleGame) delete singleGame;
    if (raceManager) delete raceManager;

    CloseAudioDevice();
    CloseWindow();
    return 0;
}