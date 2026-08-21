간단한 채팅 서버를 베이스로 여러 컨텐츠들을 넣을 예정입니다.

넣을 컨텐츠는 이런 간단해 보이는 기능은 어떻게 만들었을까? 라는 질문에서 시작해 제가 생각하는 자료구조로 컨텐츠를 제작한 이후,

AI에게 질문해 실무에서 사용하는 자료구조에 대한 정보를 들은 후 직접 구현 + 제가 원래 만들었던 자료구조와 실무 자료구조를 벤치마크하는 방식

---

## 프로젝트 방향

- 베이스: IOCP 기반 비동기 TCP 채팅 서버 (Windows)
- 모든 유저 입력은 **채팅 한 줄**. `/`로 시작하면 명령으로 해석
  - 그냥 입력 → 전챗
  - `/이름 ...` → 귓속말
  - `/친구 ...` → 친구 전체 채팅
  - `/차단 이름` → 차단(안 보이기)
  - `/랭킹`, `/거래 이름 · 수락 · 아이템 · 확인`, `/우편` 등 → 컨텐츠

## 구조 (현재)

| 파일 | 역할 |
|------|------|
| `NetworkCore` | IOCP 생성, AcceptEx, 워커 스레드 루프, 세션 배정. `Broadcast`/`SendTo` facade |
| `Session` | 세션별 recv/send, 링버퍼, 패킷 파싱, IO 참조 카운트. 완성 패킷을 핸들러로 push |
| `SessionManager` | id→세션 레지스트리(`byId_`), id 발급(`nextId_`), Broadcast/SendTo |
| `RingBuffer` | 송수신용 링버퍼 |
| `ObjectPool` | 세션 고정 풀(최대 1000) |
| `NetTypes` | OverlappedEx / Accept·Recv·Send 컨텍스트 |
| `Protocol` | 패킷 헤더/`Packet`/`RecvEvent` 정의, `MakePacket` 팩토리 |
| `MPMCQueue` | 멀티 프로듀서/컨슈머 큐 (mutex + condvar) |
| `IPacketHandler` | 네트워크→컨텐츠 경계 인터페이스 (`OnPacket`) |
| `QueueSink` | `IPacketHandler` 구현 = 어댑터. `OnPacket` → 큐 `Push` |
| `PacketHandler` | 컨텐츠 계층. 큐에서 꺼낸 패킷을 종류별로 처리(전챗/귓속말) |

## 데이터 흐름

```
클라 → IOCP 워커(N개) → Session::OnRecv → handler_->OnPacket(id, pkt)
                                              → QueueSink → recvQueue.Push
                                                                │
컨슈머 스레드(1개) → recvQueue.Pop → PacketHandler::Handle ──────┘
                        → 전챗: NetworkCore::Broadcast (전원)
                        → 귓말: NetworkCore::SendTo   (상대 + 나)
```
프로듀서 여럿(IOCP 워커) → 큐 → 소비자 하나. 이래서 MPMC 큐가 경계에 필요.

## 설계 노트 — 왜 이렇게 결정했나

> 컨텐츠(전챗/귓속말)를 붙이며 부딪힌 설계 질문과 그 결론. "문법"이 아니라 "왜"의 기록.

### 1. 계층 경계와 의존성 주입 — 큐를 누가 소유하나
- 큐는 **네트워크 계층(생산자)과 컨텐츠 계층(소비자)의 경계**. NetworkCore가 소유하는 게 아니라 `main`이 소유하고 **포인터로 주입**한다.
- 배선: `main`(큐 소유) → `NetworkCore`에 주입 → `Session`에 전달. 세 곳의 포인터가 같은 큐 하나를 가리킴.
- **수명 규칙**: 큐가 NetworkCore보다 오래 살아야 함(선언 순서로 보장). 안 그러면 dangling.

### 2. 인터페이스로 결합 끊기 — `IPacketHandler` + `QueueSink`(어댑터)
- NetworkCore가 `MPMCQueue<RecvEvent>*`를 직접 알면 **저수준 네트워크 모듈이 게임 타입(Packet/RecvEvent)에 오염**됨.
- 그래서 `IPacketHandler`(순수 가상 `OnPacket`)에만 의존 → **의존성 역전**. 구체 큐는 몰라도 됨.
- `QueueSink`는 인터페이스 시그니처(`OnPacket(id, pkt)`)와 큐 시그니처(`Push(T)`)를 잇는 **어댑터**. 큐를 아는 유일한 곳.
- **MPMCQueue를 직접 `IPacketHandler` 상속시키면 안 되는 이유**: 시그니처 불일치(변환이 어딘가 필요) + 범용 큐가 프로토콜 타입에 오염 + `MPMCQueue<int>`가 깨짐 + "큐 is-a 핸들러"는 거짓.
- 검증: 리팩터 후 `NetworkCore.h`/`Session.h`에 `MPMCQueue`가 안 보이면 결합이 끊긴 것.

### 3. 이름은 메커니즘이 아니라 역할로
- `OnPacket`(사건: 패킷이 왔다) vs `Push`(메커니즘: 큐에 넣기). 인터페이스 목적이 "큐를 숨기기"인데 `Push`라 이름 지으면 큐를 다시 노출.
- 규칙: 인터페이스=역할(`OnPacket`), 큐=연산(`Push`). 두 단어가 각자 맞는 층에 존재.

### 4. 인터페이스는 경계에만 (YAGNI)
- 소비자(로직 스레드)는 큐를 **직접 `Pop`**. 자료구조 앞엔 인터페이스를 안 붙인다(`std::queue` 앞에 안 붙이듯).
- 인터페이스는 ①모듈 경계를 넘거나 ②구현을 갈아끼울 때만. 둘 다 아니면 구체 타입이 낫다. 모든 이음새에 반사적으로 인터페이스 = 안티패턴.

### 5. Session이 핸들러를 아는 이유 — 캡슐화
- 완성된 패킷은 **Session 내부(`recvBuffer_`)에서 조립**된다. 만드는 놈이 넘기는 놈.
- NetworkCore가 대신 넘기려면 Session의 private 버퍼를 열거나 조립 로직을 옮겨야 함 → 캡슐화/책임분리 붕괴. 그래서 핸들러를 Session까지 내려보냄.

### 6. 신원은 서버가 정한다 — id 두 종류
- `PacketHeader.id`(클라가 보냄) ≠ **Session id**(`SessionManager`가 `nextId_++`로 발급). 서버가 authority.
- **senderId는 방향에 따라 다르다**:
  - 인바운드(클라→서버): 서버가 세션으로 이미 앎 → **클라가 보낸 값 무시**, sentinel(0)로.
  - 아웃바운드(브로드캐스트): 받는 클라들은 세션 테이블이 없음 → **서버가 발급 id를 패킷에 박아** 내려줌.
- 즉 같은 필드지만 "들어올 땐 안 쓰고, 나갈 땐 서버가 채운다".

### 7. 신뢰 경계 — "안 쓴다"가 아니라 "검증 후 믿는다"
- 모든 입력은 클라가 준 것. 원칙은 **경계에서 검증하고, 검증된 값은 신뢰**.
- `senderId`: 서버가 진실을 아니 **덮어쓴다**. `header.size`: 클라만 아는 값이라 대체 불가 → **[4,260] 범위 검증(OnRecv) 후 신뢰**.
- 불변식: "큐에 들어온 패킷은 이미 검증됐다". 검증은 경계(OnRecv)에서 한 번 확실히.

### 8. 닉네임 동기화 — 스냅샷 + 델타 (예정)
- id는 작은 **핸들**, 닉네임은 그 id로 **조회**하는 값(빈도: 채팅 hot / 닉네임 cold). 매 채팅에 닉네임을 싣지 않는다.
- 새 클라 접속 시 **현재 전원 명단(스냅샷)** 1회 전송 + 이후 접속/퇴장/개명은 **델타** 이벤트. 출처는 서버 `byId_` 하나.
- 이 시점에 비로소 패킷 타입(opcode) 구분이 필요(채팅/스냅샷/델타). 전부 채팅인 지금은 enum 불필요.

### 9. 고정 크기 버퍼 — 왜 `char message[256]`
- **와이어는 이미 가변**: `header.size` 바이트만 전송("hi"면 6바이트). 대역폭 낭비 없음.
- `char[256]`은 메모리상 상한: ①`memcpy`/값 복사 가능(큐에 값으로 오감, 힙 할당 0) ②DoS 상한(과대 size 차단).
- hot path(초당 수천 패킷)에서 가변(`vector`/`string`)은 패킷마다 힙 할당 → 보통 손해. 부족하면 숫자만 키운다. `ObjectPool`과 같은 철학.

### 10. length-framed 파싱 — 널이 아니라 길이로
- `char[256]`은 자기 길이 정보가 없음. 유저가 "hi"를 쳐도 2칸만 채우고 나머지 254칸은 **안 지워진 쓰레기**.
- `std::string s = pkt.message;`는 **널까지** 읽는데 payload엔 널이 없어 쓰레기를 삼킴(`hi@@@@` 버그).
- 해결: `std::string s(pkt.message, header.size - HEADER_SIZE)` — **길이를 명시**해 payload만. 경계는 언제나 `header.size`.

### 11. Broadcast vs SendTo, MakePacket 위치
- 전원=`Broadcast`(순회), 특정 1명=`SendTo`(`byId_.find`). 귓속말은 "Whisper 메서드"를 새로 만들지 않고 `SendTo`를 **상대+나 두 번** 호출해 조립.
- `MakePacket`은 **`Protocol.h`**(타입 정의 옆)에 `inline` 자유 함수. 여러 곳에서 패킷을 만들고, 포맷 지식은 한 곳에. 생성 시 길이 상한으로 검증.

### 12. RecvEvent 제거 — 경계에서 stamping (노트 6 갱신)
- `Session::OnRecv`에서 큐에 넣기 전에 `pkt.header.id = id_`로 **서버 발급 id를 패킷에 직접 박음**. 그 뒤론 `pkt.header.id`가 authoritative.
- 그래서 `{sessionId, packet}`를 따로 나르는 `RecvEvent`가 **불필요** → 제거. 큐는 `MPMCQueue<Packet>`.
- 원리: **경계에서 정규화(normalize at boundary)**. 신뢰 못 할 값(클라 id)을 입구(OnRecv)에서 서버 값으로 덮어써 소독 → 이후 단계는 clean한 값을 신뢰. stamping을 컨슈머가 아니라 OnRecv(더 이른 경계)에서 하는 게 나음.

### 13. 닉네임 = SessionManager 확장 (새 클래스 아님)
- 닉네임 매핑은 "세션 레지스트리의 두 번째 조회 키"일 뿐 → `byName_`을 SessionManager에 추가 (새 책임 아님).
- 닉네임 **값/상태**는 `Session`(`name_`, `IsNamed()`), **매핑**은 `byName_`, **처리 로직**은 `PacketHandler`.
- **인덱스 두 개의 타이밍이 다름**: `byId_`(연결)는 **접속 즉시** 등록(식별·응답에 필요), `byName_`(named)는 **/n 성공 후**.
- 수신은 IOCP+`Session*`이라 SessionManager 무관 — "안 넣으면 못 받는다"는 오해. 진짜 이유는 발신자 식별·응답(`sessionId→byId_`).
- **correctness**: `byId_`/`byName_`을 같은 락에서 동기화. 특히 `Remove`에서 `byName_`도 지워야(유령 방지).
- 이름 게이트: `named_`가 false면 `/n` 외 전부 거부(안 그러면 이름 영영 못 정함 = 락아웃).

### 14. 좁은 질의 vs Session* 반환 — UAF 회피
- 메모리 맵이 raw `Session*`를 밖으로 내보내면, 쓰는 사이 세션이 반납돼 **UAF**.
- `Find`(Session* 반환)보다 `IsNamed(id)` 같은 **좁은 bool 질의**(락 안에서 답만 내보냄)를 선호. 포인터가 안 샘.
- 여러 속성을 봐야 해지면 그때 **refcount 핸들**로 `Find` 도입 (순서).

### 15. middle-man 안티패턴 → SessionManager를 main 소유 + 주입
- SessionManager 메서드마다 NetworkCore에 전달용 facade를 만드는 건 **전달만 하는 중개자(anti-pattern)**.
- 원인: SessionManager가 NetworkCore private에 갇힘. 근데 실은 **두 계층(네트워크 Add/Remove, 컨텐츠 Broadcast/SendTo/...)이 공유하는 서비스**.
- 해결: **큐와 똑같이** main이 소유하고 NetworkCore·PacketHandler 양쪽에 주입. PacketHandler가 SessionManager를 직접 호출 → facade 삭제, NetworkCore 얇아짐.
- 의존성 방향: PacketHandler가 NetworkCore 전체가 아니라 **세션 레지스트리에만** 의존 → 결합↓.

### 16. ObjectPool을 SessionManager로 흡수 — Create/Destroy
- 풀(할당/반납)과 레지스트리(등록/조회)는 둘 다 "세션 수명" 책임 → **한 곳으로**.
- `Create` = `Alloc + SetId + byId_등록` (기존 Alloc+Add), `Destroy` = `byId_/byName_삭제 + Free` (기존 Remove+Free). 2단계 춤 제거.
- NetworkCore엔 **IOCP 바인딩/accept/PostRecv만** 남음(소켓 담당). SessionManager=세션, NetworkCore=소켓.
- **refcount의 자연스러운 집**: UAF는 "레지스트리 조회 ↔ 풀 반납" 사이 문제인데, 둘이 한 곳에 있으면 "refcount 0일 때만 Free"를 원자적으로 처리 가능. 미래 포석.

### 17. 닉네임 표시 = 스냅샷 + 델타 (구현)
- 클라가 `id → name` 맵 보유. 채팅 패킷은 **id만**(안 뜯음). 이름은 **/n 때 한 번** 따로 전달.
- 서버가 **`"NICK <id> <name>"`을 id=0(시스템) 패킷**으로 보냄: `/n` 시 전원 `Broadcast`(델타) + 접속자에게 `SendRosterTo`(스냅샷). 클라는 id==0이면 파싱해 맵 갱신, 아니면 `nameFor(id)`로 표시(없으면 숫자 폴백).
- **id=0은 서버만 찍음**(인바운드는 덮어씀) → 스푸핑 불가.
- **텍스트 파싱 vs opcode**: 지금 제어 2~3종(NICK/에러/명단)에 다 "한 줄 텍스트"라 텍스트 파싱 OK(저빈도+기존 스타일). 제어가 5~6종 넘거나 구조화 필드 생기면 **type 필드(opcode)**. 나눈다면 **유저명령(`/w`, 사람이 침 → 텍스트)이 아니라 서버→클라 제어(기계끼리 → opcode)부터**.

### 18. 세션 id vs 유저 id — 언제 유저 id가 필요한가
- 실시간 라우팅은 **라이브 레지스트리**(닉/세션→`Session*`)로 끝, 유저 id(DB PK) 불필요.
- **온라인+휘발성**(아무것도 저장 X): 유저 id/DB 아예 불필요, 세션 id 하나면 됨.
- **영속**(인벤/친구/우편이 로그아웃해도 남음): 유저 id 필요. 단 **라우팅용이 아니라** ①저장/불러오기 키 ②관계의 불변 참조(닉·세션은 변해도 유저 id는 안 변함).
- 세션 id ≠ 계정 id를 미리 나누지 말 것(YAGNI). 중복 로그인/프리로그인 관리 같은 엣지케이스가 실제로 물 때 나눔.

### 19. 친구 & DB — 영속성 진입
- 영속 친구 = DB 필요. 근데 **친구의 진짜 레슨은 동시성(2인 수정 → 락 순서)** 이라 **메모리 먼저**(락 순서 학습) → DB는 phase 2.
- 메모리 설계: `Session`에 `friends_`(친구 **id** 집합, `Session*` 금지=UAF) + `friendLock_`(per-player). 상호추가 시 **낮은 id부터 잠금**(또는 `scoped_lock`)으로 데드락 회피. `Init`에서 `clear`. `/f`는 id들을 `Find`로 그때 해석(오프라인=nullptr 스킵).
- 한계: 세션 id는 휘발 → 신원 재사용 문제. 이게 **영속 계정 id가 필요한 이유**. DB 단계에서 해결.
- DB 계획: **SQLite**(파일 하나, 서버 불필요; amalgamation 2파일). 신원 키는 **닉네임**(계정/auth 스킵 = "이름으로 로그인, 비번 없음"). 접근은 **동기 먼저**(SQLite 로컬 빠름) → 부하 크면 **DB 워커 스레드+큐**로 async. 새 클래스 **FriendRepository**(영속성=별도 책임, SessionManager엔 안 넣음). 런타임 패턴: **load-on-login → 메모리 캐시 → 변경 시 write-through**.

## 진행 로그

### 2026-08-13 — IOCP 코어 완성 & 안정화
- 동기 accept(전용 스레드) → **비동기 AcceptEx**로 전환
- IOCP 뼈대 완성: 워커 풀, `OverlappedEx` ioType 분기, AcceptEx 재사용
- 리뷰로 발견해 수정한 버그들:
  - 세션 풀 반환 누락 → `GetIndex()` + 완료 시 `Free`
  - **IO 참조 카운트(`pendingIO_`) 도입** → 다중 스레드 완료 시 UAF/double-free 방지
    (마지막 IO가 0으로 떨어질 때만 세션 반납)
  - `Stop()` 워커 종료 off-by-one → 데드락 수정
  - 패킷 헤더 크기 검증 추가 (스택 오버플로/무한 루프 차단)
  - 리슨 소켓을 IOCP에 등록 + `Start()` 초기화 순서 정정 (안 그러면 accept 0건)
  - `Init()`에서 `closing_`/`pendingIO_` 리셋 (재사용 세션 불능 방지)
- **Send 구현 + 동시성 처리**
  - `PostSend`/`OnSend`(부분 전송 재전송) + 외부 진입점 `Send()`
  - 다중 스레드가 동시에 Send → `sendLock_`(mutex)로 `sendBuffer_` + `sending_` 보호
  - lost-wakeup 방지: "버퍼 비었나 검사"와 "`sending_=false`"를 같은 락 안에서 처리

## 진행 로그

### 2026-08-19 — 컨텐츠 계층 배선 & 귓속말
- **MPMC 큐로 네트워크↔컨텐츠 분리**: `main`이 큐 소유 → `IPacketHandler`로 주입 → `QueueSink`(어댑터)가 push. 컨슈머는 별도 스레드에서 `Pop`.
- **전챗(Broadcast)**: `SessionManager::Broadcast`(shared_lock 순회). 컨슈머가 서버 발급 id를 패킷에 박아 전송.
- **귓속말(`/w`)**: `SessionManager::SendTo`(id로 `find`) 추가. 상대+나 두 번 호출. `MakePacket` 팩토리 도입.
- 잡은 버그:
  - `handler_->OnPacket` push 누락 → 패킷이 파싱만 되고 버려짐 (Session까지 핸들러 배선)
  - 브로드캐스트가 클라 선언 id를 그대로 되돌려줌 → 서버 발급 `sessionId`로 덮어쓰기
  - **length-framed 파싱 버그**: `std::string = pkt.message`가 널까지 읽어 쓰레기 삼킴(`hi@@@@`) → 길이 명시 생성자로 수정

### 2026-08-21 — 닉네임 & 이름 표시, 구조 리팩터링 논의
- **RecvEvent 제거**: `OnRecv`에서 `pkt.header.id = id_`로 경계 stamping → 큐는 `MPMCQueue<Packet>`. (노트 12)
- **닉네임(`/n`)**: `SessionManager`에 `byName_` + `SetName`(unique_lock, 중복검사) / `IsNamed`(shared_lock) / `SendRosterTo`. 이름 게이트(unnamed는 `/n`만). Session에 `name_`/`IsNamed()`/`GetName()`.
- **이름 표시(스냅샷+델타)**: 서버가 `"NICK id 이름"`을 id=0으로 Broadcast(델타)+SendRosterTo(스냅샷). **Go 클라 수정 완료**: `names map[uint16]string` + `handleControl`(id=0 파싱) + `nameFor`(폴백). (노트 17)
- 논의만 하고 아직 미적용인 리팩터링: **SessionManager를 main 소유+주입**(middle-man 제거, 노트 15) / **ObjectPool을 SessionManager로 흡수**(Create/Destroy, 노트 16).
- 다음: 친구(메모리 먼저 → DB) — SQLite, 닉네임=신원, FriendRepository. (노트 19)

## TODO / 남은 이슈

### 논의됨, 아직 미적용 (리팩터링)
- [ ] **SessionManager를 main 소유로 빼고** NetworkCore·PacketHandler에 주입 → NetworkCore facade(Broadcast/SendTo/Find...) 삭제 (노트 15)
- [ ] **ObjectPool<Session>를 SessionManager로 이동** → `Create`/`Destroy`로 통합, NetworkCore는 IOCP 바인딩만 (노트 16)
- [ ] `Remove`/`Destroy`에서 `byName_`도 삭제 (유령 방지)
- [ ] 락 잡고 Send → **"스냅샷 후 락 밖 Send"** 패턴으로 (Broadcast/SendRosterTo 공통)

### 지금 붙은 것 정리
- [x] ~~`Handle`이 `RecvEvent&`~~ → RecvEvent 제거하고 경계 stamping으로 해결
- [ ] 귓말 `getline` 앞 공백 trim / 대상 없을 때 에러 응답
- [ ] `PacketHeader.id` → `senderId`로 개명(의도 명확화, 폭은 추후; `unsigned short` 잘림 경고 C4244)

### 코어 마무리
- [ ] `Init()`에서 `sending_` 리셋 (재사용 세션 전송 불능 버그)
- [ ] `RingBuffer::Write` 반환값 누락(`return true`) — UB
- [ ] `RingBuffer::Peek`의 `void*` 포인터 산술 → `char*` 캐스트 (MSVC 컴파일 에러)
- [ ] 최대 패킷 크기 == 버퍼 크기일 때 연결 데드락 (상한을 `BUFFER_SIZE`보다 작게)
- [ ] recv 링버퍼가 linear로 꽉 차면 세션 정지(좀비) 처리

### 컨텐츠 (동시성 서사 순서)
1. [ ] `SessionManager` + **refcount 핸들**(`AddRef`/`Release`) — 이름 lookup 후 UAF 방지 (전 컨텐츠의 척추)
2. [x] 전챗 (락 안에서 스냅샷 → 락 밖에서 Send 패턴)
3. [x] 귓속말 (`/w`, id 기반)
4. [x] 닉네임 레지스트리 + **스냅샷/델타** 동기화 (`/n`, id=0 시스템 메시지)
5. [ ] **친구** (2인 수정 → 락 순서 규칙) — 메모리 먼저 → DB(SQLite) 영속화 ← **지금 여기**
6. [ ] 차단 (per-player 락)
7. [ ] 랭킹 (읽기 폭주/쓰기 소수 비대칭)
8. [ ] 거래 (2인 상태머신 + 데드락 + 중도 이탈 롤백) — 클라이맥스
9. [ ] 우편 (온라인/오프라인 경계 + 영속성)

### DB / 영속성 (친구부터)
- [ ] SQLite amalgamation(`sqlite3.c/.h`) 프로젝트에 추가
- [ ] 스키마: `friends(nickname, friend)` (+ 필요시 `accounts(nickname)`)
- [ ] `FriendRepository`(Open/Load/Add/Remove) — 영속성 별도 책임
- [ ] `/n` 시 친구목록 load → `friends_` 캐시 / `/friend` 시 메모리+DB write-through
- [ ] (나중) DB 호출 async화 — DB 워커 스레드 + 큐 (단일 컨슈머 블로킹 회피)
