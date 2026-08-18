#include "Session.h"

void Session::Init(SOCKET socket, int index, IPacketHandler* h)
{
    socket_ = socket;
    index_ = index;
    closing_.store(false);
    pendingIO_.store(0);
    sending_.store(false);
    recvBuffer_.Clear();
    sendBuffer_.Clear();
    handler_ = h;
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
        recvBuffer_.Peek(reinterpret_cast<char*>(&header), HEADER_SIZE);
        if (header.size < HEADER_SIZE || header.size > sizeof(Packet))  // 260
        {
            Close();
            break;
        }

        if (recvBuffer_.GetUsedSize() < header.size) break;

        Packet pkt;
        recvBuffer_.Peek(reinterpret_cast<char*>(&pkt), header.size);   // Packet¿¡ Á÷Á¢
        recvBuffer_.moveHead(header.size);
        pkt.header.id = id_;

        handler_->OnPacket(pkt);
    }
    PostRecv();
}

void Session::PostSend()
{
    if (closing_) return;
    if (sendBuffer_.GetUsedSize() == 0) { sending_ = false; return; }

    sendWsaBuf_.buf = sendBuffer_.GetHead();
    sendWsaBuf_.len = sendBuffer_.GetLinearUsedSize();

    DWORD flags = 0;
    DWORD bytesSent = 0;

    ZeroMemory(&sendContext_.overlapped, sizeof(WSAOVERLAPPED));
    ++pendingIO_;

    int ret = WSASend(
        socket_,
        &sendWsaBuf_,
        1,
        &bytesSent,
        flags,
        &sendContext_.overlapped,
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

void Session::Send(const char* data, int len)
{
    std::lock_guard g(sendLock_);
    if (sendBuffer_.GetEmptySize() < len) return;
    if (!sendBuffer_.Write(data, len)) return;
	sendBuffer_.moveTail(len);
    if (!sending_) { sending_ = true; PostSend(); }
}

void Session::OnSend(int bytes)
{
    std::lock_guard g(sendLock_);
    sendBuffer_.moveHead(bytes);
    if (sendBuffer_.GetUsedSize() != 0) PostSend();
    else sending_ = false;
}

bool Session::CompleteIO()
{
    int count = --pendingIO_;

    return closing_ && count == 0;
}
