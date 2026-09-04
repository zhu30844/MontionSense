package server

import (
	"io/fs"
	"net/http"
	"time"

	"github.com/motionsense/agent/internal/database"
	"github.com/motionsense/agent/internal/stream"
)

const mjpegBoundary = "frame"

// New builds the http.Server; the caller owns its lifecycle (ListenAndServe / Shutdown).
func New(subStaticPath fs.FS, addr string, broker *stream.Broker, storage *database.Storage, dcimRoot string) *http.Server {
	// for APP start time
	startTime := time.Now()

	mux := http.NewServeMux()
	// homepage
	mux.Handle("/", http.FileServer(http.FS(subStaticPath)))
	// stream
	mux.HandleFunc("/api/stream", mjpegStream(broker))
	// status
	mux.HandleFunc("GET /api/status", statusHandler(broker, startTime, dcimRoot))
	// recordings files: playlists + ts segments for hls.js
	mux.HandleFunc("GET /media/{date}/{segment}/{file}", mediaHandler(dcimRoot))
	// calendar
	mux.HandleFunc("GET /api/recordings/calendar", recordingsCalendarHandler(storage))
	// recordings of one day
	mux.HandleFunc("GET /api/recordings/{date}", recordingDayHandler(storage))
	return &http.Server{
		Addr:    addr,
		Handler: mux,
		// NOTE: no WriteTimeout — it would kill long-lived MJPEG streams
		ReadHeaderTimeout: 10 * time.Second,
	}
}
