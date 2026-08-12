#pragma once
#include <iostream>
#include <winsock2.h>
#include <thread>
#include <vector>
#include "ObjectPool.h"
#include "Session.h"
#pragma comment(lib, "ws2_32.lib")

class NetworkCore {
public:
    ~NetworkCore() { Stop(); }
    bool Start(uint16_t port);
    void Stop();

private:
    bool InitSocket(uint16_t port); 
    bool InitIOCP();
    void SpawnWorkers();
    void AccepterLoop();             
    void WorkerLoop();               

    SOCKET listenSocket_ = INVALID_SOCKET;
    HANDLE iocp_ = nullptr;
    ObjectPool<Session, 1000> sessions_; // 최대 1000명 동시 접속
    std::vector<std::thread> workers_;
};