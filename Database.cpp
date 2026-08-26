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

uint64_t Database::AcceptFriend(uint64_t userId, const std::string& friendName)
{
    pqxx::work tx(conn_);

    pqxx::result found = tx.exec(
        "SELECT user_id FROM users WHERE user_name = $1",
        pqxx::params{ friendName });
    if (found.empty())
        return 0;

    uint64_t requesterId = found[0][0].as<uint64_t>();

    pqxx::result upd = tx.exec(
        "UPDATE friendships SET status = 'ACCEPTED' "
        "WHERE user_id = $1 AND friend_id = $2 AND status = 'PENDING' "
        "RETURNING user_id",
        pqxx::params{ requesterId, userId });

    tx.commit();

    if (upd.empty())
        return 0; // 실패

    return requesterId; // 성공 → 상대 id 반환
}

bool Database::RejectFriend(uint64_t userId, const std::string& friendName)
{
    pqxx::work tx(conn_);

    pqxx::result found = tx.exec(
        "SELECT user_id FROM users WHERE user_name = $1",
        pqxx::params{ friendName });
    if (found.empty())
        return false;

    uint64_t requesterId = found[0][0].as<uint64_t>();

    pqxx::result del = tx.exec(
        "DELETE FROM friendships "
        "WHERE user_id = $1 AND friend_id = $2 AND status = 'PENDING' "
        "RETURNING user_id",
        pqxx::params{ requesterId, userId });

    tx.commit();

    return !del.empty();
}

uint64_t Database::BlockFriend(uint64_t userId, const std::string& friendName)
{
    pqxx::work tx(conn_);

    pqxx::result found = tx.exec(
        "SELECT user_id FROM users WHERE user_name = $1",
        pqxx::params{ friendName });
    if (found.empty())
        return 0;

    uint64_t targetId = found[0][0].as<uint64_t>();
    if (targetId == userId)
        return 0;

    tx.exec(
        "DELETE FROM friendships "
        "WHERE (user_id = $1 AND friend_id = $2) OR (user_id = $2 AND friend_id = $1)",
        pqxx::params{ userId, targetId });

    tx.exec(
        "INSERT INTO friendships(user_id, friend_id, status) VALUES($1, $2, 'BLOCKED')",
        pqxx::params{ userId, targetId });

    tx.commit();
    return targetId;
}

FriendList Database::GetFriendList(uint64_t userId)
{
    pqxx::work tx(conn_);
    pqxx::result r = tx.exec(
        "SELECT 'B' AS kind, u.user_name "
        "  FROM friendships f JOIN users u ON u.user_id = f.friend_id "
        " WHERE f.user_id = $1 AND f.status = 'BLOCKED' "
        "UNION ALL "
        "SELECT 'P', u.user_name "
        "  FROM friendships f JOIN users u ON u.user_id = f.user_id "
        " WHERE f.friend_id = $1 AND f.status = 'PENDING' "
        "UNION ALL "
        "SELECT 'F', u.user_name "
        "  FROM friendships f "
        "  JOIN users u ON u.user_id = CASE WHEN f.user_id = $1 "
        "                                   THEN f.friend_id ELSE f.user_id END "
        " WHERE (f.user_id = $1 OR f.friend_id = $1) AND f.status = 'ACCEPTED'",
        pqxx::params{ userId });
    tx.commit();

    FriendList out;
    for (const auto& row : r) {
        std::string kind = row[0].as<std::string>();
        std::string name = row[1].as<std::string>();
        if (kind == "B") out.blocked.push_back(name);
        else if (kind == "P") out.pending.push_back(name);
        else                  out.friends.push_back(name);
    }
    return out;
}

std::vector<uint64_t> Database::GetFriendIds(uint64_t userId)
{
    pqxx::work tx(conn_);
    pqxx::result r = tx.exec(
        "SELECT CASE WHEN user_id = $1 THEN friend_id ELSE user_id END "
        "FROM friendships "
        "WHERE (user_id = $1 OR friend_id = $1) AND status = 'ACCEPTED'",
        pqxx::params{ userId });
    tx.commit();

    std::vector<uint64_t> out;
    for (const auto& row : r)
        out.push_back(row[0].as<uint64_t>());
    return out;
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
