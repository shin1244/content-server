#pragma once
#include <winsock2.h>
#include "RingBuffer.h"


class Session
{
public:
    void Init(SOCKET socket);
    void Close();
    void postRecv();

private:
    SOCKET socket_ = INVALID_SOCKET;

    RingBuffer recvBuffer_;
    RingBuffer sendBuffer_;

    WSABUF recvWsaBuf_{};
    WSABUF sendWsaBuf_{};

    OVERLAPPED recvOverlapped_;
    OVERLAPPED sendOverlapped_;
};