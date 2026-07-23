package server

import (
	"encoding/json"
	"fmt"
	"log"
	"github.com/motionsense/agent/internal/database"
	"github.com/motionsense/agent/internal/recording"
	"github.com/motionsense/agent/internal/stream"
	"net/http"
	"time"

	"golang.org/x/sys/unix"
)

// statusResponse : device and app status
type statusResponse struct {
	Clients      int    `json:"clients"`
	SystemUptime string `json:"systemUptime"`
	AppUptime    string `json:"appUptime"`
	Totalram     uint64 `json:"totalram"`
	Freeram      uint64 `json:"freeram"`
	WorkLoad     uint64 `json:"workLoad"`
	CpuTemp      string `json:"cpuTemp"`
	LastDeletion string `json:"lastDeletion"`
}

// DayRecordingResponse : response body for playback page
type DayRecordingResponse struct {
	Date     string            `json:"date"`
	Segments []SegmentResponse `json:"segments"`
}

// SegmentResponse : playback info for each segment
type SegmentResponse struct {
	Segment      string  `json:"segment"`
	StartTime    string  `json:"startTime"`
	TotalFrames  int64   `json:"totalFrames"`
	PlaylistURL  string  `json:"playlistUrl"`
	MotionFrames []int64 `json:"motionFrames"`
	LastWrite    string  `json:"lastWrite"` // TODO: check m3u8 moderation time, should be a string like "2026-05-05"
}

func mjpegStream(broker *stream.Broker) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", fmt.Sprintf("multipart/x-mixed-replace; boundary=%s", mjpegBoundary))
		w.Header().Set("Cache-Control", "no-cache, no-store")
		w.Header().Set("Connection", "keep-alive")

		flusher, ok := w.(http.Flusher)
		if !ok {
			http.Error(w, "Streaming not supported", http.StatusInternalServerError)
			return
		}
		ch := broker.Subscribe()
		defer broker.Unsubscribe(ch)

		ctx := r.Context()
		for {
			select {
			case <-ctx.Done():
				return
			case frame, ok := <-ch:
				if !ok {
					return
				}
				fmt.Fprintf(w, "--%s\r\nContent-Type: image/jpeg\r\nContent-Length: %d\r\n\r\n",
					mjpegBoundary, len(frame))
				w.Write(frame)
				fmt.Fprintf(w, "\r\n")
				flusher.Flush()
			}
		}
	}
}

func statusHandler(broker *stream.Broker, start time.Time) func(http.ResponseWriter, *http.Request) {
	return func(w http.ResponseWriter, r *http.Request) {
		var status statusResponse
		var sysInfo unix.Sysinfo_t
		if err := unix.Sysinfo(&sysInfo); err != nil {
			http.Error(w, "Get System Info failed", http.StatusInternalServerError)
			return
		}
		status.AppUptime = time.Since(start).String()
		status.Clients = broker.ClientCount()
		status.SystemUptime = (time.Duration(sysInfo.Uptime) * time.Second).String()
		status.WorkLoad = sysInfo.Loads[0] / 65536.0
		status.Totalram = sysInfo.Totalram
		status.Freeram = sysInfo.Freeram

		w.Header().Set("Content-Type", "application/json")
		err := json.NewEncoder(w).Encode(status)
		if err != nil {
			log.Printf("json error")
		}
	}
}

func recordingDayHandler(storage *database.Storage) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		date := r.PathValue("date")
		if !recording.ValidDate(date) {
			http.Error(w, "Invalid date", http.StatusBadRequest)
			return
		}
		segments, err := storage.SegmentsForDay(r.Context(), date)
		if err != nil {
			http.Error(w, "Get Segments failed", http.StatusInternalServerError)
			return
		}
		events, err := storage.EventsOfDay(r.Context(), date)
		if err != nil {
			http.Error(w, "Get Events failed", http.StatusInternalServerError)
			return
		}
		eventsByVideoID := make(map[int64][]int64)
		for _, e := range events {
			eventsByVideoID[e.VideoID] = append(eventsByVideoID[e.VideoID], e.MotionFrame)
		}
		response := DayRecordingResponse{
			Date:     date,
			Segments: make([]SegmentResponse, 0, len(segments)),
		}
		for _, segment := range segments {
			response.Segments = append(response.Segments, SegmentResponse{
				Segment:      segment.Folder,
				StartTime:    segment.StartTime,
				TotalFrames:  segment.TotalFrames,
				PlaylistURL:  "/media/" + date + "/" + segment.Folder + "/index.m3u8",
				MotionFrames: eventsByVideoID[segment.ID],
			})
		}
		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(http.StatusOK)
		err = json.NewEncoder(w).Encode(response)
		if err != nil {
			log.Printf("json error")
		}
	}
}

func recordingsCalendarHandler(storage *database.Storage) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		days, err := storage.ListDays(r.Context())
		if err != nil {
			http.Error(w, "failed to list recording days", http.StatusInternalServerError)
			return
		}
		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(http.StatusOK)
		err = json.NewEncoder(w).Encode(days)
		if err != nil {
			log.Printf("json error")
		}
	}
}
