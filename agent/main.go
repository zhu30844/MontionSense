package main

import (
	"context"
	"errors"
	"io/fs"
	"log"
	"net"
	"net/http"
	"os"
	"os/signal"
	"strings"
	"syscall"
	"time"

	"github.com/motionsense/agent/internal/database"
	"github.com/motionsense/agent/internal/server"
	"github.com/motionsense/agent/internal/stream"
)

const (
	// Port 80 when running as root, so the UI is reachable without one in the
	// URL; 3000 otherwise, since a non-root process cannot bind below 1024.
	portPrivileged   = ":80"
	portUnprivileged = ":3000"

	socketPath      = "/tmp/motionsense-stream.sock"
	dcimRoot        = "/mnt/sdcard/DCIM"
	shutdownTimeout = 5 * time.Second
)

// listenURLs renders the addresses the server can be reached on, so the log
// says where to point a browser instead of guessing at localhost.
func listenURLs(port string) string {
	addrs, err := net.InterfaceAddrs()
	if err != nil {
		return "no addresses"
	}
	var urls []string
	for _, a := range addrs {
		ipnet, ok := a.(*net.IPNet)
		if !ok || ipnet.IP.IsLoopback() || ipnet.IP.To4() == nil {
			continue
		}
		if port == ":80" {
			urls = append(urls, "http://"+ipnet.IP.String())
		} else {
			urls = append(urls, "http://"+ipnet.IP.String()+port)
		}
	}
	if len(urls) == 0 {
		return "no addresses"
	}
	return strings.Join(urls, " ")
}

func main() {
	// cancelled on SIGINT/SIGTERM
	ctx, stop := signal.NotifyContext(context.Background(), syscall.SIGINT, syscall.SIGTERM)
	defer stop()

	// static files
	sub, err := fs.Sub(frontendFiles, "static")
	if err != nil {
		log.Fatalf("embed static: %v", err)
	}

	// storage (optional at startup: the card may not be mounted yet, and on a
	// fresh one the daemon has not written the database at all). Open always
	// hands back a usable Storage that connects when the database appears, so
	// the log line is a note and not a degraded mode.
	store, err := database.Open(dcimRoot)
	if err != nil {
		log.Printf("database not ready: %v — will connect when it appears", err)
	}
	defer store.Close()

	// stream: C process -> broker -> http clients
	broker := stream.NewBroker()
	go stream.ReceiveFrames(ctx, socketPath, broker)

	// webserver
	port := portUnprivileged
	if os.Geteuid() == 0 {
		port = portPrivileged
	}

	srv := server.New(sub, port, broker, store, dcimRoot)
	go func() {
		log.Printf("listening on %s (%s)", port, listenURLs(port))
		if err := srv.ListenAndServe(); !errors.Is(err, http.ErrServerClosed) {
			log.Fatalf("http server: %v", err)
		}
	}()

	// block until signal, then shut down with a deadline
	<-ctx.Done()
	log.Println("shutting down...")
	shutdownCtx, cancel := context.WithTimeout(context.Background(), shutdownTimeout)
	defer cancel()
	if err := srv.Shutdown(shutdownCtx); err != nil {
		log.Printf("http shutdown: %v", err)
	}
	log.Println("stopped")
}
