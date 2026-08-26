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
	byUserId_.erase(s->GetUserId());
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

void SessionManager::SendToFriends(uint64_t userId, const char* data, int len)
{
	std::shared_lock g(lock_);

	// 1. 보낸 사람 세션 찾기 → friend set 얻기
	auto it = byUserId_.find(userId);
	if (it == byUserId_.end()) return;
	auto sit = byId_.find(it->second);
	if (sit == byId_.end()) return;

	// 2. 내 친구 set만 순회 (전체 유저 아님)
	for (uint64_t fid : sit->second->GetFriends())   // set을 const ref로 반환하는 접근자
	{
		auto fit = byUserId_.find(fid);
		if (fit == byUserId_.end()) continue;        // 그 친구 오프라인 → 건너뜀
		auto fsit = byId_.find(fit->second);
		if (fsit == byId_.end()) continue;
		fsit->second->Send(data, len);               // SendTo 아님! Send 직접 호출
	}
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

void SessionManager::LoadFriendCache(uint64_t userId, const std::vector<uint64_t>& ids)
{
	std::unique_lock g(lock_);
	auto it = byUserId_.find(userId);
	if (it == byUserId_.end()) return;
	auto sit = byId_.find(it->second);
	if (sit == byId_.end()) return;

	for (uint64_t id : ids)
		sit->second->addFriend(id);
}

// 세션의 친구 캐쉬에 추가
void SessionManager::CacheAddFriend(uint64_t userId, uint64_t friendId)
{
	std::unique_lock g(lock_);
	auto it = byUserId_.find(userId);
	if (it == byUserId_.end()) return;
	auto sit = byId_.find(it->second);
	if (sit == byId_.end()) return;
	sit->second->addFriend(friendId);
}

// 세션의 친구 캐쉬에 삭제
void SessionManager::CacheRemoveFriend(uint64_t userId, uint64_t friendId)
{
	std::unique_lock g(lock_);
	auto it = byUserId_.find(userId);
	if (it == byUserId_.end()) return;
	auto sit = byId_.find(it->second);
	if (sit == byId_.end()) return;
	sit->second->removeFriend(friendId);
}

// 세션에서 두 유저가 친구 관계인지 확인
bool SessionManager::AreFriends(uint64_t userId, uint64_t friendId)
{
	std::shared_lock lk(lock_);
	auto it = byUserId_.find(userId);
	if (it == byUserId_.end()) return false;
	auto sit = byId_.find(it->second);
	if (sit == byId_.end()) return false;
	return sit->second->hasFriend(friendId);
}

void SessionManager::SetUserId(uint64_t sessionId, uint64_t userId)
{
	std::unique_lock g(lock_);
	auto it = byId_.find(sessionId);
	if (it == byId_.end()) return;
	it->second->SetUserId(userId);
	byUserId_[userId] = sessionId;
}

uint64_t SessionManager::GetUserId(uint64_t sessionId)
{
	std::shared_lock g(lock_);
	auto it = byId_.find(sessionId);
	if (it == byId_.end()) return 0;
	return it->second->GetUserId();
}
