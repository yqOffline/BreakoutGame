#ifndef RACE_MANAGER_H
#define RACE_MANAGER_H

#include "raylib.h"
#include "RacePlayer.h"
#include "SoundManager.h"
#include "json.hpp"
#include <enet/enet.h>
#include <string>

using json = nlohmann::json;

enum class NetworkRole { NONE, HOST, GUEST };
enum class RaceResult { NONE, WIN, LOSE };

enum class NetMsgType : uint8_t {
    CONTROL_START,
    CONTROL_PAUSE,
    CONTROL_RESUME,
    PADDLE_POSITION,
    GAME_OVER_NOTIFY,
    VICTORY_NOTIFY,
    RESULT_NOTIFY,
    RANDOM_SEED
};

#pragma pack(push, 1)
struct NetMessage {
    NetMsgType type;
    uint32_t data1;
    uint32_t data2;
    float floatData;
};
#pragma pack(pop)

class RaceManager {
public:
    RaceManager(int baseWidth, int baseHeight, const json& config,
                bool asHost, const std::string& ip, uint16_t port);
    ~RaceManager();

    void Update(float dt);
    void Draw();
    void HandleInput();
    bool IsRunning() const { return running; }
    bool IsGameStarted() const { return gameStarted; }   // 新增，方便外部查询
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

    ENetHost* host;
    ENetPeer* peer;
    std::string remoteIP;
    uint16_t remotePort;
    unsigned int pendingSeed;

    Rectangle continueBtn;
    Rectangle restartBtn;
    SoundManager soundManager;

    bool networkError;
    bool m_backToLobby = false;

    // 背景纹理
    Texture2D bgLeft;
    Texture2D bgRight;

    void InitNetwork(bool asHost, const std::string& ip, uint16_t port);
    void SendMessage(const NetMessage& msg, bool reliable = true);
    void ProcessNetworkEvents();
    void HandlePacket(ENetPacket* packet);
    void CheckGameEndCondition();
    void SetPaused(bool paused);
    void StartGame();
    void ResetGame();

    // ★ 这三个绘制函数必须声明
    void DrawLobby();
    void DrawPauseScreen();
    void DrawResultScreen();
};

#endif