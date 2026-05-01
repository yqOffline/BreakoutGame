#ifndef NETWORK_PROTOCOL_H
#define NETWORK_PROTOCOL_H

#include <cstdint>

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

#endif // NETWORK_PROTOCOL_H