#include "VersusNetMessage.h"
#include <cstring>
#include <algorithm>
#include <endian.h>   // 提供 htobe16, htobe32 等（Linux）
// 或者使用 <arpa/inet.h> 提供 htons, htonl

// ========== 字节序转换辅助函数（使用标准函数）==========
static inline uint16_t ToBigEndian16(uint16_t x) {
    return htobe16(x);  // 或 htons(x)
}
static inline uint32_t ToBigEndian32(uint32_t x) {
    return htobe32(x);  // 或 htonl(x)
}
static inline float ToBigEndianFloat(float f) {
    uint32_t i;
    std::memcpy(&i, &f, sizeof(i));
    i = ToBigEndian32(i);
    float res;
    std::memcpy(&res, &i, sizeof(res));
    return res;
}
static inline uint16_t FromBigEndian16(uint16_t x) {
    return be16toh(x);  // 或 ntohs(x)
}
static inline uint32_t FromBigEndian32(uint32_t x) {
    return be32toh(x);  // 或 ntohl(x)
}
static inline float FromBigEndianFloat(uint32_t x) {
    x = FromBigEndian32(x);
    float f;
    std::memcpy(&f, &x, sizeof(f));
    return f;
}

// ========== 通用序列化读写辅助 ==========
template<typename T>
static void Write(std::vector<uint8_t>& buf, const T& val) {
    T net = val;
    if constexpr (std::is_integral_v<T> && sizeof(T) == 2) {
        uint16_t tmp = ToBigEndian16(static_cast<uint16_t>(val));
        std::memcpy(&net, &tmp, sizeof(T));
    } else if constexpr (std::is_integral_v<T> && sizeof(T) == 4) {
        uint32_t tmp = ToBigEndian32(static_cast<uint32_t>(val));
        std::memcpy(&net, &tmp, sizeof(T));
    } else if constexpr (std::is_floating_point_v<T> && sizeof(T) == 4) {
        float tmp = ToBigEndianFloat(val);
        std::memcpy(&net, &tmp, sizeof(T));
    }
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&net);
    buf.insert(buf.end(), p, p + sizeof(T));
}

template<typename T>
static void Read(const uint8_t*& data, size_t& len, T& val) {
    if (len < sizeof(T)) return;
    std::memcpy(&val, data, sizeof(T));
    if constexpr (std::is_integral_v<T> && sizeof(T) == 2) {
        val = static_cast<T>(FromBigEndian16(static_cast<uint16_t>(val)));
    } else if constexpr (std::is_integral_v<T> && sizeof(T) == 4) {
        val = static_cast<T>(FromBigEndian32(static_cast<uint32_t>(val)));
    } else if constexpr (std::is_floating_point_v<T> && sizeof(T) == 4) {
        uint32_t tmp;
        std::memcpy(&tmp, &val, sizeof(tmp));
        float f = FromBigEndianFloat(tmp);
        std::memcpy(&val, &f, sizeof(f));
    }
    data += sizeof(T);
    len -= sizeof(T);
}

// ========== VersusNetMessage ==========
std::vector<uint8_t> Serialize(const VersusNetMessage& msg) {
    std::vector<uint8_t> buf;
    Write(buf, static_cast<uint8_t>(msg.type));
    Write(buf, msg.data1);
    Write(buf, msg.data2);
    Write(buf, msg.floatData);
    return buf;
}

bool Deserialize(const uint8_t* data, size_t len, VersusNetMessage& msg) {
    if (len < sizeof(uint8_t) + 2*sizeof(int32_t) + sizeof(float)) return false;
    const uint8_t* ptr = data;
    size_t remaining = len;
    uint8_t type;
    Read(ptr, remaining, type);
    msg.type = static_cast<VersusMsgType>(type);
    Read(ptr, remaining, msg.data1);
    Read(ptr, remaining, msg.data2);
    Read(ptr, remaining, msg.floatData);
    return true;
}

// ========== GameStateSnapshot ==========
std::vector<uint8_t> Serialize(const GameStateSnapshot& snap) {
    std::vector<uint8_t> buf;
    Write(buf, snap.ballX);
    Write(buf, snap.ballY);
    Write(buf, snap.ballSpeedX);
    Write(buf, snap.ballSpeedY);
    Write(buf, snap.upperPaddleX);
    Write(buf, snap.lowerPaddleX);
    Write(buf, snap.upperLives);
    Write(buf, snap.lowerLives);
    Write(buf, snap.ballR);
    Write(buf, snap.ballG);
    Write(buf, snap.ballB);
    Write(buf, snap.ballA);
    Write(buf, snap.ballAttached);
    Write(buf, snap.brickActiveBits);
    Write(buf, snap.hostGameTime);
    for (int i = 0; i < MAX_EFFECTS_PER_PLAYER; ++i) {
        Write(buf, snap.upperEffectRemaining[i]);
        Write(buf, snap.upperEffectTypes[i]);
    }
    for (int i = 0; i < MAX_EFFECTS_PER_PLAYER; ++i) {
        Write(buf, snap.lowerEffectRemaining[i]);
        Write(buf, snap.lowerEffectTypes[i]);
    }
    Write(buf, snap.skillBallCount);
    for (int i = 0; i < snap.skillBallCount && i < MAX_SKILLBALLS; ++i) {
        Write(buf, snap.skillBalls[i].x);
        Write(buf, snap.skillBalls[i].y);
        Write(buf, snap.skillBalls[i].speedX);
        Write(buf, snap.skillBalls[i].speedY);
        Write(buf, snap.skillBalls[i].skillType);
        Write(buf, snap.skillBalls[i].active);
    }
    return buf;
}

bool Deserialize(const uint8_t* data, size_t len, GameStateSnapshot& snap) {
    const uint8_t* ptr = data;
    size_t remaining = len;
    #define READ(var) Read(ptr, remaining, var)
    READ(snap.ballX);
    READ(snap.ballY);
    READ(snap.ballSpeedX);
    READ(snap.ballSpeedY);
    READ(snap.upperPaddleX);
    READ(snap.lowerPaddleX);
    READ(snap.upperLives);
    READ(snap.lowerLives);
    READ(snap.ballR);
    READ(snap.ballG);
    READ(snap.ballB);
    READ(snap.ballA);
    READ(snap.ballAttached);
    READ(snap.brickActiveBits);
    READ(snap.hostGameTime);
    for (int i = 0; i < MAX_EFFECTS_PER_PLAYER; ++i) {
        READ(snap.upperEffectRemaining[i]);
        READ(snap.upperEffectTypes[i]);
    }
    for (int i = 0; i < MAX_EFFECTS_PER_PLAYER; ++i) {
        READ(snap.lowerEffectRemaining[i]);
        READ(snap.lowerEffectTypes[i]);
    }
    READ(snap.skillBallCount);
    if (snap.skillBallCount > MAX_SKILLBALLS) snap.skillBallCount = MAX_SKILLBALLS;
    for (int i = 0; i < snap.skillBallCount; ++i) {
        READ(snap.skillBalls[i].x);
        READ(snap.skillBalls[i].y);
        READ(snap.skillBalls[i].speedX);
        READ(snap.skillBalls[i].speedY);
        READ(snap.skillBalls[i].skillType);
        READ(snap.skillBalls[i].active);
    }
    #undef READ
    return true;
}

// ========== SkillBallSpawnMsg ==========
std::vector<uint8_t> Serialize(const SkillBallSpawnMsg& msg) {
    std::vector<uint8_t> buf;
    Write(buf, static_cast<uint8_t>(msg.type));
    Write(buf, msg.posX);
    Write(buf, msg.posY);
    Write(buf, msg.speedY);
    Write(buf, msg.skillType);
    return buf;
}

bool Deserialize(const uint8_t* data, size_t len, SkillBallSpawnMsg& msg) {
    if (len < sizeof(uint8_t) + 3*sizeof(float) + sizeof(int32_t)) return false;
    const uint8_t* ptr = data;
    size_t remaining = len;
    uint8_t type;
    Read(ptr, remaining, type);
    msg.type = static_cast<VersusMsgType>(type);
    Read(ptr, remaining, msg.posX);
    Read(ptr, remaining, msg.posY);
    Read(ptr, remaining, msg.speedY);
    Read(ptr, remaining, msg.skillType);
    return true;
}

// ========== ParticleSpawnMsg ==========
std::vector<uint8_t> Serialize(const ParticleSpawnMsg& msg) {
    std::vector<uint8_t> buf;
    Write(buf, static_cast<uint8_t>(msg.type));
    Write(buf, msg.posX);
    Write(buf, msg.posY);
    Write(buf, msg.r);
    Write(buf, msg.g);
    Write(buf, msg.b);
    Write(buf, msg.a);
    Write(buf, msg.count);
    Write(buf, msg.isBreak);
    return buf;
}

bool Deserialize(const uint8_t* data, size_t len, ParticleSpawnMsg& msg) {
    if (len < sizeof(uint8_t) + 2*sizeof(float) + 4*sizeof(uint8_t) + sizeof(int32_t) + sizeof(uint8_t))
        return false;
    const uint8_t* ptr = data;
    size_t remaining = len;
    uint8_t type;
    Read(ptr, remaining, type);
    msg.type = static_cast<VersusMsgType>(type);
    Read(ptr, remaining, msg.posX);
    Read(ptr, remaining, msg.posY);
    Read(ptr, remaining, msg.r);
    Read(ptr, remaining, msg.g);
    Read(ptr, remaining, msg.b);
    Read(ptr, remaining, msg.a);
    Read(ptr, remaining, msg.count);
    Read(ptr, remaining, msg.isBreak);
    return true;
}