#pragma once
#include <sw/redis++/redis++.h>
#include <vector>
#include <string>
#include <cstdint>
#include <iostream>
#include "Types.h"

class Ranking
{
public:
	Ranking(const std::string& uri, size_t poolSize);
	bool Ping();
	void Update(uint64_t userId, uint64_t totalPower);
	void Rebuild(const std::vector<RankEntry>& all);
	std::vector<RankEntry> Top(int64_t start, int64_t stop);
	std::optional<int64_t> RankOf(uint64_t userId);

private:
	static constexpr const char* KEY = "lb:power";
	sw::redis::Redis redis_;
};