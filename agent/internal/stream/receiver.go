package stream

import (
	"bufio"
	"context"
	"encoding/binary"
	"io"
	"log"
	"net"
	"time"
)

func ReceiveFrames(ctx context.Context, socketPath string, broker *Broker) {
	var dialer net.Dialer
	for {
		conn, err := dialer.DialContext(ctx, "unix", socketPath)
		if err != nil {
			if ctx.Err() != nil {
				return
			}
			log.Printf("[stream] connect %s: %v — retrying in 1s", socketPath, err)
			select {
			case <-ctx.Done():
				return
			case <-time.After(time.Second):
			}
			continue
		}
		log.Printf("[stream] connected to %s\n", socketPath)
		readFrames(ctx, conn, broker)
		if ctx.Err() != nil {
			return
		}
		log.Printf("[stream] C process disconnected — reconnecting")
	}
}

func readFrames(ctx context.Context, conn net.Conn, broker *Broker) {
	defer conn.Close()
	// unblock io.ReadFull when ctx is cancelled by closing the conn
	done := make(chan struct{})
	defer close(done)
	go func() {
		select {
		case <-ctx.Done():
			conn.Close()
		case <-done:
		}
	}()

	r := bufio.NewReaderSize(conn, 256*1024)
	var sizeBuf [4]byte
	for {
		if _, err := io.ReadFull(r, sizeBuf[:]); err != nil {
			return
		}
		size := binary.BigEndian.Uint32(sizeBuf[:])
		if size == 0 || size > 8<<20 {
			log.Printf("[stream] frame size error, size: (%d)", size)
			return
		}
		frame := make([]byte, size)
		if _, err := io.ReadFull(r, frame); err != nil {
			log.Printf("[stream] read frame error: %s", err)
			return
		}
		broker.Publish(frame)
	}
}
