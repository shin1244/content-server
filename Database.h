#pragma once
#include <string>
#include <cstdint>
#include <pqxx/pqxx>

class Database
{
public:
    explicit Database(const std::string& connString);
    bool     Ping();                                    

private:
    pqxx::connection conn_;
};
