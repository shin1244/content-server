#pragma once
#include <thread>
#include "MPMCQueue.h"
#include "PacketHandler.h"
#include "Protocol.h" 
class Consumer
{
public:
	void Start(MPMCQueue<Packet>* queue, SessionManager* sessions);
	void Stop();
private:
	void Loop();
	void Handle(Packet& pkt);

	MPMCQueue<Packet>* queue_;
	SessionManager* session_manager_;
	std::thread thread_;
};

