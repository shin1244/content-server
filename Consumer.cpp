#include "Consumer.h"

void Consumer::Start(MPMCQueue<Packet>* queue)
{
	queue_ = queue;
    thread_ = std::thread([this] { Loop(); });
}

void Consumer::Stop()
{
	queue_->Stop();
	if (thread_.joinable())
		thread_.join();
}

void Consumer::Loop()
{
	Packet pkt;
	while (queue_->Pop(pkt))
	{
		
	}
}
