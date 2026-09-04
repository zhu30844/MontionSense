package stream

import (
	"sync"
	"time"
)

type Frame []byte

type Broker struct {
	mu      sync.RWMutex
	clients map[chan Frame]struct{}
	// lastFrame is when a frame last arrived from the C daemon. The page uses
	// it to tell a live stream from a stalled one; an <img> cannot, since it
	// fires neither load nor error while a connection is open but silent.
	lastFrame time.Time
}

func NewBroker() *Broker {
	return &Broker{clients: make(map[chan Frame]struct{})}
}

func (b *Broker) Subscribe() chan Frame {
	ch := make(chan Frame, 4)
	b.mu.Lock()
	b.clients[ch] = struct{}{}
	b.mu.Unlock()
	return ch
}

func (b *Broker) Unsubscribe(ch chan Frame) {
	b.mu.Lock()
	delete(b.clients, ch)
	b.mu.Unlock()
	for {
		select {
		case <-ch:
		default:
			return
		}
	}
}

func (b *Broker) Publish(f Frame) {
	b.mu.Lock()
	b.lastFrame = time.Now()
	b.mu.Unlock()

	b.mu.RLock()
	defer b.mu.RUnlock()
	for ch := range b.clients {
		select {
		case ch <- f:
		default:
		}
	}
}

// SecondsSinceFrame reports how long ago the last frame arrived. Returns -1
// if none ever has.
func (b *Broker) SecondsSinceFrame() float64 {
	b.mu.RLock()
	defer b.mu.RUnlock()
	if b.lastFrame.IsZero() {
		return -1
	}
	return time.Since(b.lastFrame).Seconds()
}

func (b *Broker) ClientCount() int {
	b.mu.RLock()
	defer b.mu.RUnlock()
	return len(b.clients)
}
