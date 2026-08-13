#pragma once
#include <atomic>
#include <mutex>

#include "RingBuffer.h"
#include "Protocol.h"
#include "NetTypes.h"


class Session
{
public:
    void Init(SOCKET socket, int index);
    void Close();

    void PostRecv();
    void OnRecv(int bytes);

    void PostSend();
    void OnSend(int bytes);
    void Send(const char* data, int len);

    bool CompleteIO();
    int GetIndex() { return index_; }


private:
    std::atomic<int> pendingIO_{ 0 }; // IO중 참조 카운트
    std::atomic<bool> closing_{ false }; // 종료 예약
    std::atomic<bool> sending_{ false }; // 이미 보내는 중인지 체크
    std::mutex sendLock_;

    SOCKET socket_ = INVALID_SOCKET;
    int index_ = -1;

    RingBuffer recvBuffer_;
    RingBuffer sendBuffer_;

    WSABUF recvWsaBuf_{};
    WSABUF sendWsaBuf_{};

    RecvContext recvContext_;
    SendContext sendContext_;
};