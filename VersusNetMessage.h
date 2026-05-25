#ifndef VERSUS_NET_MESSAGE_H
#define VERSUS_NET_MESSAGE_H

#include <cstdint>
#include <vector>
#include <cstddef>

// ========== 消息类型枚举 ==========
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

// ========== 限制常量 ==========
const int MAX_EFFECTS_PER_PLAYER = 4;   // 每个玩家最多同时生效的效果数
const int MAX_SKILLBALLS = 8;           // 最多同时存在的技能球

// ========== 网络结构体（仅供内部使用，不再直接 memcpy）==========
#pragma pack(push, 1)

struct VersusNetMessage {
    VersusMsgType type;
    int32_t data1;
    int32_t data2;
    float floatData;
};

struct SkillBallSpawnMsg {
    VersusMsgType type;     // SKILLBALL_SPAWN
    float posX, posY;
    float speedY;
    int32_t skillType;      // SkillType 枚举值
};

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
    uint8_t ballAttached;               // 0=飞行,1=附着上板,2=附着下板
    uint32_t brickActiveBits;           // 砖块活跃位掩码（最多支持32个砖块）
    float hostGameTime;                 // 主机端的游戏时间（秒）

    // 效果同步
    float upperEffectRemaining[MAX_EFFECTS_PER_PLAYER];
    uint8_t upperEffectTypes[MAX_EFFECTS_PER_PLAYER]; // EffectType 枚举值
    float lowerEffectRemaining[MAX_EFFECTS_PER_PLAYER];
    uint8_t lowerEffectTypes[MAX_EFFECTS_PER_PLAYER];

    // 技能球同步
    int32_t skillBallCount;             // 实际技能球数量 (0 ~ MAX_SKILLBALLS)
    struct {
        float x, y;
        float speedX, speedY;
        uint8_t skillType;              // SkillType 枚举值
        uint8_t active;                 // 1=活跃,0=已失效
    } skillBalls[MAX_SKILLBALLS];
};

#pragma pack(pop)

// ========== 序列化函数声明 ==========

// VersusNetMessage
std::vector<uint8_t> Serialize(const VersusNetMessage& msg);
bool Deserialize(const uint8_t* data, size_t len, VersusNetMessage& msg);

// GameStateSnapshot
std::vector<uint8_t> Serialize(const GameStateSnapshot& snap);
bool Deserialize(const uint8_t* data, size_t len, GameStateSnapshot& snap);

// SkillBallSpawnMsg
std::vector<uint8_t> Serialize(const SkillBallSpawnMsg& msg);
bool Deserialize(const uint8_t* data, size_t len, SkillBallSpawnMsg& msg);

// ParticleSpawnMsg
std::vector<uint8_t> Serialize(const ParticleSpawnMsg& msg);
bool Deserialize(const uint8_t* data, size_t len, ParticleSpawnMsg& msg);

#endif // VERSUS_NET_MESSAGE_H