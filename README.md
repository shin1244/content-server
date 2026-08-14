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
| `NetworkCore` | IOCP 생성, AcceptEx, 워커 스레드 루프, 세션 배정 |
| `Session` | 세션별 recv/send, 링버퍼, 패킷 파싱, IO 참조 카운트 |
| `RingBuffer` | 송수신용 링버퍼 |
| `ObjectPool` | 세션 고정 풀(최대 1000) |
| `NetTypes` | OverlappedEx / Accept·Recv·Send 컨텍스트 |
| `Protocol` | 패킷 헤더 정의 |
| `MPMCQueue` | (제작 중) 멀티 프로듀서/컨슈머 큐 |

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

## TODO / 남은 이슈

### 코어 마무리 (컨텐츠 전에)
- [ ] `Init()`에서 `sending_` 리셋 (재사용 세션 전송 불능 버그)
- [ ] `main.cpp`를 `NetworkCore` 배선으로 교체 (현재 빈 main → 실행 안 됨)
- [ ] `RingBuffer::Write` 반환값 누락(`return true`) — UB
- [ ] `RingBuffer::Peek`의 `void*` 포인터 산술 → `char*` 캐스트 (MSVC 컴파일 에러)
- [ ] 최대 패킷 크기 == 버퍼 크기일 때 연결 데드락 (상한을 `BUFFER_SIZE`보다 작게)
- [ ] recv 링버퍼가 linear로 꽉 차면 세션 정지(좀비) 처리

### 컨텐츠 (동시성 서사 순서)
1. [ ] `SessionManager` + **refcount 핸들**(`AddRef`/`Release`) — 이름 lookup 후 UAF 방지 (전 컨텐츠의 척추)
2. [ ] 전챗 (락 안에서 스냅샷 → 락 밖에서 Send 패턴)
3. [ ] 귓속말 + 이름 레지스트리
4. [ ] 차단 (per-player 락)
5. [ ] 친구 (2인 수정 → 락 순서 규칙)
6. [ ] 랭킹 (읽기 폭주/쓰기 소수 비대칭)
7. [ ] 거래 (2인 상태머신 + 데드락 + 중도 이탈 롤백) — 클라이맥스
8. [ ] 우편 (온라인/오프라인 경계 + 영속성)
