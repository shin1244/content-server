#include "PacketHandler.h"

#include <string>

void PacketHandler::Handle(Packet& pkt)
{
    int len = pkt.header.size;
    std::string text(pkt.message, len - HEADER_SIZE);
    if (text.empty() || text[0] != '/')
    {
        net_->Broadcast(reinterpret_cast<char*>(&pkt), len);
        return;
    }
    std::istringstream iss(text);
    std::string cmd;
    iss >> cmd;
    if (cmd == "/w")
    {
        uint64_t targetId = 0;
        iss >> targetId;
        std::string msg;
        std::getline(iss, msg);
        Packet out = MakePacket(pkt.header.id, msg);
        net_->SendTo(targetId, reinterpret_cast<const char*>(&out), out.header.size);
        net_->SendTo(pkt.header.id, reinterpret_cast<const char*>(&out), out.header.size);
    }
}