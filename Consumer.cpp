#include "Consumer.h"

#include <iostream>

void Consumer::Start(MPMCQueue<Packet>* queue, SessionManager* sessions, Database* db)
{
    queue_ = queue;
    session_manager_ = sessions;
    thread_ = std::thread([this] { Loop(); });
    db_ = db;
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

        if (name.size() < 3 || name.size() >= 10)
        {
            Packet err = MakePacket(0, "invalid name");
            session_manager_->SendTo(pkt.header.id,
                reinterpret_cast<const char*>(&err), err.header.size);
            return;
        }

        if (session_manager_->IsNamed(pkt.header.id))
        {
            Packet err = MakePacket(0, "already name");
            session_manager_->SendTo(pkt.header.id,
                reinterpret_cast<const char*>(&err), err.header.size);
            return;
        }

        uint64_t userId = 0;
        try
        {
            userId = db_->LoginOrRegister(name);
        }
        catch (const std::exception& e)
        {
            std::cerr << "[DB] LoginOrRegister failed: " << e.what() << "\n";
            Packet err = MakePacket(0, "server error");
            session_manager_->SendTo(pkt.header.id,
                reinterpret_cast<const char*>(&err), err.header.size);
            return;
        }

        if (!session_manager_->SetName(pkt.header.id, name))
        {
            Packet err = MakePacket(0, "already online");
            session_manager_->SendTo(pkt.header.id,
                reinterpret_cast<const char*>(&err), err.header.size);
            return;
        }

        session_manager_->SetUserId(pkt.header.id, userId);

        session_manager_->SendRosterTo(pkt.header.id);

        std::string announce = "NICK " + std::to_string(pkt.header.id) + " " + name;
        Packet a = MakePacket(0, announce);
        session_manager_->Broadcast(reinterpret_cast<char*>(&a), a.header.size);
    }
}
