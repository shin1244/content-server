#pragma once
#include <winsock2.h>
#include <mswsock.h>

// I/O 종류 구분
enum class IoType {
    Accept,
    Recv,
    Send
};

// 모든 비동기 구조체의 공통 부모
struct OverlappedEx {
    WSAOVERLAPPED overlapped = {};
    IoType        ioType;
};

struct AcceptContext : public OverlappedEx {
    SOCKET clientSocket = INVALID_SOCKET;
    char   buf[(sizeof(SOCKADDR_IN) + 16) * 2] = {};

    AcceptContext() { ioType = IoType::Accept; }
};

struct RecvContext : public OverlappedEx {
    RecvContext() { ioType = IoType::Recv; }
};

struct SendContext : public OverlappedEx {
    SendContext() { ioType = IoType::Send; }
};