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
    CONTROL_START,
    PARTICLE_SPAWN
};

#pragma pack(push, 1)

struct VersusNetMessage {
    VersusMsgType type;
    int32_t data1;
    int32_t data2;
    float floatData;
};

// 技能球生成专用消息（位置、速度Y、类型）
struct SkillBallSpawnMsg {
    VersusMsgType type;     // SKILLBALL_SPAWN
    float posX, posY;
    float speedY;
    int32_t skillType;      // SkillType enum 的值
};

// 粒子生成专用消息（位置、颜色、数量）
struct ParticleSpawnMsg {
    VersusMsgType type;     // PARTICLE_SPAWN
    float posX, posY;
    uint8_t r, g, b, a;
    int32_t count;
    uint8_t isBreak;
};

struct GameStateSnapshot {
    float ballX, ballY;
    float ballSpeedX, ballSpeedY;
    float upperPaddleX, lowerPaddleX;
    int32_t upperLives, lowerLives;
    uint8_t ballR, ballG, ballB, ballA;
    uint8_t ballAttached;
    uint32_t brickActiveBits;
    float hostGameTime;      // 新增：主机端的游戏时间（秒）
};

#pragma pack(pop)

#endif