#include "PacketHandler.h"

#include <string>

void PacketHandler::Handle(Packet& pkt)
{
    int len = pkt.header.size;
    std::string text(pkt.message, len - HEADER_SIZE);

    std::istringstream iss(text);
    std::string cmd;
    iss >> cmd;

    if (!net_->IsNamed(pkt.header.id) && cmd != "/n")
    {
        std::string msg = "name plz.";
        Packet out = MakePacket(pkt.header.id, msg);
        net_->SendTo(pkt.header.id, reinterpret_cast<const char*>(&out), out.header.size);
        return;
    }

    if (text.empty() || text[0] != '/')
    {
        net_->Broadcast(reinterpret_cast<char*>(&pkt), len);
        return;
    }

    if (cmd == "/w")
    {
        uint64_t targetId = 0;
        iss >> targetId;
        std::string msg;
        std::getline(iss, msg);
        Packet out = MakePacket(pkt.header.id, msg);
        if (net_->SendTo(targetId, reinterpret_cast<const char*>(&out), out.header.size))
			net_->SendTo(pkt.header.id, reinterpret_cast<const char*>(&out), out.header.size);
    } else if (cmd == "\n")
    {
        std::string name;
        iss >> name;
        net_.
        std::string msg = "Hello!" + name;
        Packet out = MakePacket(pkt.header.id, msg);
        net_->SendTo(pkt.header.id, reinterpret_cast<const char*>(&out), out.header.size);
        return;
    }
}