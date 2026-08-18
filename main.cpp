#include <iostream>
#include <thread>

#include "MPMCQueue.h"
#include "NetworkCore.h"
#include "QueueSink.h"
#include "PacketHandler.h"

int main()
{
	MPMCQueue<Packet> queue;
	QueueSink q(&queue);
	NetworkCore core;
    PacketHandler h(&core);
	core.Start(5050, &q);

    std::thread consumer([&] {
        Packet pkt;
        while (queue.Pop(pkt)) {
            h.Handle(pkt);
        }
        });

    consumer.join();
}