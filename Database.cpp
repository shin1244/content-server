#include "Database.h"
#include <iostream>

Database::Database(const std::string& connString)
    : conn_(connString)
{
    std::cout << "[DB] connected: " << conn_.dbname() << "\n";
}

bool Database::Ping()
{
    try {
        pqxx::work tx(conn_);
        pqxx::row r = tx.exec("SELECT 1").one_row();
        tx.commit();
        return r[0].as<int>() == 1;
    }
    catch (const std::exception& e) {
        std::cerr << "[DB] ping failed: " << e.what() << "\n";
        return false;
    }
}

bool Database::AddFriend(uint64_t userId, const std::string& friendName)
{
    pqxx::work tx(conn_);

    pqxx::result found = tx.exec(
        "SELECT user_id FROM users WHERE user_name = $1",
        pqxx::params{ friendName });
    if (found.empty())
        return false;

    uint64_t friendId = found[0][0].as<uint64_t>();

    if (friendId == userId)
        return false;

    pqxx::result ins = tx.exec(
        "INSERT INTO friendships(user_id, friend_id) VALUES($1, $2) "
        "ON CONFLICT (user_id, friend_id) DO NOTHING "
        "RETURNING user_id",
        pqxx::params{ userId, friendId });

    tx.commit();

    return !ins.empty();
}

uint64_t Database::LoginOrRegister(const std::string& name)
{
    pqxx::work tx(conn_);
    pqxx::row r = tx.exec(
        "INSERT INTO users(user_name) VALUES($1) "
        "ON CONFLICT(user_name) DO UPDATE SET user_name = EXCLUDED.user_name "
        "RETURNING user_id",
        pqxx::params{ name }
    ).one_row();

    tx.commit();
    return r[0].as<uint64_t>();
}
