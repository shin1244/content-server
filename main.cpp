#include "MPMCQueue.h"
#include "NetworkCore.h"
#include "QueueSink.h"
#include "PacketHandler.h"
#include "Consumer.h"

int main()
{
	SessionManager sessions;
	MPMCQueue<Packet> queue;
	QueueSink q(&queue);
	NetworkCore core(&sessions);
	PacketHandler handler(&sessions);
	Consumer consumer(&queue, &handler);


	core.Start(5050, &q);
	consumer.Start();

	while (true) {}

	consumer.Stop();
	core.Stop();
}