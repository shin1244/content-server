#pragma once
#include <atomic>
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
    void OnSend(int bytes);
    bool CompleteIO();
    int GetIndex() { return index_; }


private:
    std::atomic<int> pendingIO_{ 0 };
    std::atomic<bool> closing_{ false };

    SOCKET socket_ = INVALID_SOCKET;
    int index_ = -1;

    RingBuffer recvBuffer_;
    RingBuffer sendBuffer_;

    WSABUF recvWsaBuf_{};
    WSABUF sendWsaBuf_{};

    RecvContext recvContext_;
    SendContext sendContext_;
};