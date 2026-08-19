#include "SessionManager.h"

void SessionManager::Add(Session* s)
{
	uint64_t id = nextId_.fetch_add(1);
	s->SetId(id);
	std::unique_lock g(lock_);
	byId_[id] = s;
}

void SessionManager::Remove(Session* s)
{
	std::unique_lock g(lock_);
	byId_.erase(s->GetId());
	byName_.erase(s->GetName());
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