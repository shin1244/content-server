#define _CRT_SECURE_NO_WARNINGS

#include "MPMCQueue.h"
#include "NetworkCore.h"
#include "PacketHandler.h"
#include "Consumer.h"
#include "Database.h"
#include <iostream>
#include <cstdlib>

int main()
{
    const char* dsnEnv = std::getenv("SERVER_DB_DSN");
    if (!dsnEnv) {
        std::cerr << "[Error] SERVER_DB_DSN 환경변수가 설정되지 않았습니다.\n";
        return 1;
    }

    std::unique_ptr<Database> db;
    try {
        db = std::make_unique<Database>(dsnEnv);
        if (db->Ping()) {
            std::cout << "[DB] ping OK\n";
        }
    }
    catch (const std::exception& e) {
        std::cerr << "[DB] connect failed: " << e.what() << "\n";
        return 1;
    }

    SessionManager sessions;
    MPMCQueue<Packet> queue;
    NetworkCore core(&sessions);
    Consumer consumer;

    core.Start(5050, &queue);

    consumer.Start(&queue, &sessions, db.get());

    std::cout << "[Server] Press Enter to shutdown...\n";
    std::cin.get(); 

    // 서버 종료 처리
    consumer.Stop();
    core.Stop();

    return 0;
}