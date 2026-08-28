#pragma once
#include <sw/redis++/redis++.h>
#include <vector>
#include <string>
#include <cstdint>
#include <iostream>

class Ranking
{
public:
	Ranking(const std::string& uri, size_t poolSize);
	bool Ping();
	void Update(uint64_t userId, int64_t totalPower);
	//void Rebuild(const std::vector<RankEntry>& all);

private:
	static constexpr const char* KEY = "lb:power";
	sw::redis::Redis redis_;
};

