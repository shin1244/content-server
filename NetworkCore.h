#pragma once
#include <iostream>
#include <winsock2.h>
#include <thread>
#include <vector>
#include <mswsock.h> 

#include "Session.h"
#include "SessionManager.h"
#include "MPMCQueue.h"
#pragma comment(lib, "ws2_32.lib")

class NetworkCore {
public:
    NetworkCore(SessionManager* sessionManager) : sessions_(sessionManager) {}
    ~NetworkCore() { Stop(); }
    bool Start(uint16_t port);
    void Stop();
    void Broadcast(char* data, int len) { sessions_->Broadcast(data, len); }
    bool SendTo(uint64_t id, const char* data, int len) { return sessions_->SendTo(id, data, len); }
    bool IsNamed(uint64_t id) { return sessions_->IsNamed(id); }


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
    SessionManager* sessions_;
    LPFN_ACCEPTEX fnAcceptEx_ = nullptr;
    std::vector<std::thread> workers_;
};