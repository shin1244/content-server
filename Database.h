#pragma once
#include <string>
#include <cstdint>
#include <pqxx/pqxx>

class Database
{
public:
    explicit Database(const std::string& connString);
    bool Ping();               
    uint64_t LoginOrRegister(const std::string& name);

private:
    pqxx::connection conn_;
};
