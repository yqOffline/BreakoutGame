#include "NetworkThread.h"
#include <iostream>
#include <chrono>
#include <cstring>

NetworkThread::NetworkThread(bool asHost, const std::string& ip, uint16_t port)
    : asHost_(asHost), remoteIP_(ip), remotePort_(port) {}

NetworkThread::~NetworkThread() {
    Shutdown();
}

void NetworkThread::Start() {
    if (enet_initialize() != 0) {
        std::cerr << "Failed to initialize ENet!" << std::endl;
        networkError_ = true;
        running_ = false;
        return;
    }

    if (asHost_) {
        ENetAddress address;
        address.host = ENET_HOST_ANY;
        address.port = remotePort_;
        host_ = enet_host_create(&address, 2, 2, 0, 0);
        if (!host_) {
            std::cerr << "Failed to create host!" << std::endl;
            networkError_ = true;
            running_ = false;
            return;
        }
        std::cout << "Host created on port " << remotePort_ << std::endl;
    } else {
        host_ = enet_host_create(nullptr, 1, 2, 0, 0);
        if (!host_) {
            std::cerr << "Failed to create client host!" << std::endl;
            networkError_ = true;
            running_ = false;
            return;
        }
        ENetAddress address;
        enet_address_set_host(&address, remoteIP_.c_str());
        address.port = remotePort_;
        peer_ = enet_host_connect(host_, &address, 2, 0);
        if (!peer_) {
            std::cerr << "Failed to initiate connection!" << std::endl;
            networkError_ = true;
            running_ = false;
            return;
        }
        std::cout << "Connecting to " << remoteIP_ << ":" << remotePort_ << std::endl;
    }

    running_ = true;
    worker_ = std::thread(&NetworkThread::ThreadLoop, this);
}

void NetworkThread::SendMessage(const NetMessage& msg, bool reliable) {
    outgoingQueue_.push({msg, reliable});
}

bool NetworkThread::TryRecvMessage(NetMessage& msg) {
    return incomingQueue_.try_pop(msg);
}

void NetworkThread::Shutdown() {
    if (!running_.exchange(false)) return;
    quit_ = true;
    if (worker_.joinable()) worker_.join();
    if (host_) enet_host_destroy(host_);
    enet_deinitialize();
}

void NetworkThread::ThreadLoop() {
    while (!quit_) {
        // 处理网络事件
        ENetEvent event;
        while (enet_host_service(host_, &event, 0) > 0) {
            ProcessEvent(event);
        }

        // 从发送队列取出消息并发送
        std::pair<NetMessage, bool> outItem;
        while (outgoingQueue_.try_pop(outItem)) {
            if (peer_) {
                ENetPacket* packet = enet_packet_create(&outItem.first, sizeof(outItem.first),
                    outItem.second ? ENET_PACKET_FLAG_RELIABLE : ENET_PACKET_FLAG_UNSEQUENCED);
                enet_peer_send(peer_, 0, packet);
            }
        }

        // 短暂休眠避免 CPU 空转
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void NetworkThread::ProcessEvent(const ENetEvent& event) {
    switch (event.type) {
        case ENET_EVENT_TYPE_CONNECT:
            std::cout << "Peer connected!" << std::endl;
            peer_ = event.peer;
            break;

        case ENET_EVENT_TYPE_RECEIVE:
            HandlePacket(event.packet);
            enet_packet_destroy(event.packet);
            break;

        case ENET_EVENT_TYPE_DISCONNECT: {
            std::cout << "Peer disconnected!" << std::endl;
            peer_ = nullptr;
            // 通知主线程断开（使用特殊消息）
            NetMessage discMsg{NetMsgType::RESULT_NOTIFY, 0, 0, 0.0f};
            incomingQueue_.push(discMsg);
            break;
        }

        default: break;
    }
}

void NetworkThread::HandlePacket(ENetPacket* packet) {
    if (packet->dataLength != sizeof(NetMessage)) return;
    NetMessage* msg = (NetMessage*)packet->data;
    incomingQueue_.push(*msg);
}