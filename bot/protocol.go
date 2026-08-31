package main

import (
	"encoding/binary"
	"fmt"
	"io"
	"unicode/utf8"
)

// 서버 Protocol.h 와 동일한 규격.
//
//	struct PacketHeader { unsigned short size; unsigned short id; };
//	struct Packet       { PacketHeader header; char message[256]; };
//
// size 는 헤더(4바이트)를 포함한 전체 길이이며 리틀엔디언이다.
const (
	headerSize     = 4
	maxPayloadSize = 256
	maxPacketSize  = headerSize + maxPayloadSize
)

// writePacket 은 payload 를 패킷으로 감싸 전송한다.
// id 는 0 으로 보낸다. 서버가 Session::OnRecv 에서 세션 id 로 덮어쓴다.
func writePacket(w io.Writer, payload string) error {
	body := truncateUTF8([]byte(payload), maxPayloadSize)

	packet := make([]byte, headerSize+len(body))
	binary.LittleEndian.PutUint16(packet[0:2], uint16(len(packet)))
	binary.LittleEndian.PutUint16(packet[2:4], 0)
	copy(packet[headerSize:], body)

	_, err := w.Write(packet)
	return err
}

// readPacket 은 패킷 하나를 읽어 (id, payload) 로 돌려준다.
func readPacket(r io.Reader) (uint16, string, error) {
	var header [headerSize]byte
	if _, err := io.ReadFull(r, header[:]); err != nil {
		return 0, "", err
	}

	size := binary.LittleEndian.Uint16(header[0:2])
	id := binary.LittleEndian.Uint16(header[2:4])

	if size < headerSize || int(size) > maxPacketSize {
		return 0, "", fmt.Errorf("잘못된 패킷 크기: %d", size)
	}

	payload := make([]byte, int(size)-headerSize)
	if _, err := io.ReadFull(r, payload); err != nil {
		return 0, "", err
	}

	return id, string(payload), nil
}

// truncateUTF8 은 limit 바이트를 넘지 않도록 자르되 UTF-8 경계를 깨지 않는다.
func truncateUTF8(b []byte, limit int) []byte {
	if len(b) <= limit {
		return b
	}
	b = b[:limit]
	for len(b) > 0 && !utf8.Valid(b) {
		b = b[:len(b)-1]
	}
	return b
}
