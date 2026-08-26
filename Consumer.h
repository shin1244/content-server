#pragma once
#include <thread>
#include "MPMCQueue.h"
#include "PacketHandler.h"
#include "Protocol.h" 
#include "Database.h"

class Consumer
{
public:
    void Start(MPMCQueue<Packet>* queue, SessionManager* sessions, Database* db);
    void Stop();

private:
    void Loop();
    void Handle(Packet& pkt);

    void HandleNick(uint64_t sessionId, std::istringstream& iss);
    void HandleWhisper(uint64_t senderId, std::istringstream& iss);
    void HandleFriend(uint64_t senderId, std::istringstream& iss);
    void HandleFriendAdd(uint64_t senderId, std::istringstream& iss);

    void SendPacket(uint64_t sessionId, const Packet& pkt);
    void SendError(uint64_t sessionId, const std::string& msg);

    Database* db_ = nullptr;
    MPMCQueue<Packet>* queue_ = nullptr;
    SessionManager* session_manager_ = nullptr;
    std::thread thread_;
};