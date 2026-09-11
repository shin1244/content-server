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

enum class MsgSrc : uint8_t { Client, TradeRequest };

inline Packet MakePacket(unsigned short id, const std::string& msg)
{
    Packet pkt{};
    int copyLen = std::min<int>(msg.size(), sizeof(pkt.message));
    std::memcpy(pkt.message, msg.data(), copyLen);
    pkt.header.id = id;
    pkt.header.size = static_cast<unsigned short>(HEADER_SIZE + copyLen);
    return pkt;
}

enum class MsgType : uint8_t
{
    Client,
    TradeRequest,
    TradeAccept,
    TradeReject,
    TradeCancel,
    TradeDisconnect,
};

struct ShardMsg
{
    MsgType  type = MsgType::Client;
    uint64_t sessionId = 0;
    uint64_t fromSid = 0;       // 보낸 세션. Client일 땐 안 씀
    Packet   pkt{};             // Client일 때만 사용
};