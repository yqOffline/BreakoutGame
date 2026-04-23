#include "raylib.h"
#include "game.h"
#include "RaceManager.h"
#include <fstream>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
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

    // 选中状态：'H' = Host, 'G' = Guest, 0 = 未选择
    char selectedRole = 0;
    std::string remoteIP = "127.0.0.1";
    if (argc > 1) {
        remoteIP = argv[1];
        std::cout << "Guest will connect to " << remoteIP << std::endl;
    }
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
                selectedRole = 0;   // 重置选中状态
            }
            if (!singleGame) continue;
        }

        // ---------- 竞速模式 Lobby ----------
        else if (progState == ProgramState::RACE_LOBBY) {
            // 键盘输入
            if (IsKeyPressed(KEY_H)) {
                selectedRole = (selectedRole == 'H') ? 0 : 'H';
            }
            if (IsKeyPressed(KEY_G)) {
                selectedRole = (selectedRole == 'G') ? 0 : 'G';
            }
            if (IsKeyPressed(KEY_B)) {
                progState = ProgramState::SINGLE_PLAYER;
                singleGame = new Game(baseWidth, baseHeight);
                continue;
            }
            if (IsKeyPressed(KEY_R)) {
                // 只有选中角色后才能 READY
                if (selectedRole != 0) {
                    SetWindowSize(baseWidth * 2, baseHeight);
                    bool asHost = (selectedRole == 'H');
                    raceManager = new RaceManager(baseWidth, baseHeight, config, asHost, remoteIP, port);
                    progState = ProgramState::RACE_PLAYING;
                    continue;
                }
            }

            // 鼠标输入
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (CheckCollisionPointRec(mousePos, hostBtn)) {
                    selectedRole = (selectedRole == 'H') ? 0 : 'H';
                }
                else if (CheckCollisionPointRec(mousePos, guestBtn)) {
                    selectedRole = (selectedRole == 'G') ? 0 : 'G';
                }
                else if (CheckCollisionPointRec(mousePos, readyBtn)) {
                    if (selectedRole != 0) {
                        SetWindowSize(baseWidth * 2, baseHeight);
                        bool asHost = (selectedRole == 'H');
                        raceManager = new RaceManager(baseWidth, baseHeight, config, asHost, remoteIP, port);
                        progState = ProgramState::RACE_PLAYING;
                        continue;
                    }
                }
                else if (CheckCollisionPointRec(mousePos, backBtn)) {
                    progState = ProgramState::SINGLE_PLAYER;
                    singleGame = new Game(baseWidth, baseHeight);
                    continue;
                }
            }

            BeginDrawing();
            ClearBackground(RAYWHITE);
            DrawText("RACE MODE - LOBBY", baseWidth/2 - 150, 120, 40, DARKBLUE);
            DrawText("Select role and press READY", baseWidth/2 - 150, 180, 20, DARKGRAY);

            // Host 按钮：选中时持续变暗（DARKGREEN），未选中为标准颜色（GREEN），鼠标悬停再加 Fade
            Color hostColor = (selectedRole == 'H') ? DARKBLUE : BLUE;
            if (CheckCollisionPointRec(mousePos, hostBtn)) hostColor = Fade(hostColor, 0.7f);
            DrawRectangleRec(hostBtn, hostColor);
            DrawText("HOST (H)", hostBtn.x + 20, hostBtn.y + 15, 20, WHITE);

            // Guest 按钮：选中时变暗（DARKBLUE），未选中为 BLUE
            Color guestColor = (selectedRole == 'G') ? DARKBLUE : BLUE;
            if (CheckCollisionPointRec(mousePos, guestBtn)) guestColor = Fade(guestColor, 0.7f);
            DrawRectangleRec(guestBtn, guestColor);
            DrawText("GUEST (G)", guestBtn.x + 15, guestBtn.y + 15, 20, WHITE);

            // READY 按钮：如果未选择角色则灰色不可点击，选中后绿色可点击
            Color readyColor = (selectedRole != 0) ? GREEN : GRAY;
            if (selectedRole != 0 && CheckCollisionPointRec(mousePos, readyBtn)) readyColor = DARKGREEN;
            DrawRectangleRec(readyBtn, readyColor);
            DrawText("READY (R)", readyBtn.x + 30, readyBtn.y + 15, 20, BLACK);

            // BACK 按钮
            DrawRectangleRec(backBtn, CheckCollisionPointRec(mousePos, backBtn) ? DARKGRAY : GRAY);
            DrawText("BACK (B)", backBtn.x + 35, backBtn.y + 15, 20, WHITE);

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