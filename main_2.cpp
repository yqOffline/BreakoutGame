#include "raylib.h"
#include "game.h"
#include "RaceManager.h"
#include "VersusManager.h"
#include "TextureCache.h"
#include <fstream>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    const int baseWidth = 800;
    const int baseHeight = 600;

    InitWindow(baseWidth, baseHeight, "2DBREAKOUT!!!");
    InitAudioDevice();
    SetTargetFPS(60);

    Music bgm = LoadMusicStream("bgm.wav");
    if (bgm.stream.buffer != nullptr) {
        bgm.looping = true;
        PlayMusicStream(bgm);
        SetMusicVolume(bgm, 0.5f);
    } else {
        std::cerr << "Warning: Could not load bgm.wav, continuing without music." << std::endl;
    }

    json config;
    std::ifstream f("config.json");
    if (!f.is_open()) {
        std::cerr << "Failed to open config.json" << std::endl;
        CloseWindow();
        return -1;
    }
    f >> config;
    f.close();

    enum class ProgramState {
        SINGLE_PLAYER,
        RACE_LOBBY,
        RACE_PLAYING,
        VERSUS_LOBBY,
        VERSUS_PLAYING
    };
    ProgramState progState = ProgramState::SINGLE_PLAYER;

    Game* singleGame = new Game(baseWidth, baseHeight);
    RaceManager* raceManager = nullptr;
    VersusManager* versusManager = nullptr;
    // 创建 singleGame 后立即设置背景音乐
    singleGame = new Game(baseWidth, baseHeight);
    singleGame->SetBGM(bgm);   // bgm 是之前加载的 Music 变量

    // 初始化 paddle 纹理和 ball 纹理
    Paddle::LoadTexture("2.png");
    Ball::GenerateTexture();

    char selectedRole = 0;
    std::string remoteIP = "127.0.0.1";
    if (argc > 1) {
        remoteIP = argv[1];
        std::cout << "Guest will connect to " << remoteIP << std::endl;
    }
    uint16_t port = 1234;

    Rectangle hostBtn   = { baseWidth / 2.0f - 100, 200, 200, 50 };
    Rectangle guestBtn  = { baseWidth / 2.0f - 100, 270, 200, 50 };
    Rectangle readyBtn  = { baseWidth / 2.0f - 80,  350, 160, 50 };
    Rectangle backBtn   = { baseWidth / 2.0f - 60,  baseHeight - 80, 120, 50 };

    while (!WindowShouldClose()) {

        Vector2 mousePos = GetMousePosition();
        if (bgm.stream.buffer != nullptr) {
            UpdateMusicStream(bgm);
        }
        // ========== 单机模式 ==========
        if (progState == ProgramState::SINGLE_PLAYER) {
            singleGame->HandleInput(mousePos);
            singleGame->Update(GetFrameTime());

            BeginDrawing();
            TextureCache::Instance().UploadPendingTextures(); 
            ClearBackground(RAYWHITE);
            singleGame->Draw();
            EndDrawing();

            if (singleGame->ShouldExitToRaceLobby()) {
                delete singleGame;
                singleGame = nullptr;
                progState = ProgramState::RACE_LOBBY;
                selectedRole = 0;
            }
            if (singleGame && singleGame->ShouldExitToVersusLobby()) {
                delete singleGame;
                singleGame = nullptr;
                progState = ProgramState::VERSUS_LOBBY;
                selectedRole = 0;
            }
            if (!singleGame) continue;
        }

        else if (progState == ProgramState::RACE_LOBBY) {
            int sw = baseWidth;
            int sh = baseHeight;
            Vector2 mousePos = GetMousePosition();

            // 输入处理：热键选择角色和启动游戏
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
                if (selectedRole != 0) {
                    SetWindowSize(baseWidth * 2, baseHeight);
                    bool asHost = (selectedRole == 'H');
                    raceManager = new RaceManager(baseWidth, baseHeight, config, asHost, remoteIP, port);
                    progState = ProgramState::RACE_PLAYING;
                    continue;
                }
            }

            // 鼠标点击输入
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (CheckCollisionPointRec(mousePos, hostBtn)) {
                    selectedRole = (selectedRole == 'H') ? 0 : 'H';
                } else if (CheckCollisionPointRec(mousePos, guestBtn)) {
                    selectedRole = (selectedRole == 'G') ? 0 : 'G';
                } else if (CheckCollisionPointRec(mousePos, readyBtn)) {
                    if (selectedRole != 0) {
                        SetWindowSize(baseWidth * 2, baseHeight);
                        bool asHost = (selectedRole == 'H');
                        raceManager = new RaceManager(baseWidth, baseHeight, config, asHost, remoteIP, port);
                        progState = ProgramState::RACE_PLAYING;
                        continue;
                    }
                } else if (CheckCollisionPointRec(mousePos, backBtn)) {
                    progState = ProgramState::SINGLE_PLAYER;
                    singleGame = new Game(baseWidth, baseHeight);
                    continue;
                }
            }

            // 开始绘制
            BeginDrawing();
            TextureCache::Instance().UploadPendingTextures();

            // 背景图片
            Texture2D bg = TextureCache::Instance().GetTexture("1.png");
            if (bg.id != 0) {
                DrawTexturePro(bg,
                    {0, 0, (float)bg.width, (float)bg.height},
                    {0, 0, (float)sw, (float)sh},
                    {0, 0}, 0, WHITE);
            } else {
                ClearBackground(RAYWHITE);
            }

            // 半透明白色圆角面板（无遮罩）
            Rectangle panel = { sw/2.0f - 250, sh/2.0f - 160, 500, 320 };
            DrawRectangleRounded(panel, 0.2f, 10, Fade(WHITE, 0.85f));
            DrawRectangleLinesEx(panel, 2, BLACK);

            DrawText("RACE MODE - LOBBY", sw/2 - 160, sh/2 - 200, 36, DARKBLUE);
            DrawText("Select your role", sw/2 - 80, sh/2 - 70, 20, DARKGRAY);

            // Host 按钮（蓝色）
            Color hostNormal = BLUE;
            Color hostHover = DARKBLUE;
            Color hostSelected = DARKGREEN;
            bool hoverHost = CheckCollisionPointRec(mousePos, hostBtn);
            Color hostColor = (selectedRole == 'H') ? hostSelected : (hoverHost ? hostHover : hostNormal);
            DrawRectangleRounded(hostBtn, 0.2f, 8, hostColor);
            DrawRectangleLinesEx(hostBtn, 2, BLACK);
            DrawText("HOST (H)", hostBtn.x + 50, hostBtn.y + 12, 24, WHITE);

            // Guest 按钮（红色）
            Color guestNormal = RED;
            Color guestHover = MAROON;
            Color guestSelected = DARKGREEN;
            bool hoverGuest = CheckCollisionPointRec(mousePos, guestBtn);
            Color guestColor = (selectedRole == 'G') ? guestSelected : (hoverGuest ? guestHover : guestNormal);
            DrawRectangleRounded(guestBtn, 0.2f, 8, guestColor);
            DrawRectangleLinesEx(guestBtn, 2, BLACK);
            DrawText("GUEST (G)", guestBtn.x + 45, guestBtn.y + 12, 24, WHITE);

            // Ready 按钮（绿色）
            bool canReady = (selectedRole != 0);
            Color readyNormal = GREEN;
            Color readyHover = DARKGREEN;
            Color readyDisabled = LIGHTGRAY;
            Color readyColor = canReady ? (CheckCollisionPointRec(mousePos, readyBtn) ? readyHover : readyNormal) : readyDisabled;
            DrawRectangleRounded(readyBtn, 0.2f, 8, readyColor);
            DrawRectangleLinesEx(readyBtn, 2, BLACK);
            DrawText("READY (R)", readyBtn.x + 35, readyBtn.y + 12, 20, canReady ? BLACK : DARKGRAY);
            if (!canReady) {
                DrawText("Select HOST or GUEST first", sw/2 - 140, 420, 18, RED);
            }

            // Back 按钮（灰色）
            bool hoverBack = CheckCollisionPointRec(mousePos, backBtn);
            Color backColor = hoverBack ? DARKGRAY : GRAY;
            DrawRectangleRounded(backBtn, 0.2f, 8, backColor);
            DrawRectangleLinesEx(backBtn, 2, BLACK);
            DrawText("BACK (B)", backBtn.x + 20, backBtn.y + 12, 20, WHITE);

            // 提示文字
            DrawText("Host: wait for client, then click READY", sw/2 - 180, sh - 120, 16, DARKGRAY);
            DrawText("Guest: connect to host, wait for start", sw/2 - 180, sh - 100, 16, DARKGRAY);

            EndDrawing();
        }

        // ========== 竞速游戏中 ==========
        else if (progState == ProgramState::RACE_PLAYING) {
            if (raceManager) {
                raceManager->HandleInput();
                raceManager->Update(GetFrameTime());

                BeginDrawing();
                TextureCache::Instance().UploadPendingTextures(); 
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

        // ========== VERSUS Lobby ==========
        else if (progState == ProgramState::VERSUS_LOBBY) {
            int sw = baseWidth;
            int sh = baseHeight;
            Vector2 mousePos = GetMousePosition();

            // 输入处理
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
                if (selectedRole != 0) {
                    SetWindowSize(baseWidth * 1.5, (int)(baseHeight * 1.6));
                    bool asHost = (selectedRole == 'H');
                    versusManager = new VersusManager(baseWidth * 1.5, (int)(baseHeight * 1.6), config, asHost, remoteIP, port);
                    progState = ProgramState::VERSUS_PLAYING;
                    continue;
                }
            }

            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (CheckCollisionPointRec(mousePos, hostBtn)) {
                    selectedRole = (selectedRole == 'H') ? 0 : 'H';
                } else if (CheckCollisionPointRec(mousePos, guestBtn)) {
                    selectedRole = (selectedRole == 'G') ? 0 : 'G';
                } else if (CheckCollisionPointRec(mousePos, readyBtn)) {
                    if (selectedRole != 0) {
                        SetWindowSize(baseWidth * 1.5, (int)(baseHeight * 1.6));
                        bool asHost = (selectedRole == 'H');
                        versusManager = new VersusManager(baseWidth * 1.5, (int)(baseHeight * 1.6), config, asHost, remoteIP, port);
                        progState = ProgramState::VERSUS_PLAYING;
                        continue;
                    }
                } else if (CheckCollisionPointRec(mousePos, backBtn)) {
                    progState = ProgramState::SINGLE_PLAYER;
                    singleGame = new Game(baseWidth, baseHeight);
                    continue;
                }
            }

            BeginDrawing();
            TextureCache::Instance().UploadPendingTextures();

            // 背景图片（不加遮罩）
            Texture2D bg = TextureCache::Instance().GetTexture("1.png");
            if (bg.id != 0) {
                DrawTexturePro(bg,
                    {0, 0, (float)bg.width, (float)bg.height},
                    {0, 0, (float)sw, (float)sh},
                    {0, 0}, 0, WHITE);
            } else {
                ClearBackground(RAYWHITE);
            }

            // 半透明白色圆角面板
            Rectangle panel = { sw/2.0f - 250, sh/2.0f - 160, 500, 320 };
            DrawRectangleRounded(panel, 0.2f, 10, Fade(WHITE, 0.85f));
            DrawRectangleLinesEx(panel, 2, BLACK);

            DrawText("VERSUS MODE - LOBBY", sw/2 - 160, sh/2 - 200, 36, DARKBLUE);
            DrawText("Select your role", sw/2 - 80, sh/2 - 70, 20, DARKGRAY);

            // Host 按钮（蓝色）
            Color hostNormal = BLUE;
            Color hostHover = DARKBLUE;
            Color hostSelected = DARKGREEN;
            bool hoverHost = CheckCollisionPointRec(mousePos, hostBtn);
            Color hostColor = (selectedRole == 'H') ? hostSelected : (hoverHost ? hostHover : hostNormal);
            DrawRectangleRounded(hostBtn, 0.2f, 8, hostColor);
            DrawRectangleLinesEx(hostBtn, 2, BLACK);
            DrawText("HOST (H)", hostBtn.x + 50, hostBtn.y + 12, 24, WHITE);

            // Guest 按钮（红色）
            Color guestNormal = RED;
            Color guestHover = MAROON;
            Color guestSelected = DARKGREEN;
            bool hoverGuest = CheckCollisionPointRec(mousePos, guestBtn);
            Color guestColor = (selectedRole == 'G') ? guestSelected : (hoverGuest ? guestHover : guestNormal);
            DrawRectangleRounded(guestBtn, 0.2f, 8, guestColor);
            DrawRectangleLinesEx(guestBtn, 2, BLACK);
            DrawText("GUEST (G)", guestBtn.x + 45, guestBtn.y + 12, 24, WHITE);

            // Ready 按钮（绿色）
            bool canReady = (selectedRole != 0);
            Color readyNormal = GREEN;
            Color readyHover = DARKGREEN;
            Color readyDisabled = LIGHTGRAY;
            Color readyColor = canReady ? (CheckCollisionPointRec(mousePos, readyBtn) ? readyHover : readyNormal) : readyDisabled;
            DrawRectangleRounded(readyBtn, 0.2f, 8, readyColor);
            DrawRectangleLinesEx(readyBtn, 2, BLACK);
            DrawText("READY (R)", readyBtn.x + 35, readyBtn.y + 12, 20, canReady ? BLACK : DARKGRAY);
            if (!canReady) {
                DrawText("Select HOST or GUEST first", sw/2 - 140, 420, 18, RED);
            }

            // Back 按钮（灰色）
            bool hoverBack = CheckCollisionPointRec(mousePos, backBtn);
            Color backColor = hoverBack ? DARKGRAY : GRAY;
            DrawRectangleRounded(backBtn, 0.2f, 8, backColor);
            DrawRectangleLinesEx(backBtn, 2, BLACK);
            DrawText("BACK (B)", backBtn.x + 20, backBtn.y + 12, 20, WHITE);

            // 提示文字
            DrawText("Host: wait for client, then click READY", sw/2 - 180, sh - 120, 16, DARKGRAY);
            DrawText("Guest: connect to host, wait for start", sw/2 - 180, sh - 100, 16, DARKGRAY);

            EndDrawing();
        }

        // ========== VERSUS 游戏中 ==========
        else if (progState == ProgramState::VERSUS_PLAYING) {
            if (versusManager) {
                versusManager->HandleInput();
                versusManager->Update(GetFrameTime());

                BeginDrawing();
                TextureCache::Instance().UploadPendingTextures(); 
                ClearBackground(RAYWHITE);
                versusManager->Draw();
                EndDrawing();

                if (!versusManager->IsRunning()) {
                    delete versusManager;
                    versusManager = nullptr;
                    SetWindowSize(baseWidth, baseHeight);   // ★ 恢复窗口
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
    if (versusManager) delete versusManager;

    if (bgm.stream.buffer != nullptr) {
        UnloadMusicStream(bgm);
    }

    CloseAudioDevice();
    CloseWindow();
    return 0;
}