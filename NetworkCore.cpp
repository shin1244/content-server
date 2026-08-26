#include "NetworkCore.h"

bool NetworkCore::Start(uint16_t port)
{
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cout << "WSAStartup failed\n";
        return false;
    }
    if (!InitSocket(port)) return false;
    if (!LoadExtensionFns()) return false;
    if (!InitIOCP())       return false;
    CreateIoCompletionPort((HANDLE)listenSocket_, iocp_, 0, 0);
    PrepareAccepts();
    SpawnWorkers();

    return true;
}

void NetworkCore::Stop()
{
    size_t workerCount = workers_.size();
    for (size_t i = 0; i < workerCount; ++i) {
        PostQueuedCompletionStatus(iocp_, 0, 0, nullptr);
    }

    for (auto& t : workers_) {
        if (t.joinable()) t.join();
    }

    closesocket(listenSocket_);
    CloseHandle(iocp_);
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

void NetworkCore::PrepareAccepts()
{
    acceptContexts_.resize(8);
    for (int i = 0; i < 8; ++i) {
        PostAccept(&acceptContexts_[i]);
    }
}

void NetworkCore::WorkerLoop()
{
    while (true) {
        DWORD bytes = 0;
        ULONG_PTR key = 0;
        OVERLAPPED* overlapped = nullptr;

        BOOL ok = GetQueuedCompletionStatus(iocp_, &bytes, &key, &overlapped, INFINITE);
        
    	if (overlapped == nullptr)
            break;

        OverlappedEx* overlappedEx = reinterpret_cast<OverlappedEx*>(overlapped);

        if (overlappedEx->ioType == IoType::Accept)
        {
            AcceptContext* acceptCtx = static_cast<AcceptContext*>(overlappedEx);
            if (!ok) {
                closesocket(acceptCtx->clientSocket);
                PostAccept(acceptCtx);
                continue;
            }
            OnAcceptComplete(acceptCtx);
            continue;
        }

        Session* session = reinterpret_cast<Session*>(key);
        if (session == nullptr)
            continue;
        if (!ok)
        {
            session->Close();

            if (session->CompleteIO())
                CloseSession(session);

            continue;
        }

        if (overlappedEx->ioType == IoType::Recv)
        {
            if (bytes == 0)
            {
                session->Close();

                if (session->CompleteIO())
                    CloseSession(session);

                continue;
            }
            session->OnRecv(bytes);
        }
        else if (overlappedEx->ioType == IoType::Send)
        {
            session->OnSend(bytes);
        }

        if (session->CompleteIO())
        {
            CloseSession(session);
        }
    }
}

// 확장 함수인 AcceptEx의 실제 함수 주소를 가져와서 fnAcceptEx_에 저장하는 함수
bool NetworkCore::LoadExtensionFns()
{
    GUID guid = WSAID_ACCEPTEX;
    DWORD bytes = 0;

    int ret = WSAIoctl(
        listenSocket_,
        SIO_GET_EXTENSION_FUNCTION_POINTER,
        &guid, sizeof(guid),
        &fnAcceptEx_, sizeof(fnAcceptEx_),
        &bytes, nullptr, nullptr
        );
    if (ret == SOCKET_ERROR)
    {
        std::cout << "WSAIoctl AcceptEx failed: " << WSAGetLastError() << "\n";
        return false;
    }
    return true;
}

bool NetworkCore::PostAccept(AcceptContext* ctx)
{
    ZeroMemory(&ctx->overlapped, sizeof(OVERLAPPED));
    ctx->clientSocket = WSASocket(
        AF_INET, SOCK_STREAM, IPPROTO_TCP,
        nullptr, 0, WSA_FLAG_OVERLAPPED);
    if (ctx->clientSocket == INVALID_SOCKET) {
        std::cout << "WSASocket failed: " << WSAGetLastError() << "\n";
        return false;
    }

    DWORD bytes = 0;
    BOOL ret = fnAcceptEx_(
        listenSocket_,
        ctx->clientSocket,
        ctx->buf,
        0,
        sizeof(sockaddr_in) + 16,
        sizeof(sockaddr_in) + 16,
        &bytes,
        &ctx->overlapped
    );

    if (!ret && WSAGetLastError() != ERROR_IO_PENDING) {
        closesocket(ctx->clientSocket);
        ctx->clientSocket = INVALID_SOCKET;
        return false;
    }
    return true;
}

void NetworkCore::OnAcceptComplete(AcceptContext* ctx)
{
    SOCKET clientSocket = ctx->clientSocket;

    setsockopt(clientSocket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
        (char*)&listenSocket_, sizeof(listenSocket_));

    Session* session = sessions_->Create(clientSocket);
    if (!session) { closesocket(clientSocket); PostAccept(ctx); return; }

    CreateIoCompletionPort((HANDLE)clientSocket, iocp_,
        (ULONG_PTR)session, 0);
    session->PostRecv();

    PostAccept(ctx);
}

void NetworkCore::CloseSession(Session* session)
{
    sessions_->Destroy(session);
}