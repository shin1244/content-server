#define _CRT_SECURE_NO_WARNINGS

#include "MPMCQueue.h"
#include "NetworkCore.h"
#include "PacketHandler.h"
#include "Consumer.h"
#include "Database.h"
#include <iostream>
#include "ShardServer.h"

int main()
{
    const char* dsnEnv = std::getenv("SERVER_DB_DSN");
    if (!dsnEnv) {
        std::cerr << "[Error] SERVER_DB_DSN 환경변수가 설정되지 않았습니다.\n";
        return 1;
    }

    SessionManager sessions;
    NetworkCore core(&sessions);
    ShardServer shards;

    shards.Start(8, dsnEnv, &sessions);
    core.Start(5050);

    std::cout << "[Server] Press Enter to shutdown...\n";
    std::cin.get(); 

    shards.Stop();
    return 0;
}