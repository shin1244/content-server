# content-server

Windows IOCP 기반 TCP 채팅 서버입니다. 채팅만 되는 서버에서 시작해서 친구, 인벤토리,
랭킹처럼 게임에 흔히 들어가는 기능을 하나씩 붙여보고 있습니다.

- C++20 / Windows IOCP
- PostgreSQL, Redis
- 클라이언트는 Go로 두 개: 터미널 채팅 클라이언트, 부하 테스트용 봇

모든 입력은 채팅 한 줄입니다. `/` 로 시작하면 명령으로 처리합니다.

## 실행

환경변수 두 개가 필요합니다.

```
set SERVER_DB_DSN=postgresql://user:pw@host/db
set SERVER_REDIS_URL=tcp://127.0.0.1:6379
```

서버는 Visual Studio에서 빌드하고, 5050 포트로 엽니다. 클라이언트는 각각 이렇게 씁니다.

```
cd client && go build -o chat.exe . && chat.exe
cd bot    && go build -o bot.exe .  && bot.exe -n 100 -interval 1s
```

## 명령어

| 명령 | 설명 |
|---|---|
| `/n <이름>` | 로그인 (없으면 가입). 이름을 정하기 전엔 다른 명령이 막힙니다 |
| (그냥 입력) | 접속자 전체에게 브로드캐스트 |
| `/w <id> <내용>` | 귓속말 |
| `/f add <이름>` | 친구 요청 |
| `/f accept\|reject\|block <이름>` | 요청 수락 / 거절 / 차단 |
| `/f list` | 차단 · 받은 요청 · 친구 목록 |
| `/f <내용>` | 접속 중인 친구 전체에게 |
| `/i` | 골드, 총 전투력, 아이템 목록 |
| `/i enh <itemId>` | 강화 |

## 진행 상황

**되는 것**

- 채팅: 로그인/닉네임, 전체 채팅, 귓속말, 닉네임 동기화(접속 시 명단 1회 + 이후 변경분만)
- 친구: 요청 / 수락 / 거절 / 차단 / 목록 / 친구 채팅, 접속 중인 친구 캐시
- 인벤토리: 채팅 보상(골드 1~10, 1/50 확률로 아이템 드랍), 강화
- 랭킹: 강화 성공·아이템 드랍 시 Redis ZSET 갱신, 서버 기동 시 DB에서 전체 리빌드
- 클라이언트: 터미널 채팅 클라이언트, 봇 클라이언트

**다음**

- `/r` — 상위 10명 + 내 순위. `Ranking` 에 `Top`(ZREVRANGE) / `RankOf`(ZREVRANK) 추가 필요
- 거래 — 2인 상태머신, 서로 다른 샤드에 있는 두 유저를 다뤄야 해서 제일 큰 숙제
- 우편 — 오프라인 유저에게 전달
- 귓속말 대상이 없을 때 에러 응답

## 구조

| 파일 | 역할 |
|---|---|
| `NetworkCore` | IOCP 생성, AcceptEx, 워커 스레드 루프 |
| `Session` | 세션별 recv/send, 링버퍼, 패킷 조립, IO 참조 카운트 |
| `SessionManager` | id/이름/userId 레지스트리, 세션 풀, 샤드 큐 소유, 브로드캐스트 |
| `ShardServer` | 샤드별 { 큐 + Consumer 스레드 + DB 커넥션 } 소유 |
| `Consumer` | 큐에서 꺼내 명령 처리. 채팅/친구/인벤토리 로직이 여기 |
| `Database` | libpqxx 래퍼, 샤드마다 하나 |
| `Ranking` | Redis ZSET 래퍼, 전체 하나 (내부 커넥션 풀) |
| `Protocol` | 패킷 헤더와 `Packet` 정의 |
| `MPMCQueue` / `RingBuffer` / `ObjectPool` | 큐 / 링버퍼 / 세션 풀 |

```
클라 → IOCP 워커(N) → Session::OnRecv → 세션 id를 찍어서
                                       → shardQueues_[sessionId % 8].Push
                                                     │
샤드 컨슈머(8) → 큐 Pop → Consumer::Handle ──────────┘
                       → SessionManager / Database / Ranking
```

패킷은 `[크기 2B][세션 id 2B][본문]` 이고 본문은 최대 256바이트입니다. 클라이언트는 id를
0으로 보내고 서버가 자기 세션 id로 덮어씁니다. 서버가 보내는 id 0은 시스템 메시지입니다.

### DB 스키마

- `users(user_id, user_name unique, gold)`
- `items(item_id, owner_id, enhance_level, power, created_at)`
- `friendships(user_id, friend_id, status)` — status는 PENDING / ACCEPTED / BLOCKED

## 설계 메모

**샤딩** — `sessionId % 8` 로 유저를 컨슈머 스레드에 고정합니다. 한 유저의 패킷은 항상 같은
스레드에서 처리되니 유저 단위 상태는 락이 필요 없고, 명령 순서도 보장됩니다. 대신 두 유저를
동시에 건드리는 기능(거래)은 이 전제가 깨져서 따로 설계해야 합니다.

**강화는 SQL의 WHERE로 검증** — "잔액 확인하고 차감", "아이템 확인하고 적용"으로 명령을 분리하면 문제가 생길 가능성이 있습니다.
그래서 조건을 전부 UPDATE의 WHERE에 넣었습니다.

```sql
UPDATE users SET gold = gold - $2
WHERE user_id = $1 AND gold >= $2 RETURNING gold;   -- 0행이면 골드 부족
```

두 UPDATE를 한 트랜잭션에 넣었기 때문에 아이템 쪽이 실패하면 골드 차감도 같이 롤백됩니다.
소유 검증(`WHERE owner_id = $1`)은 동시성 때문이 아니라 클라가 남의 item_id를 찍어 보내는 걸
막기 위한 겁니다.

**랭킹 점수는 절대값으로** — ZINCRBY 대신 ZADD를 씁니다. 만약 power의 값 변경이 누락된다고 해도
절대값은 다음 갱신에서 알아서 맞춰집니다.

**커넥션은 샤드당 하나** — `pqxx::connection` 은 스레드 안전하지 않아서 샤드마다 `Database`
객체를 따로 만듭니다. `sw::redis::Redis` 는 내부에 커넥션 풀이 있어서 객체 하나를 공유하고
풀 크기만 샤드 수에 맞춥니다. 모양은 달라도 결국 스레드 8개에 커넥션 8개로 같습니다.

## 트러블슈팅 기록

**세션 재사용 시 이름과 userId가 남아 있던 문제** — `ObjectPool` 이 방금 끊긴 슬롯을 바로
재사용하는데 `Session::Init` 이 `name_` 과 `userId_` 를 안 지웠습니다. 새로 접속한 사람이
이전 사용자의 닉네임과 계정을 그대로 물려받아서, 로그인도 안 했는데 채팅이 되고 골드는 남의
계정에 쌓였습니다. 봇 100마리를 붙였다 뗐다 하다가 다른 사람 채팅이 제 닉네임으로 찍히는 걸
보고 찾았습니다.