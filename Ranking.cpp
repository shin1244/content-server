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
        redis_.del(KEY);

        if (all.empty()) return;

        std::vector<std::pair<std::string, double>> buf;
        buf.reserve(all.size());
        for (auto r : all) {
            buf.emplace_back(std::to_string(r.userId), static_cast<double>(r.totalPower));
        }
        redis_.zadd(KEY, buf.begin(), buf.end());
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
        out.reserve(buf.size());
        for (const auto& [member, score] : buf)
            out.push_back({ std::stoull(member), static_cast<uint64_t>(score) });
    }
    catch (const std::exception& e)  {
        std::cerr << "[Redis] top failed: " << e.what() << "\n";
    }
    return out;
}

