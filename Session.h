#pragma once
#include "RingBuffer.h"
#include "Protocol.h"
#include "NetTypes.h"


class Session
{
public:
    void Init(SOCKET socket);
    void Close();
    void PostRecv();
    void OnRecv(int bytes);
    void OnSend(int bytes);
    OVERLAPPED* GetOverlapped() { return &overlapped_; }


private:
    SOCKET socket_ = INVALID_SOCKET;

    RingBuffer recvBuffer_;
    RingBuffer sendBuffer_;

    WSABUF recvWsaBuf_{};
    WSABUF sendWsaBuf_{};

    RecvContext recvContext_;
    SendContext sendContext_;
};