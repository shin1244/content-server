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

inline Packet MakePacket(unsigned short senderId, const std::string& msg)
{
    Packet pkt{};
    int copyLen = std::min<int>(msg.size(), sizeof(pkt.message));
    std::memcpy(pkt.message, msg.data(), copyLen);
    pkt.header.id = senderId;
    pkt.header.size = static_cast<unsigned short>(HEADER_SIZE + copyLen);
    return pkt;
}