#include <iostream>
#include <thread>
#include "NetworkCore.h"

int main()
{
	MPMCQueue<RecvEvent> recvQueue;
	NetworkCore core;
	core.Start(5050, &recvQueue);
	while (true){}
}