#include "Consumer.h"

void Consumer::Start(MPMCQueue<Packet>* queue, SessionManager* sessions)
{
	queue_ = queue;
	session_manager_ = sessions;
    thread_ = std::thread([this] { Loop(); });
}

void Consumer::Stop()
{
	queue_->Stop();
	if (thread_.joinable())
		thread_.join();
}

void Consumer::Loop()
{
	Packet pkt;
	while (queue_->Pop(pkt))
	{
        Handle(pkt);
	}
}

void Consumer::Handle(Packet& pkt)
{
    int len = pkt.header.size;
    std::string text(pkt.message, len - HEADER_SIZE);

    std::istringstream iss(text);
    std::string cmd;
    iss >> cmd;

    if (!session_manager_->IsNamed(pkt.header.id) && cmd != "/n")
    {
        std::string msg = "name plz.";
        Packet out = MakePacket(pkt.header.id, msg);
        session_manager_->SendTo(pkt.header.id, reinterpret_cast<const char*>(&out), out.header.size);
        return;
    }

    if (text.empty() || text[0] != '/')
    {
        session_manager_->Broadcast(reinterpret_cast<char*>(&pkt), len);
        return;
    }

    if (cmd == "/w")
    {
        uint64_t targetId = 0;
        iss >> targetId;
        std::string msg;
        std::getline(iss, msg);
        Packet out = MakePacket(pkt.header.id, msg);
        if (session_manager_->SendTo(targetId, reinterpret_cast<const char*>(&out), out.header.size))
            session_manager_->SendTo(pkt.header.id, reinterpret_cast<const char*>(&out), out.header.size);
    }
    else if (cmd == "/n")
    {
        std::string name;
        iss >> name;

        if (!session_manager_->SetName(pkt.header.id, name))
        {
            Packet err = MakePacket(0, "already taken");
            session_manager_->SendTo(pkt.header.id, reinterpret_cast<const char*>(&err), err.header.size);
            return;
        }

        session_manager_->SendRosterTo(pkt.header.id);

        std::string announce = "NICK " + std::to_string(pkt.header.id) + " " + name;
        Packet a = MakePacket(0, announce);
        session_manager_->Broadcast(reinterpret_cast<char*>(&a), a.header.size);
    }
}
