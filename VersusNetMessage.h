#ifndef VERSUS_NET_MESSAGE_H
#define VERSUS_NET_MESSAGE_H

#include <cstdint>
#include <vector>
#include <cstddef>

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
    PARTICLE_SPAWN,
    CONNECT_ACK,
    READY,
    START_GAME,
    REQUEST_SNAPSHOT
};

const int MAX_EFFECTS_PER_PLAYER = 4;
const int MAX_SKILLBALLS = 8;

#pragma pack(push, 1)

struct VersusNetMessage {
    VersusMsgType type;
    int32_t data1;
    int32_t data2;
    float floatData;
};

struct SkillBallSpawnMsg {
    VersusMsgType type;
    float posX, posY;
    float speedY;
    int32_t skillType;
};

struct ParticleSpawnMsg {
    VersusMsgType type;
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
    float hostGameTime;

    float upperEffectRemaining[MAX_EFFECTS_PER_PLAYER];
    uint8_t upperEffectTypes[MAX_EFFECTS_PER_PLAYER];
    float lowerEffectRemaining[MAX_EFFECTS_PER_PLAYER];
    uint8_t lowerEffectTypes[MAX_EFFECTS_PER_PLAYER];

    int32_t skillBallCount;
    struct {
        float x, y;
        float speedX, speedY;
        uint8_t skillType;
        uint8_t active;
    } skillBalls[MAX_SKILLBALLS];
};

#pragma pack(pop)

// 序列化函数声明
std::vector<uint8_t> Serialize(const VersusNetMessage& msg);
bool Deserialize(const uint8_t* data, size_t len, VersusNetMessage& msg);

std::vector<uint8_t> Serialize(const GameStateSnapshot& snap);
bool Deserialize(const uint8_t* data, size_t len, GameStateSnapshot& snap);

std::vector<uint8_t> Serialize(const SkillBallSpawnMsg& msg);
bool Deserialize(const uint8_t* data, size_t len, SkillBallSpawnMsg& msg);

std::vector<uint8_t> Serialize(const ParticleSpawnMsg& msg);
bool Deserialize(const uint8_t* data, size_t len, ParticleSpawnMsg& msg);

#endif // VERSUS_NET_MESSAGE_H