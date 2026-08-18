package main

import (
	"encoding/binary"
	"fmt"
	"io"
	"net"
)

const HeaderSize = 4

func main() {
	conn, err := net.Dial("tcp", "127.0.0.1:5050")
	if err != nil {
		panic(err)
	}
	defer conn.Close()

	// 수신 goroutine
	go recvLoop(conn)

	// 송신
	for {
		var message string

		fmt.Print(">> ")
		fmt.Scanln(&message)

		data := []byte(message)

		packet := make([]byte, HeaderSize+len(data))

		// size
		binary.LittleEndian.PutUint16(
			packet[0:2],
			uint16(len(packet)),
		)

		// packet id
		binary.LittleEndian.PutUint16(
			packet[2:4],
			1,
		)

		// message
		copy(packet[HeaderSize:], data)

		_, err := conn.Write(packet)
		if err != nil {
			fmt.Println("send error:", err)
			return
		}
	}
}

func recvLoop(conn net.Conn) {
	for {
		// Header
		header := make([]byte, HeaderSize)

		_, err := io.ReadFull(conn, header)
		if err != nil {
			fmt.Println("recv error:", err)
			return
		}

		size := binary.LittleEndian.Uint16(header[0:2])
		id := binary.LittleEndian.Uint16(header[2:4])

		if size < HeaderSize {
			fmt.Println("invalid packet size:", size)
			return
		}

		// Payload
		payloadSize := int(size) - HeaderSize
		payload := make([]byte, payloadSize)

		_, err = io.ReadFull(conn, payload)
		if err != nil {
			fmt.Println("recv payload error:", err)
			return
		}

		fmt.Printf("[RECV] id=%d message=%s\n", id, string(payload))
	}
}
