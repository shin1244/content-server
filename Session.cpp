#include "Session.h"

void Session::Init(SOCKET socket, int index)
{
    socket_ = socket;
    index_ = index;

    recvBuffer_.Clear();
    sendBuffer_.Clear();
}

void Session::Close()
{
    closing_ = true;
    closesocket(socket_);
}

void Session::PostRecv()
{
    if (closing_)
        return;

    int freeSize = recvBuffer_.GetLinearEmptySize();
    if (freeSize <= 0) return;

    recvWsaBuf_.buf = recvBuffer_.GetTail();
    recvWsaBuf_.len = freeSize;

    DWORD flags = 0;
    DWORD byteRecv = 0;

    ZeroMemory(&recvContext_.overlapped, sizeof(WSAOVERLAPPED));
    ++pendingIO_;

    int ret = WSARecv(
        socket_,
        &recvWsaBuf_,
        1,
        &byteRecv,
        &flags,
        &recvContext_.overlapped,
        nullptr
    );

    if (ret == SOCKET_ERROR)
    {
        int errCode = WSAGetLastError();
        if (errCode != WSA_IO_PENDING)
        {
            --pendingIO_;
            Close();
        }
    }
}

void Session::OnRecv(int bytes)
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
    PostRecv();
}

void Session::OnSend(int bytes)
{
    sendBuffer_.moveHead(bytes);
}

bool Session::CompleteIO()
{
    int count = --pendingIO_;

    return closing_ && count == 0;
}