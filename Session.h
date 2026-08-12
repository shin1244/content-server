#pragma once
#include <winsock2.h>

class Session
{
	SOCKET socket_ = INVALID_SOCKET;

	RingBuffer recvBuffer_;
	RingBuffer sendBuffer_;

	OVERLAPPED recvOverlapped_;
	OVERLAPPED sendOverlapped_;
	WSABUF recvWsaBuf_;
	WSABUF sendWsaBuf_;
};

