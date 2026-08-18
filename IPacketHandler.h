#pragma once
#include <cstdint>

#include "Protocol.h"

struct IPacketHandler
{
	virtual ~IPacketHandler() = default;
	virtual void OnPacket(uint64_t sessionId, const Packet& pkt) = 0;
};