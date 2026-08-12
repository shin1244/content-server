#include "NetworkCore.h"

bool NetworkCore::Start(uint16_t port)
{
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cout << "WSAStartup failed\n";
        return false;
    }
    if (!InitSocket(port)) return false;
    if (!InitIOCP())       return false;
    SpawnWorkers();
    workers_.emplace_back([this] { AccepterLoop(); });
    std::cout << "server started on port " << port << "\n";
    return true;
}

void NetworkCore::Stop()
{
	if (listenSocket_ != INVALID_SOCKET) {
		closesocket(listenSocket_);
		listenSocket_ = INVALID_SOCKET;
	}
	if (iocp_ != nullptr) {
		CloseHandle(iocp_);
		iocp_ = nullptr;
	}
	WSACleanup();
}

bool NetworkCore::InitSocket(uint16_t port)
{
    listenSocket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket_ == INVALID_SOCKET) {
        std::cout << "socket failed: " << WSAGetLastError() << "\n";
        return false;
    }
    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    if (bind(listenSocket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        std::cout << "bind failed: " << WSAGetLastError() << "\n";
        return false;
    }
    if (listen(listenSocket_, SOMAXCONN) == SOCKET_ERROR) {
        std::cout << "listen failed: " << WSAGetLastError() << "\n";
        return false;
    }
    return true;
}

bool NetworkCore::InitIOCP()
{
    iocp_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
    if (iocp_ == nullptr) {
        std::cout << "CreateIoCompletionPort failed: " << GetLastError() << "\n";
        return false;
    }
    return true;
}

void NetworkCore::SpawnWorkers()
{
    unsigned int n = std::thread::hardware_concurrency();
    for (unsigned int i = 0; i < n; ++i)
        workers_.emplace_back([this] { WorkerLoop(); });
    std::cout << "spawned " << n << " worker threads\n";
}

void NetworkCore::AccepterLoop()
{
    while (true) {
        sockaddr_in clientAddr = {};
        int addrLen = sizeof(clientAddr);
        SOCKET clientSocket = accept(listenSocket_,
            reinterpret_cast<sockaddr*>(&clientAddr), &addrLen);
        if (clientSocket == INVALID_SOCKET) {
            std::cout << "accept ended: " << WSAGetLastError() << "\n";
            break;
        }
        std::cout << "client connected\n";

        int index = sessions_.Alloc();
        if (index == -1)
        {
            closesocket(clientSocket);
            continue;
        }
        Session* session = &sessions_[index];

        session->Init(clientSocket);
        CreateIoCompletionPort(
            reinterpret_cast<HANDLE>(clientSocket),
            iocp_,
            reinterpret_cast<ULONG_PTR>(session),
            0
        );
        session->postRecv();
    }
}

void NetworkCore::WorkerLoop()
{
    while (true) {
        DWORD bytes = 0;
        ULONG_PTR key = 0;
        OVERLAPPED* overlapped = nullptr;

        BOOL ok = GetQueuedCompletionStatus(iocp_, &bytes, &key, &overlapped, INFINITE);

        if (key == 0 && overlapped == nullptr) break;
        return;
    }
}
