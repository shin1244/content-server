#include <iostream>
#include <thread>

#include "MPMCQueue.h"
#include "NetworkCore.h"
#include "QueueSink.h"

int main()
{
	MPMCQueue<RecvEvent> recvQueue;
	QueueSink q(&recvQueue);
	NetworkCore core;
	core.Start(5050, &q);

    std::thread consumer([&] {
        RecvEvent evt;
        while (recvQueue.Pop(evt)) {
            evt.packet.header.id = static_cast<unsigned short>(evt.sessionId);
            int len = evt.packet.header.size;
            core.Broadcast(reinterpret_cast<char*>(&evt.packet), len);
        }
        });

    consumer.join();
}