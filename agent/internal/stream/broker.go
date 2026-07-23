package stream

import (
	"sync"
)

type Frame []byte

type Broker struct {
	mu      sync.RWMutex
	clients map[chan Frame]struct{}
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
	b.mu.RLock()
	defer b.mu.RUnlock()
	for ch := range b.clients {
		select {
		case ch <- f:
		default:
		}
	}
}

func (b *Broker) ClientCount() int {
	b.mu.RLock()
	defer b.mu.RUnlock()
	return len(b.clients)
}
