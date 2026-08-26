#include "Consumer.h"
#include <iostream>

void Consumer::Start(MPMCQueue<Packet>* queue, SessionManager* sessions, Database* db)
{
    queue_ = queue;
    session_manager_ = sessions;
    db_ = db;

    thread_ = std::thread([this] { Loop(); });
}

void Consumer::Stop()
{
    if (queue_) queue_->Stop();
    if (thread_.joinable()) thread_.join();
}

void Consumer::Loop()
{
    Packet pkt;
    while (queue_->Pop(pkt))
    {
        Handle(pkt);
    }
}

void Consumer::Handle(Packet& pkt)
{
    const uint64_t sessionId = pkt.header.id;
    const int len = pkt.header.size;
    std::string text(pkt.message, len - HEADER_SIZE);

    if (text.empty() || text[0] != '/')
    {
        if (!session_manager_->IsNamed(sessionId)) {
            SendError(sessionId, "name plz.");
            return;
        }
        session_manager_->Broadcast(reinterpret_cast<char*>(&pkt), len);
        return;
    }

    std::istringstream iss(text);
    std::string cmd;
    iss >> cmd;

    if (cmd == "/n")
    {
        HandleNick(sessionId, iss);
        return;
    }

    // 닉네임 설정 전에는 다른 명령어 실행 불가
    if (!session_manager_->IsNamed(sessionId))
    {
        SendError(sessionId, "name plz.");
        return;
    }

    if (cmd == "/w")        HandleWhisper(sessionId, iss);
    else if (cmd == "/f")   HandleFriend(sessionId, iss);
}

void Consumer::HandleNick(uint64_t sessionId, std::istringstream& iss)
{
    std::string name;
    iss >> name;

    if (name.size() < 3 || name.size() >= 10) {
        SendError(sessionId, "invalid name");
        return;
    }

    if (session_manager_->IsNamed(sessionId)) {
        SendError(sessionId, "already name");
        return;
    }

    uint64_t userId = 0;
    try {
        userId = db_->LoginOrRegister(name);
    }
    catch (const std::exception& e) {
        std::cerr << "[DB] LoginOrRegister failed: " << e.what() << "\n";
        SendError(sessionId, "server error");
        return;
    }

    if (!session_manager_->SetName(sessionId, name)) {
        SendError(sessionId, "already online");
        return;
    }

    session_manager_->SetUserId(sessionId, userId);
    session_manager_->SendRosterTo(sessionId);

    std::string announce = "NICK " + std::to_string(sessionId) + " " + name;
    Packet a = MakePacket(0, announce);
    session_manager_->Broadcast(reinterpret_cast<char*>(&a), a.header.size);
}

void Consumer::HandleWhisper(uint64_t senderId, std::istringstream& iss)
{
    uint64_t targetId = 0;
    if (!(iss >> targetId)) return;

    std::string msg;
    iss >> std::ws; // targetId 읽은 후 남은 앞쪽 공백 제거
    std::getline(iss, msg);

    if (msg.empty()) return;

    Packet out = MakePacket(senderId, msg);
    if (session_manager_->SendTo(targetId, reinterpret_cast<const char*>(&out), out.header.size))
    {
        SendPacket(senderId, out);
    }
}

void Consumer::HandleFriend(uint64_t senderId, std::istringstream& iss)
{
    std::string command;
    iss >> command;

    if (command == "add")    HandleFriendAdd(senderId, iss);
    else if (command == "accept") HandleFriendAccept(senderId, iss);
    else if (command == "reject") HandleFriendReject(senderId, iss);
    else if (command == "block")  HandleFriendBlock(senderId, iss);
    else if (command == "list")   HandleFriendList(senderId, iss);
    else SendError(senderId, "usage: /f add|accept|reject|block|list <name>");
}

void Consumer::HandleFriendAdd(uint64_t senderId, std::istringstream& iss)
{
    std::string friendName;
    iss >> friendName;
    if (friendName.empty()) { SendError(senderId, "usage: /f add <name>"); return; }

    uint64_t myUserId = session_manager_->GetUserId(senderId);

    bool ok;
    try {
        ok = db_->AddFriend(myUserId, friendName);
    }
    catch (const std::exception& e) {
        std::cerr << "[DB] AddFriend failed: " << e.what() << "\n";
        SendError(senderId, "server error");
        return;
    }

    SendError(senderId, ok ? ("friend request sent: " + friendName) : "add failed");
}

void Consumer::HandleFriendAccept(uint64_t senderId, std::istringstream& iss)
{
    std::string friendName;
    iss >> friendName;
    if (friendName.empty()) { SendError(senderId, "usage: /f accept <name>"); return; }

    uint64_t myUserId = session_manager_->GetUserId(senderId);
    bool ok;
    try {
        ok = db_->AcceptFriend(myUserId, friendName);
    }
    catch (const std::exception& e) {
        std::cerr << "[DB] AddFriend failed: " << e.what() << "\n";
        SendError(senderId, "server error");
        return;
    }

    SendError(senderId, ok ? ("friend request sent: " + friendName) : "Accept failed");
}

void Consumer::HandleFriendReject(uint64_t senderId, std::istringstream& iss)
{
    std::string friendName;
    iss >> friendName;
    if (friendName.empty()) { SendError(senderId, "usage: /f reject <name>"); return; }

    uint64_t myUserId = session_manager_->GetUserId(senderId);
    bool ok;
    try {
        ok = db_->RejectFriend(myUserId, friendName);
    }
    catch (const std::exception& e) {
        std::cerr << "[DB] AddFriend failed: " << e.what() << "\n";
        SendError(senderId, "server error");
        return;
    }

    SendError(senderId, ok ? ("friend request sent: " + friendName) : "Reject failed");
}

void Consumer::HandleFriendBlock(uint64_t senderId, std::istringstream& iss)
{
    std::string friendName;
    iss >> friendName;
    if (friendName.empty()) { SendError(senderId, "usage: /f block <name>"); return; }

    uint64_t myUserId = session_manager_->GetUserId(senderId);
    bool ok;
    try {
        ok = db_->BlockFriend(myUserId, friendName);
    }
    catch (const std::exception& e) {
        std::cerr << "[DB] AddFriend failed: " << e.what() << "\n";
        SendError(senderId, "server error");
        return;
    }

    SendError(senderId, ok ? ("friend request sent: " + friendName) : "Block failed");
}

void Consumer::HandleFriendList(uint64_t senderId, std::istringstream& iss)
{
    std::string text;
    iss >> text;
    if (!text.empty()) { SendError(senderId, "usage: /f list"); return; }

    uint64_t myUserId = session_manager_->GetUserId(senderId);

    FriendList list;
    try {
        list = db_->GetFriendList(myUserId);
    }
    catch (const std::exception& e) {
        std::cerr << "[DB] friend list failed: " << e.what() << "\n";
        SendError(senderId, "server error");
        return;
    }

    std::string msg;
    msg += "Blocked\n";
    for (const auto& n : list.blocked) msg += n + "\n";
    msg += "----------------\n";
    msg += "Request\n";
    for (const auto& n : list.pending) msg += n + "\n";
    msg += "----------------\n";
    msg += "Friends\n";
    for (const auto& n : list.friends) msg += n + "\n";

    SendError(senderId, msg);
}

void Consumer::SendPacket(uint64_t sessionId, const Packet& pkt)
{
    session_manager_->SendTo(sessionId, reinterpret_cast<const char*>(&pkt), pkt.header.size);
}

void Consumer::SendError(uint64_t sessionId, const std::string& msg)
{
    Packet err = MakePacket(0, msg);
    SendPacket(sessionId, err);
}