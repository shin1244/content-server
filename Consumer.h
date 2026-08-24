#pragma once
#include <thread>
#include "MPMCQueue.h"
#include "PacketHandler.h"
#include "Protocol.h" 
#include "Database.h"
class Consumer
{
public:
	void Start(MPMCQueue<Packet>* queue, SessionManager* sessions, Database* db);
	void Stop();
	
private:
	void Loop();
	void Handle(Packet& pkt);

	Database* db_;
	MPMCQueue<Packet>* queue_;
	SessionManager* session_manager_;
	std::thread thread_;
};

