#pragma once
#include <iostream>
#include <winsock2.h>
#include <thread>
#include <vector>
#include <mswsock.h> 

#include "ObjectPool.h"
#include "Session.h"
#include "SessionManager.h"
#include "IPacketHandler.h"
#pragma comment(lib, "ws2_32.lib")

class NetworkCore {
public:
    ~NetworkCore() { Stop(); }
    bool Start(uint16_t port, IPacketHandler* h);
    void Stop();

private:
    bool InitSocket(uint16_t port); 
    bool InitIOCP();
    void SpawnWorkers();
    void PrepareAccepts();
    void WorkerLoop();     
    bool LoadExtensionFns();

    std::vector<AcceptContext> acceptContexts_;

    bool PostAccept(AcceptContext* ctx);
    void OnAcceptComplete(AcceptContext* ctx);
    void CloseSession(Session* session);

    SOCKET listenSocket_ = INVALID_SOCKET;
    HANDLE iocp_ = nullptr;
    ObjectPool<Session, 1000> sessions_; // 최대 1000명 동시 접속
    SessionManager sessionMgr_;
    LPFN_ACCEPTEX fnAcceptEx_ = nullptr;
    std::vector<std::thread> workers_;

    IPacketHandler* ph_ = nullptr;
};