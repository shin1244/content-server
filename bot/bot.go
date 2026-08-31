package main

import (
	"bufio"
	"context"
	"log"
	"math/rand"
	"net"
	"strings"
	"sync"
	"time"
)

// bot 은 서버에 붙은 가짜 유저 하나를 나타낸다.
//
// 동작 방식
//   - 접속 직후 "/n <name>" 으로 로그인한다.
//   - 이후 tick 마다 chatRate 확률로 전체 채팅, 나머지 확률로 친구 명령을 쓴다.
//   - 수신 루프는 별도 고루틴에서 돌며 "/f list" 응답을 파싱해 pending 을 채운다.
//     accept / reject 는 pending 이 채워진 뒤에만(= 리스트를 본 뒤에만) 선택지에 오른다.
type bot struct {
	name string
	pool []string // 친구 추가 대상 후보 (전체 봇 이름)
	cfg  *config
	st   *stats
	rng  *rand.Rand

	conn net.Conn

	mu       sync.Mutex
	pending  []string // 나에게 친구 요청을 보낸 유저 (Request 섹션)
	friends  []string // 이미 수락된 친구
	listSeen bool     // "/f list" 응답을 한 번이라도 받았는지
}

func newBot(name string, pool []string, cfg *config, st *stats, seed int64) *bot {
	return &bot{
		name: name,
		pool: pool,
		cfg:  cfg,
		st:   st,
		rng:  rand.New(rand.NewSource(seed)),
	}
}

// run 은 접속부터 종료까지 봇 한 마리의 수명 전체를 담당한다.
func (b *bot) run(ctx context.Context) {
	conn, err := net.DialTimeout("tcp", b.cfg.addr, 5*time.Second)
	if err != nil {
		b.st.dialErr.Add(1)
		b.logf("접속 실패: %v", err)
		return
	}
	b.conn = conn
	defer conn.Close()

	b.st.alive.Add(1)
	defer b.st.alive.Add(-1)

	// 수신 루프. 서버가 끊으면 recvDone 이 닫힌다.
	recvDone := make(chan struct{})
	go b.recvLoop(recvDone)

	b.st.sentLogin.Add(1)
	if !b.send("/n " + b.name) {
		return
	}

	// 서버가 /n 을 처리(DB 로그인)할 시간을 잠깐 준다.
	if !sleepCtx(ctx, b.cfg.loginWait) {
		return
	}

	for {
		select {
		case <-ctx.Done():
			return
		case <-recvDone:
			b.logf("서버와 연결이 끊어졌습니다")
			return
		case <-time.After(b.nextInterval()):
		}

		if !b.act() {
			return
		}
	}
}

// nextInterval 은 평균 cfg.interval 주변에서 흔들리는 대기 시간을 만든다.
// 모든 봇이 같은 박자로 때리지 않도록 0.5x ~ 1.5x 지터를 준다.
func (b *bot) nextInterval() time.Duration {
	base := float64(b.cfg.interval)
	return time.Duration(base * (0.5 + b.rng.Float64()))
}

// act 는 이번 tick 의 행동 하나를 고르고 전송한다. 전송 실패면 false.
func (b *bot) act() bool {
	if b.rng.Float64() < b.cfg.chatRate {
		b.st.sentChat.Add(1)
		return b.send(b.randomChat())
	}

	b.st.sentCmd.Add(1)
	return b.friendAction()
}

// friendAction 은 친구 명령 하나를 골라 보낸다.
//
//	add    : 무작위 유저에게 친구 요청
//	list   : 친구 목록 확인 (accept/reject 를 열어 주는 유일한 통로)
//	chat   : 친구 전체에게 채팅
//	accept : 나에게 요청을 건 유저 수락  (list 를 본 뒤에만)
//	reject : 나에게 요청을 건 유저 거절  (list 를 본 뒤에만)
func (b *bot) friendAction() bool {
	choices := []weighted{
		{"add", 3},
		{"list", 3},
		{"chat", 2},
	}

	if b.hasPending() {
		choices = append(choices, weighted{"accept", 3}, weighted{"reject", 1})
	}

	switch pickWeighted(b.rng, choices) {
	case "add":
		target := b.randomOther()
		if target == "" {
			return true
		}
		b.st.friendAdd.Add(1)
		return b.send("/f add " + target)

	case "list":
		b.st.friendList.Add(1)
		return b.send("/f list")

	case "chat":
		b.st.friendChat.Add(1)
		return b.send("/f " + b.randomChat())

	case "accept":
		name := b.takePending()
		if name == "" {
			return true
		}
		b.st.friendAccept.Add(1)
		return b.send("/f accept " + name)

	case "reject":
		name := b.takePending()
		if name == "" {
			return true
		}
		b.st.friendReject.Add(1)
		return b.send("/f reject " + name)
	}

	return true
}

func (b *bot) send(payload string) bool {
	if err := writePacket(b.conn, payload); err != nil {
		b.st.sendErr.Add(1)
		b.logf("전송 실패: %v", err)
		return false
	}

	b.st.sent.Add(1)
	if b.cfg.verbose {
		log.Printf("[%s] >> %s", b.name, payload)
	}
	return true
}

// randomChat 은 단어 풀에서 무작위 개수를 뽑아 이어 붙인다.
func (b *bot) randomChat() string {
	n := minChatWords + b.rng.Intn(maxChatWords-minChatWords+1)

	var sb strings.Builder
	for i := 0; i < n; i++ {
		if i > 0 {
			sb.WriteByte(' ')
		}
		sb.WriteString(chatWords[b.rng.Intn(len(chatWords))])
	}
	return sb.String()
}

// randomOther 는 자기 자신을 제외한 무작위 유저 이름을 고른다.
func (b *bot) randomOther() string {
	if len(b.pool) < 2 {
		return ""
	}
	for {
		name := b.pool[b.rng.Intn(len(b.pool))]
		if name != b.name {
			return name
		}
	}
}

// ============================================================
// 수신
// ============================================================

func (b *bot) recvLoop(done chan<- struct{}) {
	defer close(done)

	r := bufio.NewReaderSize(b.conn, 64*1024)

	for {
		id, payload, err := readPacket(r)
		if err != nil {
			return
		}

		b.st.recv.Add(1)

		// id != 0 은 다른 유저의 채팅.
		if id != 0 {
			b.st.recvChat.Add(1)
			continue
		}

		b.onControl(payload)
	}
}

// onControl 은 id==0 제어 메시지를 해석한다.
func (b *bot) onControl(payload string) {
	switch {
	case strings.HasPrefix(payload, "NICK "):
		b.st.recvNick.Add(1)

	case strings.HasPrefix(payload, "Blocked"):
		b.st.recvSystem.Add(1)
		b.applyFriendList(payload)

	default:
		b.st.recvSystem.Add(1)
		if b.cfg.verbose {
			log.Printf("[%s] << %s", b.name, strings.ReplaceAll(payload, "\n", " | "))
		}
	}
}

// applyFriendList 는 "/f list" 응답을 파싱해 pending/friends 를 갱신한다.
func (b *bot) applyFriendList(payload string) {
	pending, friends := parseFriendList(payload)

	b.mu.Lock()
	b.pending = pending
	b.friends = friends
	b.listSeen = true
	b.mu.Unlock()

	if b.cfg.verbose {
		log.Printf("[%s] << list: 요청 %d명, 친구 %d명", b.name, len(pending), len(friends))
	}
}

// parseFriendList 는 Consumer::HandleFriendList 가 만든 텍스트를 읽는다.
//
//	Blocked
//	<이름들>
//	----------------
//	Request          <- 나에게 친구 요청을 건 유저
//	<이름들>
//	----------------
//	Friends
//	<이름들>
func parseFriendList(payload string) (pending, friends []string) {
	lines := strings.Split(payload, "\n")

	// 서버 패킷은 256바이트에서 잘리므로, 꽉 찬 응답의 마지막 줄은 신뢰하지 않는다.
	if len(payload) >= maxPayloadSize && len(lines) > 0 {
		lines = lines[:len(lines)-1]
	}

	section := ""
	for _, line := range lines {
		line = strings.TrimSpace(line)

		switch {
		case line == "":
			continue
		case strings.HasPrefix(line, "---"):
			continue
		case line == "Blocked":
			section = "blocked"
			continue
		case line == "Request":
			section = "pending"
			continue
		case line == "Friends":
			section = "friends"
			continue
		}

		switch section {
		case "pending":
			pending = append(pending, line)
		case "friends":
			friends = append(friends, line)
		}
	}
	return pending, friends
}

func (b *bot) hasPending() bool {
	b.mu.Lock()
	defer b.mu.Unlock()
	return b.listSeen && len(b.pending) > 0
}

// takePending 은 대기 중인 요청 하나를 꺼낸다(꺼낸 건 목록에서 지운다).
// 다음 "/f list" 때 다시 채워진다.
func (b *bot) takePending() string {
	b.mu.Lock()
	defer b.mu.Unlock()

	if len(b.pending) == 0 {
		return ""
	}

	i := b.rng.Intn(len(b.pending))
	name := b.pending[i]
	b.pending = append(b.pending[:i], b.pending[i+1:]...)
	return name
}

func (b *bot) logf(format string, args ...any) {
	if !b.cfg.verbose {
		return
	}
	log.Printf("[%s] "+format, append([]any{b.name}, args...)...)
}

// ============================================================
// 가중치 선택
// ============================================================

type weighted struct {
	name   string
	weight int
}

func pickWeighted(rng *rand.Rand, items []weighted) string {
	total := 0
	for _, it := range items {
		total += it.weight
	}
	if total <= 0 {
		return ""
	}

	n := rng.Intn(total)
	for _, it := range items {
		n -= it.weight
		if n < 0 {
			return it.name
		}
	}
	return items[len(items)-1].name
}

// sleepCtx 는 d 만큼 기다린다. 도중에 ctx 가 끝나면 false.
func sleepCtx(ctx context.Context, d time.Duration) bool {
	if d <= 0 {
		return ctx.Err() == nil
	}

	t := time.NewTimer(d)
	defer t.Stop()

	select {
	case <-ctx.Done():
		return false
	case <-t.C:
		return true
	}
}
