package main

import (
	"encoding/binary"
	"fmt"
	"io"
	"net"
	"strconv"
	"strings"
	"sync"

	"github.com/gdamore/tcell/v2"
)

const HeaderSize = 4

type Command struct {
	Name        string
	Description string
}

var commands = []Command{
	{"/?", "도 움 말"},
	{"/w", "귓 속 말"},
	{"/f", "친 구"},
	{"/i", "인 벤 토 리"},
}

// message 는 한 줄과 그 줄의 표시 스타일을 함께 담는다.
type message struct {
	text  string
	style tcell.Style
}

var (
	styleDefault = tcell.StyleDefault
	styleFriend  = tcell.StyleDefault.Foreground(tcell.ColorOrange) // 친구 관련
	styleItem    = tcell.StyleDefault.Foreground(tcell.ColorGreen)  // 인벤토리/거래 관련
)

// classify 는 메시지 내용의 키워드로 색 스타일을 고른다.
func classify(text string) tcell.Style {
	lower := strings.ToLower(text)

	for _, kw := range []string{"friend", "request", "blocked", "reject", "accept", "block"} {
		if strings.Contains(lower, kw) {
			return styleFriend
		}
	}
	for _, kw := range []string{"gold", "sword", "power", "enhanc", "inventory", "trade"} {
		if strings.Contains(lower, kw) {
			return styleItem
		}
	}
	return styleDefault
}

type ChatUI struct {
	screen tcell.Screen

	mu       sync.Mutex
	messages []message
	names    map[uint16]string // id -> 닉네임 (서버 스냅샷/델타로 채워짐)

	input []rune

	showCommandPopup bool
	selectedCommand  int
}

func main() {
	conn, err := net.Dial("tcp", "127.0.0.1:5050")
	if err != nil {
		panic(err)
	}
	defer conn.Close()

	screen, err := tcell.NewScreen()
	if err != nil {
		panic(err)
	}

	if err := screen.Init(); err != nil {
		panic(err)
	}
	defer screen.Fini()

	ui := &ChatUI{
		screen: screen,
		names:  make(map[uint16]string),
	}

	// 서버 수신
	go recvLoop(conn, ui)

	// UI 이벤트 루프
	ui.run(conn)
}

// ============================================================
// Network
// ============================================================

func recvLoop(conn net.Conn, ui *ChatUI) {
	for {
		// Header
		header := make([]byte, HeaderSize)

		_, err := io.ReadFull(conn, header)
		if err != nil {
			ui.addMessage("[SYSTEM] 서버와 연결이 끊어졌습니다.")
			return
		}

		size := binary.LittleEndian.Uint16(header[0:2])
		id := binary.LittleEndian.Uint16(header[2:4])

		if size < HeaderSize {
			ui.addMessage("[SYSTEM] 잘못된 패킷입니다.")
			return
		}

		// Payload
		payloadSize := int(size) - HeaderSize

		payload := make([]byte, payloadSize)

		_, err = io.ReadFull(conn, payload)
		if err != nil {
			ui.addMessage("[SYSTEM] 패킷 수신에 실패했습니다.")
			return
		}

		// id == 0 은 서버가 보내는 제어 메시지 (닉네임 알림/명단/에러 등)
		if id == 0 {
			ui.handleControl(string(payload))
			continue
		}

		// 일반 채팅: id 로 닉네임을 찾아서 표시 (없으면 숫자 폴백)
		name := ui.nameFor(id)
		ui.addMessage(fmt.Sprintf("[%s] %s", name, string(payload)))
	}
}

// handleControl 은 id==0 시스템 메시지를 해석한다.
//
//	"NICK <id> <name>" -> id->name 맵 갱신 (스냅샷/델타 공용)
//	그 외              -> 시스템 메시지로 출력
func (ui *ChatUI) handleControl(payload string) {
	parts := strings.SplitN(payload, " ", 3)

	if len(parts) == 3 && parts[0] == "NICK" {
		id, err := strconv.ParseUint(parts[1], 10, 16)
		if err == nil {
			ui.mu.Lock()
			ui.names[uint16(id)] = parts[2]
			ui.mu.Unlock()
		}
		return
	}

	ui.addMessage("[SYSTEM] " + payload)
}

// nameFor 는 id 에 매핑된 닉네임을 반환한다. 아직 모르면 숫자로 폴백.
func (ui *ChatUI) nameFor(id uint16) string {
	ui.mu.Lock()
	defer ui.mu.Unlock()

	if n, ok := ui.names[id]; ok {
		return n
	}
	return fmt.Sprintf("%d", id)
}

func sendPacket(conn net.Conn, message string) {
	data := []byte(message)

	packet := make([]byte, HeaderSize+len(data))

	// Packet size
	binary.LittleEndian.PutUint16(
		packet[0:2],
		uint16(len(packet)),
	)

	// Packet ID
	binary.LittleEndian.PutUint16(
		packet[2:4],
		0,
	)

	// Message
	copy(packet[HeaderSize:], data)

	_, err := conn.Write(packet)
	if err != nil {
		fmt.Println("send error:", err)
	}
}

// ============================================================
// UI
// ============================================================

func (ui *ChatUI) run(conn net.Conn) {
	ui.draw()

	for {
		event := ui.screen.PollEvent()

		switch ev := event.(type) {

		case *tcell.EventKey:

			switch ev.Key() {

			case tcell.KeyEscape:
				return

			case tcell.KeyEnter:
				ui.handleEnter(conn)

			case tcell.KeyUp:
				ui.handleUp()

			case tcell.KeyDown:
				ui.handleDown()

			case tcell.KeyBackspace,
				tcell.KeyBackspace2:

				if len(ui.input) > 0 {
					ui.input = ui.input[:len(ui.input)-1]
					ui.updateCommandPopup()
					ui.draw()
				}

			case tcell.KeyRune:
				ui.input = append(ui.input, ev.Rune())

				ui.updateCommandPopup()
				ui.draw()
			}
		}
	}
}

// ============================================================
// Input
// ============================================================

func (ui *ChatUI) handleEnter(conn net.Conn) {
	if len(ui.input) == 0 {
		return
	}

	// 명령어 팝업이 열려있다면
	// 선택된 명령어를 입력창에 넣는다.
	if ui.showCommandPopup {
		filtered := ui.getFilteredCommands()

		if len(filtered) > 0 {
			ui.input = []rune(filtered[ui.selectedCommand].Name)

			ui.showCommandPopup = false
			ui.selectedCommand = 0

			ui.draw()
			return
		}
	}

	message := string(ui.input)

	// 일반 메시지 전송
	sendPacket(conn, message)

	// 입력창 초기화
	ui.input = ui.input[:0]

	ui.showCommandPopup = false
	ui.selectedCommand = 0

	ui.draw()
}

func (ui *ChatUI) handleUp() {
	if !ui.showCommandPopup {
		return
	}

	filtered := ui.getFilteredCommands()

	if len(filtered) == 0 {
		return
	}

	ui.selectedCommand--

	if ui.selectedCommand < 0 {
		ui.selectedCommand = len(filtered) - 1
	}

	ui.draw()
}

func (ui *ChatUI) handleDown() {
	if !ui.showCommandPopup {
		return
	}

	filtered := ui.getFilteredCommands()

	if len(filtered) == 0 {
		return
	}

	ui.selectedCommand++

	if ui.selectedCommand >= len(filtered) {
		ui.selectedCommand = 0
	}

	ui.draw()
}

// ============================================================
// Command
// ============================================================

func (ui *ChatUI) updateCommandPopup() {
	input := string(ui.input)

	// "/"로 시작하지 않으면 팝업 닫기
	if !strings.HasPrefix(input, "/") {
		ui.showCommandPopup = false
		ui.selectedCommand = 0
		return
	}

	filtered := ui.getFilteredCommands()

	if len(filtered) == 0 {
		ui.showCommandPopup = false
		ui.selectedCommand = 0
		return
	}

	ui.showCommandPopup = true

	// 선택 범위를 벗어나지 않도록
	if ui.selectedCommand >= len(filtered) {
		ui.selectedCommand = 0
	}
}

func (ui *ChatUI) getFilteredCommands() []Command {
	input := string(ui.input)

	var result []Command

	for _, command := range commands {
		if strings.HasPrefix(command.Name, input) {
			result = append(result, command)
		}
	}

	return result
}

// ============================================================
// Message
// ============================================================

func (ui *ChatUI) addMessage(raw string) {
	ui.mu.Lock()
	raw = strings.TrimRight(raw, "\n")
	for _, line := range strings.Split(raw, "\n") {
		line = strings.TrimRight(line, "\r")
		ui.messages = append(ui.messages, message{text: line, style: classify(line)})
	}
	ui.mu.Unlock()

	ui.draw()
}

// ============================================================
// Rendering
// ============================================================

func (ui *ChatUI) draw() {
	ui.mu.Lock()
	defer ui.mu.Unlock()

	ui.screen.Clear()

	width, height := ui.screen.Size()

	// --------------------------------------------------------
	// Input 영역
	// --------------------------------------------------------

	inputY := height - 2

	// 구분선
	for x := 0; x < width; x++ {
		ui.screen.SetContent(
			x,
			inputY,
			tcell.RuneHLine,
			nil,
			tcell.StyleDefault,
		)
	}

	// --------------------------------------------------------
	// Chat messages
	// --------------------------------------------------------

	maxMessages := inputY

	startIndex := 0

	if len(ui.messages) > maxMessages {
		startIndex = len(ui.messages) - maxMessages
	}

	y := 0

	for i := startIndex; i < len(ui.messages); i++ {
		if y >= inputY {
			break
		}

		drawStringWithStyle(
			ui.screen,
			0,
			y,
			ui.messages[i].text,
			ui.messages[i].style,
		)

		y++
	}

	// --------------------------------------------------------
	// Command popup
	// --------------------------------------------------------

	if ui.showCommandPopup {
		ui.drawCommandPopup(inputY)
	}

	// --------------------------------------------------------
	// Input
	// --------------------------------------------------------

	drawString(
		ui.screen,
		0,
		inputY+1,
		">> "+string(ui.input),
	)

	// Cursor
	cursorX := 3 + len(ui.input)

	ui.screen.ShowCursor(
		cursorX,
		inputY+1,
	)

	ui.screen.Show()
}

func (ui *ChatUI) drawCommandPopup(inputY int) {
	filtered := ui.getFilteredCommands()

	if len(filtered) == 0 {
		return
	}

	// 팝업의 시작 위치
	startY := inputY - len(filtered)

	if startY < 0 {
		startY = 0
	}

	for i, command := range filtered {
		y := startY + i

		selected := i == ui.selectedCommand

		// 팝업 배경
		for x := 0; x < 30; x++ {
			style := tcell.StyleDefault

			if selected {
				style = style.Reverse(true)
			}

			ui.screen.SetContent(
				x,
				y,
				' ',
				nil,
				style,
			)
		}

		// 명령어
		text := fmt.Sprintf(
			" %-5s %s",
			command.Name,
			command.Description,
		)

		style := tcell.StyleDefault

		if selected {
			style = style.Reverse(true)
		}

		drawStringWithStyle(
			ui.screen,
			0,
			y,
			text,
			style,
		)
	}
}

// ============================================================
// Drawing Helpers
// ============================================================

func drawString(
	screen tcell.Screen,
	x int,
	y int,
	text string,
) {
	drawStringWithStyle(
		screen,
		x,
		y,
		text,
		tcell.StyleDefault,
	)
}

func drawStringWithStyle(
	screen tcell.Screen,
	x int,
	y int,
	text string,
	style tcell.Style,
) {
	for _, r := range text {
		screen.SetContent(
			x,
			y,
			r,
			nil,
			style,
		)

		x++
	}
}
