#pragma once

const int HEADER_SIZE = 4;

#pragma pack(push, 1)
struct PacketHeader { unsigned short size; unsigned short id; };
struct Packet
{
	PacketHeader header;
	char message[256];
};
#pragma pack(pop)