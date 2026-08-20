#pragma once
#include <cstdint>
#include <sstream>
#include "Protocol.h"
#include "SessionManager.h"

class PacketHandler
{
public:
	PacketHandler(SessionManager* sessions) : session_manager_(sessions) {}
	void Handle(Packet& pkt);

private:
	SessionManager* session_manager_;
};

