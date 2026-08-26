#pragma once
#include"SessionManager.h"
#include"Database.h"
#include"Consumer.h"

class ShardServer {
    SessionManager* sessions_;
    std::vector<std::unique_ptr<Database>> dbs_;
    std::vector<std::unique_ptr<Consumer>> consumers_;
public:
    void Start(size_t n, const char* dsn, SessionManager* s) {
        sessions_ = s;
        s->InitShards(n);
        for (size_t i = 0; i < n; ++i) {
            dbs_.push_back(std::make_unique<Database>(dsn));
            consumers_.push_back(std::make_unique<Consumer>());
            consumers_.back()->Start(s->GetQueue(i), s, dbs_.back().get());
        }
    }
    void Stop() { for (auto& c : consumers_) c->Stop(); }
};