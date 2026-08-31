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

uint64_t Database::GetGold(uint64_t userId)
{
    pqxx::work tx(conn_);
    pqxx::row r = tx.exec(
        "SELECT gold FROM users WHERE user_id = $1",
        pqxx::params{ userId }).one_row();
    tx.commit();
    return r[0].as<uint64_t>();
}

uint64_t Database::AddGold(uint64_t userId, uint64_t amount)
{
    pqxx::work tx(conn_);
    pqxx::row r = tx.exec(
        "UPDATE users SET gold = gold + $2 WHERE user_id = $1 RETURNING gold",
        pqxx::params{ userId, amount }).one_row();
    tx.commit();
    return r[0].as<uint64_t>();
}

RankEntry Database::DropItem(uint64_t userId)
{
    pqxx::work tx(conn_);
    pqxx::row r = tx.exec(
        "INSERT INTO items(owner_id) VALUES($1) RETURNING item_id",
        pqxx::params{ userId }).one_row();

    pqxx::row tp = tx.exec(
        "SELECT COALESCE(SUM(power), 0) FROM items WHERE owner_id = $1",
        pqxx::params{ userId }).one_row();

    tx.commit();

    return { r[0].as<uint64_t>(), tp[0].as<uint64_t>() };
}

std::vector<Item> Database::GetItems(uint64_t userId)
{
    pqxx::work tx(conn_);
    pqxx::result r = tx.exec(
        "SELECT item_id, enhance_level, power FROM items WHERE owner_id = $1 ORDER BY item_id",
        pqxx::params{ userId });
    tx.commit();

    std::vector<Item> out;
    for (const auto& row : r)
        out.push_back({ row[0].as<uint64_t>(), row[1].as<int>(), row[2].as<int>() });
    return out;
}

std::optional<Item> Database::GetItem(uint64_t userId, uint64_t itemId)
{
    pqxx::work tx(conn_);
    pqxx::result r = tx.exec(
        "SELECT enhance_level, power FROM items "
        "WHERE owner_id = $1 AND item_id = $2",
        pqxx::params{ userId, itemId });
    tx.commit();

    if (r.empty()) return std::nullopt;      // 내 아이템이 아니거나 없음

    return Item{ itemId, r[0][0].as<int>(), r[0][1].as<int>() };
}

EnhanceResult Database::EnhanceItem(uint64_t userId, uint64_t itemId,
    uint64_t cost, bool success, double mult)
{
    pqxx::work tx(conn_);

    pqxx::result g = tx.exec(
        "UPDATE users SET gold = gold - $2 "
        "WHERE user_id = $1 AND gold >= $2 "
        "RETURNING gold",
        pqxx::params{ userId, cost });

    if (g.empty())
        return { EnhanceResult::Status::NoGold, 0, 0, 0 };

    pqxx::result it = tx.exec(
        "UPDATE items "
        "SET enhance_level = enhance_level + $3, "
        "    power         = ROUND(power * $4::float8)::int "
        "WHERE owner_id = $1 AND item_id = $2 "
        "RETURNING enhance_level, power",
        pqxx::params{ userId, itemId, success ? 1 : 0, mult });

    if (it.empty())
        return { EnhanceResult::Status::NotOwned, 0, 0, 0 };

    // 총 전투력 합계 계산
    pqxx::row t = tx.exec(
        "SELECT COALESCE(SUM(power), 0) FROM items WHERE owner_id = $1",
        pqxx::params{ userId }).one_row();

    // 트랜잭션 하나
    tx.commit();

    return {
        success ? EnhanceResult::Status::Success
                : EnhanceResult::Status::Failed,
        it[0][0].as<int>(),
        it[0][1].as<int>(),
        g[0][0].as<uint64_t>(),
        t[0].as<uint64_t>()
    };
}

std::vector<RankEntry> Database::GetAllPower()
{
    pqxx::work tx(conn_);
    pqxx::result r = tx.exec(
        "SELECT owner_id, SUM(power) FROM items GROUP BY owner_id");
    tx.commit();

    std::vector<RankEntry> out;
    out.reserve(r.size());
    for (const auto& row : r)
        out.push_back({ row[0].as<uint64_t>(), row[1].as<uint64_t>() });
    return out;
}

