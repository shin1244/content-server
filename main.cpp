#include "MPMCQueue.h"
#include "NetworkCore.h"
#include "QueueSink.h"
#include "PacketHandler.h"
#include "Consumer.h"

int main()
{
	SessionManager sessions;
	MPMCQueue<Packet> queue;
	NetworkCore core(&sessions);
	Consumer consumer;


	core.Start(5050, &queue);
	consumer.Start();

	while (true) {}

	consumer.Stop();
	core.Stop();
}