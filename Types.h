#pragma once
#include <cstdint>

struct RankEntry
{
    uint64_t userId;
    uint64_t totalPower;
};

struct DropResult { uint64_t itemId; uint64_t totalPower; };