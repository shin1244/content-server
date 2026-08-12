#pragma once
#include <winsock2.h>
#include "RingBuffer.h"
#include "Protocol.h"


class Session
{
public:
    void Init(SOCKET socket);
    void Close();
    void PostRecv();
    OVERLAPPED* GetrecvOverlapped() { return &recvOverlapped_; }
    OVERLAPPED* GetsendOverlapped() { return &sendOverlapped_; }
    void OnRecvBytes(int bytes);

private:
    SOCKET socket_ = INVALID_SOCKET;

    RingBuffer recvBuffer_;
    RingBuffer sendBuffer_;

    WSABUF recvWsaBuf_{};
    WSABUF sendWsaBuf_{};

    OVERLAPPED recvOverlapped_;
    OVERLAPPED sendOverlapped_;
};