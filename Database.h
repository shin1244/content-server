#pragma once
#include <string>
#include <cstdint>
#include <pqxx/pqxx>

class Database
{
public:
    explicit Database(const std::string& connString);
    bool Ping();               
    bool AddFriend(uint64_t userId, const std::string& friendName);
    bool AcceptFriend(uint64_t userId, const std::string& friendName);
    bool RejectFriend(uint64_t userId, const std::string& friendName);
    uint64_t LoginOrRegister(const std::string& name);

private:
    pqxx::connection conn_;
};
