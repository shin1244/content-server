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
	while (true){}
}
