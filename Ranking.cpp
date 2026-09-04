#include "Ranking.h"

Ranking::Ranking(const std::string& uri, size_t poolSize)
    : redis_(uri + "?pool_size=" + std::to_string(poolSize)) { }

bool Ranking::Ping()
{
    return redis_.ping() == "PONG";
}

void Ranking::Update(uint64_t userId, uint64_t totalPower)
{
    try {
        redis_.zadd(KEY, std::to_string(userId), static_cast<double>(totalPower));
    }
    catch (const std::exception& e) {
        std::cerr << "[Redis] rank update failed (user " << userId << "): "
            << e.what() << "\n";
    }
}

void Ranking::Rebuild(const std::vector<RankEntry>& all)
{
    try {
        auto pipe = redis_.pipeline();
        pipe.del(KEY).del(NAME_KEY);

        std::vector<std::pair<std::string, double>> scores;
        std::vector<std::pair<std::string, std::string>> names;
        scores.reserve(all.size());
        names.reserve(all.size());
        for (const auto& r : all) {
            auto id = std::to_string(r.userId);
            scores.emplace_back(id, static_cast<double>(r.totalPower));
            names.emplace_back(id, r.name);
        }
        pipe.zadd(KEY, scores.begin(), scores.end())
            .hmset(NAME_KEY, names.begin(), names.end())
            .exec();
    }
    catch (const std::exception& e) {
        std::cerr << "[Redis] rebuild failed: " << e.what() << "\n";
    }
}

std::vector<RankEntry> Ranking::Top(int64_t start, int64_t stop)
{
    std::vector<RankEntry> out;
    try {
        std::vector<std::pair<std::string, double>> buf;
        redis_.zrevrange(KEY, start, stop, std::back_inserter(buf));
        if (buf.empty()) return out;

        std::vector<std::string> fields;
        fields.reserve(buf.size());
        for (const auto& [member, score] : buf)
            fields.push_back(member);

        std::vector<sw::redis::OptionalString> names;
        names.reserve(buf.size());
        redis_.hmget(NAME_KEY, fields.begin(), fields.end(),
            std::back_inserter(names));

        out.reserve(buf.size());
        for (size_t i = 0; i < buf.size(); ++i) {
            out.push_back({
                std::stoull(buf[i].first),
                static_cast<uint64_t>(buf[i].second),
                (i < names.size() && names[i]) ? *names[i] : std::string{}
                });
        }
    }
    catch (const std::exception& e) {
        std::cerr << "[Redis] top failed: " << e.what() << "\n";
    }
    return out;
}

void Ranking::SetName(uint64_t userId, std::string name) {
    std::pair<std::string, std::string>  p;
    p.first = std::to_string(userId);
    p.second = name;

    redis_.hset(NAME_KEY, p);
}

std::vector<std::string> Ranking::GetNames(const std::vector<uint64_t>& userIds)
{
    if (userIds.empty()) return {};

    std::vector<std::string> strUserIds;
    strUserIds.reserve(userIds.size());
    for (uint64_t id : userIds) {
        strUserIds.push_back(std::to_string(id));
    }

    std::vector<sw::redis::OptionalString> redisResults;
    redisResults.reserve(userIds.size());

    try {
        redis_.hmget(NAME_KEY, strUserIds.begin(), strUserIds.end(), std::back_inserter(redisResults));

        std::vector<std::string> names;
        names.reserve(redisResults.size());

        for (const auto& optName : redisResults) {
            if (optName) {
                names.push_back(*optName);
            }
            else {
                names.push_back(""); 
            }
        }

        return names;
    }
    catch (const std::exception& e) {
        std::cerr << "[Redis] GetNames failed: " << e.what() << "\n";
    }

    return {};
}

std::optional<int64_t> Ranking::RankOf(uint64_t userId)
{
    try {
        auto rank = redis_.zrevrank(KEY, std::to_string(userId));
        if (rank) return static_cast<int64_t>(*rank);
    }
    catch (const std::exception& e) {
        std::cerr << "[Redis] rankof failed (user " << userId << "): "
            << e.what() << "\n";
    }
    return std::nullopt;
}

