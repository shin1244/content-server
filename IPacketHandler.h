#pragma once
#include <cstdint>

#include "Protocol.h"

struct IPacketHandler
{
	virtual ~IPacketHandler() = default;
	virtual void OnPacket(const Packet& pkt) = 0;
};