package main

import (
	"context"
	"flag"
	"fmt"
	"log"
	"os"
	"os/signal"
	"sync"
	"sync/atomic"
	"time"
)

type config struct {
	addr      string
	count     int
	start     int
	prefix    string
	chatRate  float64
	interval  time.Duration
	ramp      time.Duration
	loginWait time.Duration
	duration  time.Duration
	statEvery time.Duration
	seed      int64
	verbose   bool
}

// stats 는 전체 봇이 공유하는 카운터다.
type stats struct {
	alive   atomic.Int64
	dialErr atomic.Int64
	sendErr atomic.Int64

	sent      atomic.Int64
	sentLogin atomic.Int64
	sentChat  atomic.Int64
	sentCmd   atomic.Int64

	friendAdd    atomic.Int64
	friendList   atomic.Int64
	friendChat   atomic.Int64
	friendAccept atomic.Int64
	friendReject atomic.Int64

	recv       atomic.Int64
	recvChat   atomic.Int64
	recvNick   atomic.Int64
	recvSystem atomic.Int64
}

func main() {
	cfg := &config{}

	flag.StringVar(&cfg.addr, "addr", "127.0.0.1:5050", "서버 주소")
	flag.IntVar(&cfg.count, "n", 10, "봇 수")
	flag.IntVar(&cfg.start, "start", 1, "이름 시작 번호 (user001 의 1)")
	flag.StringVar(&cfg.prefix, "prefix", "user", "이름 접두사")
	flag.Float64Var(&cfg.chatRate, "chat-rate", 0.9, "전체 채팅 확률 (나머지는 명령어)")
	flag.DurationVar(&cfg.interval, "interval", time.Second, "봇 한 마리의 평균 행동 간격 (0.5x~1.5x 지터)")
	flag.DurationVar(&cfg.ramp, "ramp", 20*time.Millisecond, "봇 접속 간격")
	flag.DurationVar(&cfg.loginWait, "login-wait", 500*time.Millisecond, "로그인 후 첫 행동까지 대기")
	flag.DurationVar(&cfg.duration, "duration", 0, "실행 시간 (0 이면 Ctrl+C 까지)")
	flag.DurationVar(&cfg.statEvery, "stat", 5*time.Second, "통계 출력 주기 (0 이면 끔)")
	flag.Int64Var(&cfg.seed, "seed", 0, "난수 시드 (0 이면 현재 시각)")
	flag.BoolVar(&cfg.verbose, "v", false, "주고받는 메시지 전부 출력")
	flag.Parse()

	if err := validate(cfg); err != nil {
		fmt.Fprintf(os.Stderr, "[Error] %v\n", err)
		os.Exit(1)
	}

	if cfg.seed == 0 {
		cfg.seed = time.Now().UnixNano()
	}

	log.SetFlags(log.Ltime)

	names := make([]string, 0, cfg.count)
	for i := 0; i < cfg.count; i++ {
		names = append(names, fmt.Sprintf("%s%03d", cfg.prefix, cfg.start+i))
	}

	ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt)
	defer stop()

	if cfg.duration > 0 {
		var cancel context.CancelFunc
		ctx, cancel = context.WithTimeout(ctx, cfg.duration)
		defer cancel()
	}

	st := &stats{}

	log.Printf("[Bot] %s 에 %d 마리 접속 (%s ~ %s), 채팅 %.0f%% / 명령 %.0f%%, 간격 %v, seed=%d",
		cfg.addr, cfg.count, names[0], names[len(names)-1],
		cfg.chatRate*100, (1-cfg.chatRate)*100, cfg.interval, cfg.seed)

	start := time.Now()

	var wg sync.WaitGroup
	for i, name := range names {
		if !sleepCtx(ctx, cfg.ramp) {
			break
		}

		wg.Add(1)
		go func(name string, i int) {
			defer wg.Done()
			newBot(name, names, cfg, st, cfg.seed+int64(i)*7919).run(ctx)
		}(name, i)
	}

	stopStats := startStats(ctx, cfg, st, start)

	wg.Wait()
	stopStats()

	printSummary(st, time.Since(start))
}

func validate(cfg *config) error {
	if cfg.count <= 0 {
		return fmt.Errorf("-n 은 1 이상이어야 합니다")
	}
	if cfg.chatRate < 0 || cfg.chatRate > 1 {
		return fmt.Errorf("-chat-rate 는 0~1 사이여야 합니다")
	}

	// 서버 Consumer::HandleNick 은 3자 이상 10자 미만만 받는다.
	last := fmt.Sprintf("%s%03d", cfg.prefix, cfg.start+cfg.count-1)
	if len(last) < 3 || len(last) >= 10 {
		return fmt.Errorf("이름 %q 의 길이(%d)가 서버 제한(3~9자)을 벗어납니다", last, len(last))
	}
	return nil
}

// startStats 는 주기적으로 통계를 찍고, 멈추는 함수를 돌려준다.
func startStats(ctx context.Context, cfg *config, st *stats, start time.Time) func() {
	if cfg.statEvery <= 0 {
		return func() {}
	}

	done := make(chan struct{})
	go func() {
		t := time.NewTicker(cfg.statEvery)
		defer t.Stop()

		for {
			select {
			case <-ctx.Done():
				return
			case <-done:
				return
			case <-t.C:
				log.Printf("[stat] t=%-6s 접속=%d  송신=%d (채팅 %d / 명령 %d)  수신=%d (채팅 %d / 시스템 %d / NICK %d)  에러=%d",
					time.Since(start).Truncate(time.Second),
					st.alive.Load(),
					st.sent.Load(), st.sentChat.Load(), st.sentCmd.Load(),
					st.recv.Load(), st.recvChat.Load(), st.recvSystem.Load(), st.recvNick.Load(),
					st.dialErr.Load()+st.sendErr.Load())
			}
		}
	}()

	return func() { close(done) }
}

func printSummary(st *stats, elapsed time.Duration) {
	sec := elapsed.Seconds()
	if sec <= 0 {
		sec = 1
	}

	log.Printf("[Bot] 종료 (%v)", elapsed.Truncate(time.Millisecond))
	log.Printf("  송신   : %d  (%.1f/s)  로그인 %d / 채팅 %d / 명령 %d",
		st.sent.Load(), float64(st.sent.Load())/sec,
		st.sentLogin.Load(), st.sentChat.Load(), st.sentCmd.Load())
	log.Printf("  친구   : add %d / list %d / chat %d / accept %d / reject %d",
		st.friendAdd.Load(), st.friendList.Load(), st.friendChat.Load(),
		st.friendAccept.Load(), st.friendReject.Load())
	log.Printf("  수신   : %d  (%.1f/s)  채팅 %d / 시스템 %d / NICK %d",
		st.recv.Load(), float64(st.recv.Load())/sec,
		st.recvChat.Load(), st.recvSystem.Load(), st.recvNick.Load())
	log.Printf("  에러   : 접속 %d / 전송 %d", st.dialErr.Load(), st.sendErr.Load())
}
