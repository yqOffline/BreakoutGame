#ifndef VERSUS_MANAGER_H
#define VERSUS_MANAGER_H

#include "raylib.h"
#include "VersusGame.h"
#include "VersusNetMessage.h"
#include "SoundManager.h"
#include "ThreadSafeQueue.h"
#include "json.hpp"
#include <thread>
#include <atomic>
#include <vector>
#include <string>
#include <deque>

using json = nlohmann::json;

struct SendItem {
    std::vector<uint8_t> data;
    bool reliable;
};

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

    VersusGame game;
    bool isHost;
    bool running;
    bool gameStarted;
    bool networkError;

    std::thread networkThread;
    std::atomic<bool> threadRunning{false};
    std::atomic<bool> quitThread{false};
    std::atomic<bool> isConnected{false};

    ThreadSafeQueue<std::vector<uint8_t>> incomingQueue;
    ThreadSafeQueue<SendItem> outgoingQueue;

    SoundManager soundManager;

    void SendMessage(const std::vector<uint8_t>& data, bool reliable = true);
    void SendGameState();
    void ProcessIncomingMessages();
    void HandleIncomingPacket(const std::vector<uint8_t>& data);

    void OnLifeLost();
    void OnBallLaunched(bool isUpper);
    void OnEffectApplied(EffectType type, bool upper);
    void OnEffectRemoved(EffectType type, bool upper);

    unsigned int pendingSeed;
    bool drawResult;
    std::string resultText;

    Rectangle startBtn;      // 必须存在
    Rectangle restartBtn;    // 必须存在
    Rectangle backBtn;       // 必须存在

    Texture2D background;

    bool simulatePacketLoss = false;
    float packetLossRate = 0.3f;
    std::deque<GameStateSnapshot> snapshotBuffer;
    float interpolationDelay = 0.1f;
    float latestHostTime = 0.0f;

    void AddSnapshotToBuffer(const GameStateSnapshot& snap);
    void ApplyInterpolation(float renderTime);
};

#endif