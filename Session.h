#pragma once
#include <atomic>
#include <mutex>

#include "RingBuffer.h"
#include "Protocol.h"
#include "NetTypes.h"
#include "IPacketHandler.h"
#include "MPMCQueue.h"


class Session
{
public:
    void Init(SOCKET socket, int index, int id, MPMCQueue<Packet>* h);
    void Close();

    void PostRecv();
    void OnRecv(int bytes);

    void PostSend();
    void OnSend(int bytes);
    void Send(const char* data, int len);
    
    bool CompleteIO();
    int GetIndex() { return index_; }

    void SetId(uint64_t id) { id_ = id; }
    void SetName(std::string name) { name_ = std::move(name); }

    bool IsNamed() { return !name_.empty(); }
    
    std::string GetName() { return name_; }
    uint64_t GetId() { return id_; }

    void SetUserId(uint64_t userId) { userId_ = userId; }
    uint64_t GetUserId(uint64_t sessionId) { return userId_; }

private:
    std::atomic<int> pendingIO_{ 0 }; // IO중 참조 카운트
    std::atomic<bool> closing_{ false }; // 종료 예약
    std::atomic<bool> sending_{ false }; // 이미 보내는 중인지 체크
    std::mutex sendLock_;

    SOCKET socket_ = INVALID_SOCKET;
    int index_ = -1;
    uint64_t id_ = 0;
    uint64_t userId_ = 0;

    std::string name_;

    RingBuffer recvBuffer_;
    RingBuffer sendBuffer_;

    WSABUF recvWsaBuf_{};
    WSABUF sendWsaBuf_{};

    RecvContext recvContext_;
    SendContext sendContext_;

    MPMCQueue<Packet>* handler_ = nullptr;
};
