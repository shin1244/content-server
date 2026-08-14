#include <iostream>
#include <thread>
#include "NetworkCore.h"

int main()
{
	NetworkCore core;
	core.Start(5050);
	while (true){}
}