#pragma once
#pragma pack(push, 1)
struct PacketHeader { unsigned short size; unsigned short id; };
#pragma pack(pop)

const int HEADER_SIZE = 4;