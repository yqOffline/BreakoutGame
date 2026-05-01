#ifndef RACE_MANAGER_H
#define RACE_MANAGER_H

#include "raylib.h"
#include "RacePlayer.h"
#include "SoundManager.h"
#include "NetworkProtocol.h"
#include "NetworkThread.h"
#include "json.hpp"
#include <memory>

using json = nlohmann::json;

enum class NetworkRole { NONE, HOST, GUEST };
enum class RaceResult { NONE, WIN, LOSE };

class RaceManager {
public:
    RaceManager(int baseWidth, int baseHeight, const json& config,
                bool asHost, const std::string& ip, uint16_t port);
    ~RaceManager();

    void Update(float dt);
    void Draw();
    void HandleInput();
    bool IsRunning() const { return running; }
    bool IsGameStarted() const { return gameStarted; }
    bool ShouldBackToLobby() const { return m_backToLobby; }

private:
    int winWidth, winHeight;
    int gameWidth, gameHeight;
    json config;

    RacePlayer leftPlayer;
    RacePlayer rightPlayer;

    NetworkRole role;
    bool running;
    bool gameStarted;
    bool gamePaused;
    RaceResult localResult;

    std::unique_ptr<NetworkThread> networkThread;

    void SendNetMessage(const NetMessage& msg, bool reliable = true);
    void ProcessIncomingMessages();

    Rectangle continueBtn;
    Rectangle restartBtn;
    SoundManager soundManager;

    bool networkError;
    bool m_backToLobby = false;

    Texture2D bgLeft;
    Texture2D bgRight;

    void CheckGameEndCondition();
    void SetPaused(bool paused);
    void StartGame();
    void ResetGame();

    void DrawLobby();
    void DrawPauseScreen();
    void DrawResultScreen();
};

#endif