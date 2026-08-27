#pragma once
#include <string>
#include <cstdint>
#include <pqxx/pqxx>

struct Item {
    uint64_t itemId;
    int enhanceLevel;
    int power;
};

struct EnhanceResult
{
    enum class Status { Success, Failed, NotOwned, NoGold };

    Status   status;
    int      enhanceLevel;
    int      power;
    uint64_t gold; 
};

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
    uint64_t AcceptFriend(uint64_t userId, const std::string& friendName);
    bool RejectFriend(uint64_t userId, const std::string& friendName);
    uint64_t BlockFriend(uint64_t userId, const std::string& friendName);
    FriendList GetFriendList(uint64_t userId);
    std::vector<uint64_t> GetFriendIds(uint64_t userId);
    uint64_t LoginOrRegister(const std::string& name);

    uint64_t GetGold(uint64_t userId);
    uint64_t AddGold(uint64_t userId, uint64_t amount);
    uint64_t DropItem(uint64_t userId);
    std::vector<Item> GetItems(uint64_t userId);
    std::optional<Item> GetItem(uint64_t userId, uint64_t itemId);
    EnhanceResult EnhanceItem(uint64_t userId, uint64_t itemId,
        uint64_t cost, bool success, double mult);

private:
    pqxx::connection conn_;
};
