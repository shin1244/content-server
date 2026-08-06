#include <iostream>
#include <winsock2.h>
#include <thread>
#pragma comment(lib, "ws2_32.lib")

HANDLE g_iocp;

void Accepter(SOCKET s) {
	while (true)
	{
		sockaddr_in clientAddr = {};
		int addrLen = sizeof(clientAddr);
		SOCKET clientSocket = accept(s, reinterpret_cast<sockaddr*>(&clientAddr), &addrLen);
		if (clientSocket == INVALID_SOCKET) {
			std::cout << "accept failed: " << WSAGetLastError() << "\n";
			continue;
		}
	}
}

int main()
{
	WSADATA wsa;
	WSAStartup(MAKEWORD(2, 2), &wsa);
	std::cout << "Winsock ready\n";

	SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listenSocket == INVALID_SOCKET)
	{
		std::cout << "socket failed: " << WSAGetLastError() << "\n";
		return -1;
	}

	sockaddr_in addr = {};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(5050);

	if (bind(listenSocket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
		std::cout << "bind failed: " << WSAGetLastError() << "\n";
		return -1;
	}

	if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR)
	{
		std::cout << "listen failed: " << WSAGetLastError() << "\n";
		return -1;
	}

	std::cout << "listening on port 5050...\n";

	g_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
	if (g_iocp == nullptr) {
		std::cout << "CreateIoCompletionPort failed: " << GetLastError() << "\n";
		return -1;
	}
	std::cout << "IOCP created\n";

	unsigned int n = std::thread::hardware_concurrency();
	std::cout << "spawning " << n << " worker threads\n";
	std::thread(Accepter, listenSocket).detach();

	while (true) {}
}