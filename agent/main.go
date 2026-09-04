package main

import (
	"context"
	"errors"
	"io/fs"
	"log"
	"net/http"
	"os/signal"
	"syscall"
	"time"

	"github.com/motionsense/agent/internal/database"
	"github.com/motionsense/agent/internal/server"
	"github.com/motionsense/agent/internal/stream"
)

const (
	port            = ":5000"
	socketPath      = "/tmp/motionsense-stream.sock"
	dcimRoot        = "/mnt/sdcard/DCIM"
	shutdownTimeout = 5 * time.Second
)

func main() {
	// cancelled on SIGINT/SIGTERM
	ctx, stop := signal.NotifyContext(context.Background(), syscall.SIGINT, syscall.SIGTERM)
	defer stop()

	// static files
	sub, err := fs.Sub(frontendFiles, "static")
	if err != nil {
		log.Fatalf("embed static: %v", err)
	}

	// storage (optional at startup: sdcard may not be mounted yet)
	store, err := database.Open(dcimRoot)
	if err != nil {
		log.Printf("open database: %v — continuing without storage", err)
	} else {
		defer store.Close()
	}

	// stream: C process -> broker -> http clients
	broker := stream.NewBroker()
	go stream.ReceiveFrames(ctx, socketPath, broker)

	// webserver
	srv := server.New(sub, port, broker, store, dcimRoot)
	go func() {
		log.Printf("listening on http://localhost%s", port)
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
