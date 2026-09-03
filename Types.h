#pragma once
#include <cstdint>
#include <string>

struct RankEntry
{
    uint64_t userId;
    uint64_t totalPower;
    std::string name;
};

struct DropResult { uint64_t itemId; uint64_t totalPower; };