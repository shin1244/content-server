#include "SessionManager.h"

Session* SessionManager::Create(SOCKET sock, MPMCQueue<Packet>* h)
{
	std::unique_lock g(lock_);
	int index = pool_.Alloc();
	if (index == -1) return nullptr;
	Session* s = &pool_[index];
	uint64_t id = nextId_.fetch_add(1);
	s->Init(sock, index, id, h);
	byId_[id] = s;
	return s;
}

void SessionManager::Destroy(Session* s)
{
	std::unique_lock g(lock_);
	byId_.erase(s->GetId());
	byName_.erase(s->GetName());
	pool_.Free(s->GetIndex());
}

void SessionManager::Broadcast(char* data, int len)
{
	std::shared_lock g(lock_);
	for (auto& [id, s] : byId_) {
		if (s->IsNamed())
			s->Send(data, len);
	}
}

bool SessionManager::SendTo(uint64_t id, const char* data, int len)
{
	std::shared_lock g(lock_);
	auto it = byId_.find(id);
	if (it == byId_.end()) return false;
	it->second->Send(data, len);
	return true;
}

bool SessionManager::IsNamed(uint64_t id)
{
	std::shared_lock g(lock_);
	auto it = byId_.find(id);
	return it != byId_.end() && it->second->IsNamed();
}

bool SessionManager::SetName(uint64_t id, std::string name)
{
	std::unique_lock g(lock_); 
	if (byName_.count(name)) return false;
	auto it = byId_.find(id);
	if (it == byId_.end()) return false;
	it->second->SetName(name);
	byName_[name] = id; 
	return true;
}

void SessionManager::SendRosterTo(uint64_t id)
{
	std::shared_lock g(lock_); 

	auto target = byId_.find(id);
	if (target == byId_.end()) return;
	Session* to = target->second;

	for (auto& [sid, s] : byId_)
	{
		if (sid == id) continue; 
		if (!s->IsNamed()) continue;

		std::string line = "NICK " + std::to_string(sid) + " " + s->GetName();
		Packet pkt = MakePacket(0, line); 
		to->Send(reinterpret_cast<const char*>(&pkt), pkt.header.size);
	}
}

void SessionManager::SetUserId(uint64_t sessionId, uint64_t userId)
{
	byId_[sessionId]->SetUserId(userId);
}

uint64_t SessionManager::GetUserId(uint64_t sessionId)
{
	return byId_[sessionId]->GetUserId(sessionId);
}
