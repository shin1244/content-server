#pragma once
#include <thread>
#include "MPMCQueue.h"
#include "PacketHandler.h"
#include "Protocol.h" 
class Consumer
{
public:
	Consumer(MPMCQueue<Packet>* queue, PacketHandler* handler)
		: queue_(queue), handler_(handler) {}
	void Start();
	void Stop();
private:
	void Loop();
	MPMCQueue<Packet>* queue_;
	PacketHandler* handler_; 
	std::thread thread_;
};

