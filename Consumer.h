#pragma once
#include <thread>
#include "MPMCQueue.h"
#include "PacketHandler.h"
#include "Protocol.h" 
#include "Database.h"
#include "Ranking.h"
#include <random>

class Consumer
{
public:
    void Start(MPMCQueue<Packet>* queue, SessionManager* sessions, 
        Database* db, Ranking* ranking);
    void Stop();

private:
    void Loop();
    void Handle(Packet& pkt);

    void HandleNick(uint64_t sessionId, std::istringstream& iss);
    void HandleWhisper(uint64_t senderId, std::istringstream& iss);

    void HandleFriend(uint64_t senderId, std::istringstream& iss);
    void HandleFriendAdd(uint64_t senderId, std::istringstream& iss);
    void HandleFriendAccept(uint64_t senderId, std::istringstream& iss);
    void HandleFriendReject(uint64_t senderId, std::istringstream& iss);
    void HandleFriendBlock(uint64_t senderId, std::istringstream& iss);
    void HandleFriendList(uint64_t senderId, std::istringstream& iss);
    void HandleFriendBroadcast(uint64_t senderId, const std::string& msg);

    void HandleInventory(uint64_t senderId, std::istringstream& iss);
    void ShowInventory(uint64_t senderId);
    void HandleEnhance(uint64_t senderId, std::istringstream& iss);

    void SendPacket(uint64_t sessionId, const Packet& pkt);
    void SendError(uint64_t sessionId, const std::string& msg);

    void RewardChat(uint64_t sessionId);

    Ranking* ranking_;
    Database* db_ = nullptr;
    MPMCQueue<Packet>* queue_ = nullptr;
    SessionManager* session_manager_ = nullptr;
    std::thread thread_;
};