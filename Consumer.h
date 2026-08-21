#pragma once
#include <thread>
#include "MPMCQueue.h"
#include "PacketHandler.h"
#include "Protocol.h" 
class Consumer
{
public:
	void Start(MPMCQueue<Packet>* queue);
	void Stop();
private:
	void Loop();
	MPMCQueue<Packet>* queue_;
	std::thread thread_;
};

