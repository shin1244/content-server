#include <winsock2.h>
#include <mutex>
#include <thread> 
#include <iostream>

#pragma comment(lib, "ws2_32.lib")

static HANDLE g_iocp;

int main()
{
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cout << "WSAStartup failed\n";
        return 1;
    }
    std::cout << "Winsock ready\n";

    g_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
    if (g_iocp == nullptr) {
        std::cout << "CreateIoCompletionPort failed: " << GetLastError() << "\n";
        WSACleanup();
        return 1;
    }
    std::cout << "IOCP created\n";

    const unsigned int n = std::thread::hardware_concurrency() - 1;
    std::cout << "spawning " << n << " worker threads\n";

    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        std::cout << "socket failed: " << WSAGetLastError() << "\n";
        CloseHandle(g_iocp);
        WSACleanup();
        return 1;
    }

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(5050);

    if (bind(listenSocket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        std::cout << "bind failed: " << WSAGetLastError() << "\n";
        closesocket(listenSocket);
        CloseHandle(g_iocp);
        WSACleanup();
        return 1;
    }

    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cout << "listen failed: " << WSAGetLastError() << "\n";
        closesocket(listenSocket);
        CloseHandle(g_iocp);
        WSACleanup();
        return 1;
    }
    std::cout << "listening on port 5050...\n";

    closesocket(listenSocket);
    CloseHandle(g_iocp);
    WSACleanup();

    return 0;
}