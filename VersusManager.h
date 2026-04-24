#ifndef VERSUS_MANAGER_H
#define VERSUS_MANAGER_H

#include "raylib.h"
#include "VersusGame.h"
#include "VersusNetMessage.h"
#include "SoundManager.h"
#include "json.hpp"
#include <enet/enet.h>
#include <string>
#include <functional>

using json = nlohmann::json;

class VersusManager {
public:
    VersusManager(int winWidth, int winHeight, const json& config,
                  bool asHost, const std::string& ip, uint16_t port);
    ~VersusManager();

    void Update(float dt);
    void Draw();
    void HandleInput();
    bool IsRunning() const { return running; }

private:
    int winWidth, winHeight;
    json config;

    VersusGame game;               // 游戏实例
    bool isHost;
    bool running;
    bool gameStarted;
    bool networkError;

    ENetHost* host;
    ENetPeer* peer;

    SoundManager soundManager;

    // 网络相关
    void InitNetwork(bool asHost, const std::string& ip, uint16_t port);
    void SendMessage(const VersusNetMessage& msg, bool reliable = true);
    void SendGameState();
    void ProcessNetworkEvents();
    void HandlePacket(ENetPacket* packet);

    // 游戏事件回调
    void OnLifeLost();
    void OnBallLaunched(bool isUpper);
    void OnEffectApplied(EffectType type, bool upper);
    void OnEffectRemoved(EffectType type, bool upper);
    void OnSkillBallSpawned(const SkillBall& sb);

    // 状态
    unsigned int pendingSeed;
    bool drawResult;
    std::string resultText;

    // UI 按钮
    Rectangle startBtn;
    Rectangle restartBtn;
    Rectangle backBtn;
};

#endif