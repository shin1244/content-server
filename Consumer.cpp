#include "Consumer.h"

void Consumer::Start()
{
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
		handler_->Handle(pkt);
}
