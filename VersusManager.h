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
#include <deque>

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

    Texture2D background;

    // ========== 新增：丢包模拟与插值 ==========
    bool simulatePacketLoss = false;           // 是否启用丢包模拟（仅Host端有效）
    float packetLossRate = 0.3f;               // 丢包率 (0.0~1.0)
    std::deque<GameStateSnapshot> snapshotBuffer; // 客户端插值缓冲区
    float interpolationDelay = 0.1f;           // 插值延迟（秒）
    float latestHostTime = 0.0f;               // 最新收到的主机时间

    void AddSnapshotToBuffer(const GameStateSnapshot& snap);   // 将快照加入缓冲区
    void ApplyInterpolation(float renderTime);                 // 根据目标渲染时间插值
};

#endif