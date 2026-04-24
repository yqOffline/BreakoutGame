// VersusNetMessage.h
#ifndef VERSUS_NET_MESSAGE_H
#define VERSUS_NET_MESSAGE_H

#include <cstdint>

enum class VersusMsgType : uint8_t {
    SEED,
    INPUT,
    GAME_STATE,
    LIFE_LOST,
    BALL_LAUNCHED,
    EFFECT_APPLIED,
    EFFECT_REMOVED,
    SKILLBALL_SPAWN,
    GAME_OVER,
    CONTROL_START   // 新增：Host 通知 Guest 游戏开始
};

#pragma pack(push, 1)

struct VersusNetMessage {
    VersusMsgType type;
    int32_t data1;
    int32_t data2;
    float floatData;
};

struct GameStateSnapshot {
    float ballX, ballY;
    float ballSpeedX, ballSpeedY;
    float upperPaddleX, lowerPaddleX;
    int32_t upperLives, lowerLives;
    uint8_t ballR, ballG, ballB, ballA;
    uint8_t ballAttached;       // 0=飞行, 1=附上挡板, 2=附下挡板
    uint32_t brickActiveBits;
};

#pragma pack(pop)

#endif