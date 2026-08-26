#pragma once
#include <string>
#include <cstdint>
#include <pqxx/pqxx>

struct FriendList {
    std::vector<std::string> blocked;
    std::vector<std::string> pending;
    std::vector<std::string> friends;
};

class Database
{
public:

    explicit Database(const std::string& connString);
    bool Ping();               
    bool AddFriend(uint64_t userId, const std::string& friendName);
    bool AcceptFriend(uint64_t userId, const std::string& friendName);
    bool RejectFriend(uint64_t userId, const std::string& friendName);
    bool BlockFriend(uint64_t userId, const std::string& friendName);
    FriendList GetFriendList(uint64_t userId);
    
    uint64_t LoginOrRegister(const std::string& name);

private:
    pqxx::connection conn_;
};
