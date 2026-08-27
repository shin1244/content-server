# 컨텐츠 서버

IOCP 기반 비동기 TCP 채팅 서버를 베이스로, "이런 간단해 보이는 기능은 실제로 어떻게 만들까?"라는 질문에서 시작해 컨텐츠를 하나씩 붙여가는 포트폴리오 프로젝트입니다.

- **베이스**: IOCP 비동기 TCP 서버 (Windows / C++)
- **영속성**: PostgreSQL (libpqxx)
- **동시성 모델**: `sessionId % N` 샤딩 — 한 유저는 항상 같은 컨슈머 스레드에서 처리(순서 보장 + 유저 상태 락-프리)
- **입력 규약**: 모든 입력은 채팅 한 줄. `/`로 시작하면 명령으로 해석

| 기호 | 의미 |
|------|------|
| ✅ | 구현 완료 |
| 🚧 | 진행중 / 예정 |

---

## 1. 채팅

유저 간 실시간 메시지 전달의 베이스. 모든 컨텐츠가 이 채팅 입력 위에 얹혀 있습니다.

- ✅ **로그인/닉네임 (`/n <이름>`)** — 로그인 겸 등록(upsert). 있으면 로그인, 없으면 가입, 항상 `user_id` 반환. 닉네임을 정하기 전에는 다른 명령이 막힘.
- ✅ **전체 채팅** — 그냥 입력하면 접속 중인 전원에게 브로드캐스트.
- ✅ **귓속말 (`/w <id> <내용>`)** — 특정 대상 1명 + 나에게만 전달.
- ✅ **닉네임 표시(스냅샷 + 델타)** — 채팅 패킷엔 id만 실림. 서버가 `NICK <id> <이름>`을 시스템 메시지(id=0)로 내려주고, 클라가 `id→이름` 맵을 유지. 접속 시 전체 명단(스냅샷) 1회 + 이후 변경은 델타.
- 🚧 귓속말 대상이 없을 때 에러 응답 / 입력 앞 공백 정리

## 2. 친구

영속 관계(로그아웃해도 유지)의 첫 진입. `user_id`를 관계의 불변 키로 사용합니다.

- ✅ **친구 요청 (`/f add <이름>`)** — 단방향 `PENDING` 행 생성. 자기 자신/미존재 대상 방어.
- ✅ **수락 (`/f accept <이름>`)** — `PENDING` → `ACCEPTED`.
- ✅ **거절 (`/f reject <이름>`)** — 요청 행 삭제(재요청 허용).
- ✅ **차단 (`/f block <이름>`)** — 기존 관계 정리 후 `BLOCKED`로 통일.
- ✅ **목록 (`/f list`)** — 차단 / 받은 요청 / 친구를 한 쿼리(UNION ALL)로 조회.
- ✅ **친구 전체 채팅 (`/f <내용>`)** — 서브커맨드에 안 걸리면 온라인 친구 전원에게 전달.
- ✅ **온라인 친구 캐시** — 로그인 시 친구 `user_id` 집합을 세션에 로드. accept/block 시 양방향 캐시 갱신.

## 3. 인벤토리

채팅 활동에 대한 보상 재화. 검은 강화 배율이 랜덤이라 스택이 아닌 **고유 인스턴스**로 저장합니다.

- ✅ **골드 / 아이템 드랍** — 채팅할 때마다 골드 1~10 지급 + 1/50 확률로 검 드랍.
- ✅ **인벤토리 보기 (`/i`)** — 보유 골드, 총 전투력, 검 목록(id / 강화수치 / power) 출력.
- ✅ **강화 (`/i enh <itemId>`)** — 골드를 소모해 검을 강화. 성공 시 `power × 1.20~1.25`, 레벨 +1. 실패해도 골드는 소모되고 검은 그대로(파괴 없음).
  - 비용 `100 × (레벨+1)`, 성공률 `max(20, 100 - 레벨×5)`% — 레벨이 오를수록 비싸지고 어려워짐
  - 난수는 컨슈머에서 굴리고 **배율·성공여부를 파라미터로 내려** DB는 SQL만 담당
- ✅ **스키마** — `items(item_id, owner_id, enhance_level, power, created_at)` + `users.gold`. power를 저장(강화 배율이 랜덤이라 레벨로 역산 불가).

### 설계 노트 — 강화의 원자성

강화의 진짜 주제는 확률이 아니라 **read-modify-write**입니다. "잔액 확인 → 차감", "소유 확인 → 적용"으로 나누면 그 사이가 TOCTOU 창이 돼요. 그래서 **검증을 전부 `WHERE` 절 안으로** 넣고 영향 행 수로 판정합니다.

```sql
-- 잔액 가드
UPDATE users SET gold = gold - $2
WHERE user_id = $1 AND gold >= $2 RETURNING gold;      -- 0행 → NoGold

-- 소유 가드
UPDATE items SET enhance_level = enhance_level + $3,
                 power = ROUND(power * $4::float8)::int
WHERE owner_id = $1 AND item_id = $2
RETURNING enhance_level, power;                        -- 0행 → NotOwned
```

- **두 UPDATE를 한 트랜잭션에** — 아이템 검증이 실패하면 골드 차감이 자동 롤백. 덕분에 "먼저 검증하고 차감"이라는 **순서 고민 자체가 사라짐**. `pqxx::work`는 커밋 없이 소멸하면 abort하므로 early return이 곧 롤백.
- **소유 검증은 동시성이 아니라 신뢰 경계** — 샤드 친화성 덕에 내 아이템 행을 두 스레드가 동시에 건드릴 일은 없다. `WHERE owner_id = $1`이 막는 건 **클라가 남의 `item_id`를 찍어 보내는 것**(노트 7). 거래가 들어오면 그때 같은 절이 동시성 가드로도 일하기 시작한다.
- **가격 조회와 권한 판정의 분리** — 비용·성공률이 `enhance_level`에 의존해 `GetItem`을 한 번 읽지만, 이건 **가격 책정용(참고)**이고 authoritative한 판정은 여전히 `WHERE`에 있다. 값이 낡아도 최악은 가격이 한 단계 어긋나는 것이지 무단 강화가 아니다.
- **결과는 sentinel이 아니라 타입으로** — 결과가 4종(Success/Failed/NotOwned/NoGold)이라 `bool`로 부족. `EnhanceResult` struct + `GetItem`은 `std::optional`. `0`을 실패로 쓰는 sentinel은 호출부가 규칙을 기억해야 하지만, 이쪽은 컴파일러가 강제한다.
- **libpqxx 함정** — `power * $4`에서 `power`가 `integer`라 Postgres가 `integer * integer`로 추론해 `"1.23"` 파싱에 실패한다. `$4::float8` 명시적 캐스트로 추론을 고정해야 함.

## 4. 랭킹

- 🚧 **전체 예정** — 읽기 폭주 / 쓰기 소수의 비대칭 부하를 어떻게 다룰지가 핵심 주제.

## 5. 거래

- 🚧 **전체 예정** — 2인 상태머신 + 데드락 회피 + 중도 이탈 롤백. 이 프로젝트의 클라이맥스.

## 6. 우편

- 🚧 **전체 예정** — 온라인/오프라인 경계 + 영속성 처리.

---

## 구조

| 파일 | 역할 |
|------|------|
| `NetworkCore` | IOCP 생성, AcceptEx, 워커 스레드 루프 (소켓 담당) |
| `Session` | 세션별 recv/send, 링버퍼, 패킷 파싱, IO 참조 카운트. 완성 패킷을 샤드 큐로 push |
| `SessionManager` | id/이름/userId 레지스트리, 세션 풀, 샤드 큐 소유, Broadcast/SendTo |
| `ShardServer` | 샤드별 { 큐 + Consumer 스레드 + Database 커넥션 } 소유 |
| `Consumer` | 컨텐츠 계층. 큐 Pop → 명령 디스패치(채팅/친구/인벤토리) |
| `Database` | PostgreSQL(libpqxx) 래퍼 |
| `Protocol` | 패킷 헤더/`Packet` 정의, `MakePacket` 팩토리 |
| `MPMCQueue` / `RingBuffer` / `ObjectPool` / `NetTypes` | 큐 / 링버퍼 / 세션 풀 / IO 컨텍스트 |

### 데이터 흐름

```
클라 → IOCP 워커(N개) → Session::OnRecv → (경계에서 서버 id stamping)
                                          → shardQueues_[sessionId % N].Push
                                                        │
샤드 컨슈머 스레드(N개) → 큐 Pop → Consumer::Handle ────┘
                              → 명령 디스패치 → SessionManager / Database
```
프로듀서 여럿(IOCP 워커) → 큐 → 소비자 하나(샤드). 한 유저는 항상 같은 샤드로 배정돼 유저 상태를 락 없이 다룹니다.
