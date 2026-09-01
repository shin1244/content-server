#include "Ranking.h"

Ranking::Ranking(const std::string& uri, size_t poolSize)
    : redis_(uri + "?pool_size=" + std::to_string(poolSize)) { }

bool Ranking::Ping()
{
    return redis_.ping() == "PONG";
}

void Ranking::Update(uint64_t userId, int64_t totalPower)
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
    const std::string tmpKey = std::string(KEY) + ":rebuild";

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
