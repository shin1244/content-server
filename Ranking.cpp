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
