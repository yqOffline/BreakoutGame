#ifndef NETWORK_THREAD_H
#define NETWORK_THREAD_H

#include <thread>
#include <atomic>
#include <string>
#include <utility>
#include <enet/enet.h>
#include "ThreadSafeQueue.h"
#include "NetworkProtocol.h"

class NetworkThread {
public:
    NetworkThread(bool asHost, const std::string& ip, uint16_t port);
    ~NetworkThread();

    void Start();
    void SendMessage(const NetMessage& msg, bool reliable = true);
    bool TryRecvMessage(NetMessage& msg);
    void Shutdown();
    bool IsRunning() const { return running_.load(); }
    bool HasError() const { return networkError_.load(); }

private:
    void ThreadLoop();
    void ProcessEvent(const ENetEvent& event);
    void HandlePacket(ENetPacket* packet);

    bool asHost_;
    std::string remoteIP_;
    uint16_t remotePort_;

    ENetHost* host_ = nullptr;
    ENetPeer* peer_ = nullptr;

    std::thread worker_;
    std::atomic<bool> running_{false};
    std::atomic<bool> networkError_{false};
    std::atomic<bool> quit_{false};

    ThreadSafeQueue<NetMessage> incomingQueue_;
    ThreadSafeQueue<std::pair<NetMessage, bool>> outgoingQueue_;
};

#endif // NETWORK_THREAD_H