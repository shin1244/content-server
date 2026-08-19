#pragma once
#include <unordered_map>
#include <shared_mutex>
#include "Session.h"
#include <atomic>

class SessionManager {
private:
    std::shared_mutex lock_;
    std::unordered_map<uint64_t, Session*> byId_;
    std::unordered_map<std::string, uint64_t> byName_; 
    std::atomic<uint64_t> nextId_{1};
public:
    void Add(Session* s);
    void Remove(Session* s);
    void Broadcast(char* data, int len);
    bool SendTo(uint64_t id, const char* data, int len);
    bool IsNamed(uint64_t id);
    bool SetName(uint64_t id, std::string name);
};