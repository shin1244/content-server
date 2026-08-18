#pragma once
#include <cstdint>
#include <sstream>
#include "Protocol.h"
#include "NetworkCore.h"

class PacketHandler
{
public:
	PacketHandler(NetworkCore* net) : net_(net) {}
	void Handle(Packet& pkt);

private:
	NetworkCore* net_;
};

