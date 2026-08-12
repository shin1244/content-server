#include "Session.h"

void Session::Init(SOCKET socket)
{
    socket_ = socket;

    recvBuffer_.Clear();
    sendBuffer_.Clear();
}

void Session::Close()
{
    closesocket(socket_);
}

void Session::PostRecv()
{
    int freeSize = recvBuffer_.GetLinearEmptySize();
    if (freeSize <= 0) return;

    ZeroMemory(&recvOverlapped_, sizeof(recvOverlapped_));
    recvWsaBuf_.buf = recvBuffer_.GetTail();
    recvWsaBuf_.len = freeSize;

    DWORD flags = 0;
    DWORD byteRecv = 0;

    int ret = WSARecv(
        socket_,
        &recvWsaBuf_,
        1,
        &byteRecv,
        &flags,
        &recvOverlapped_,
        nullptr
    );
}

void Session::OnRecvBytes(int bytes)
{
    recvBuffer_.moveTail(bytes);
    while (true)
    {
        if (recvBuffer_.GetUsedSize() < HEADER_SIZE) break;
        PacketHeader header;
        recvBuffer_.Peek(&header, HEADER_SIZE);
        if (recvBuffer_.GetUsedSize() < header.size) break;
        char packet[4096];
        recvBuffer_.Peek(packet, header.size);
        recvBuffer_.moveHead(header.size);
    }
}