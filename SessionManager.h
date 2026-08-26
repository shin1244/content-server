#pragma once
#include <unordered_map>
#include <shared_mutex>
#include "Session.h"
#include <atomic>

#include "MPMCQueue.h"
#include "ObjectPool.h"

class SessionManager {
private:
    std::shared_mutex lock_;
    std::unordered_map<uint64_t, Session*> byId_;
    std::unordered_map<std::string, uint64_t> byName_; 
    std::unordered_map<uint64_t, uint64_t> byUserId_;
    std::atomic<uint64_t> nextId_{1};
    ObjectPool<Session, 1000> pool_;
public:
    Session* Create(SOCKET sock, MPMCQueue<Packet>* h);
    void Destroy(Session* s);
    void Broadcast(char* data, int len);
    bool SendTo(uint64_t id, const char* data, int len);
    void SendToFriends(uint64_t userId, const char* data, int len);
    bool IsNamed(uint64_t id);
    bool SetName(uint64_t id, std::string name);
    void SendRosterTo(uint64_t id);

    void LoadFriendCache(uint64_t userId, const std::vector<uint64_t>& ids);
    void CacheAddFriend(uint64_t userId, uint64_t friendId);
    void CacheRemoveFriend(uint64_t userId, uint64_t friendId);
    bool AreFriends(uint64_t userId, uint64_t friendId);

    void SetUserId(uint64_t sessionId, uint64_t userId);
    uint64_t GetUserId(uint64_t sessionId);
};
