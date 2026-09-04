package server

import (
	"bufio"
	"encoding/json"
	"errors"
	"fmt"
	"github.com/motionsense/agent/internal/database"
	"github.com/motionsense/agent/internal/recording"
	"github.com/motionsense/agent/internal/stream"
	"log"
	"net/http"
	"os"
	"path/filepath"
	"sort"
	"strconv"
	"strings"
	"time"

	"golang.org/x/sys/unix"
)

// The SoC's only thermal zone. Reads as millidegrees Celsius.
const cpuTempPath = "/sys/class/thermal/thermal_zone0/temp"

// readCPUTemp returns the SoC temperature formatted like "36.3°C", or "" if
// the thermal zone cannot be read. A missing sensor is not worth failing the
// whole status response over.
func readCPUTemp() string {
	raw, err := os.ReadFile(cpuTempPath)
	if err != nil {
		return ""
	}
	milli, err := strconv.Atoi(strings.TrimSpace(string(raw)))
	if err != nil {
		return ""
	}
	return fmt.Sprintf("%.1f°C", float64(milli)/1000.0)
}

// lastRecordDate returns the most recent day under dcimRoot that actually
// holds video, formatted like "2026 Sep 4", or "" if there is none.
//
// Deliberately a filesystem scan rather than a query against VideoMetadata:
// a day row is written when recording starts, and the segment directory is
// created before the first .ts lands in it, so both can name a day that has
// no video in it yet.
func lastRecordDate(dcimRoot string) string {
	days, err := os.ReadDir(dcimRoot)
	if err != nil {
		return ""
	}

	// Skip days dated ahead of today: a boot that starts recording before ntpd
	// corrects the clock writes under whatever date the RTC held, and reporting
	// that as the last recording is misleading.
	today := time.Now().Format("2006-01-02")
	names := make([]string, 0, len(days))
	for _, d := range days {
		if d.IsDir() && recording.ValidDate(d.Name()) && d.Name() <= today {
			names = append(names, d.Name())
		}
	}
	// Names are YYYY-MM-DD, so lexical order is chronological.
	sort.Sort(sort.Reverse(sort.StringSlice(names)))

	for _, name := range names {
		if dayHasVideo(filepath.Join(dcimRoot, name)) {
			t, err := time.Parse("2006-01-02", name)
			if err != nil {
				return ""
			}
			return t.Format("2006 Jan 2")
		}
	}
	return ""
}

// dayHasVideo reports whether any segment directory under dir holds a .ts.
func dayHasVideo(dir string) bool {
	segments, err := os.ReadDir(dir)
	if err != nil {
		return false
	}
	for _, seg := range segments {
		if !seg.IsDir() {
			continue
		}
		files, err := os.ReadDir(filepath.Join(dir, seg.Name()))
		if err != nil {
			continue
		}
		for _, f := range files {
			if !f.IsDir() && strings.HasSuffix(f.Name(), ".ts") {
				return true
			}
		}
	}
	return false
}

// statusResponse : device and app status
type statusResponse struct {
	Clients      int     `json:"clients"`
	SystemUptime string  `json:"systemUptime"`
	AppUptime    string  `json:"appUptime"`
	Totalram     uint64  `json:"totalram"`
	Freeram      uint64  `json:"freeram"`
	WorkLoad     float64 `json:"workLoad"`
	CpuTemp      string  `json:"cpuTemp"`
	// True while frames are still arriving from the C daemon. The page cannot
	// infer this from the <img>: a stalled stream keeps the connection open
	// and fires no event either way.
	StreamLive bool   `json:"streamLive"`
	LastRecord string `json:"lastRecord"`
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
	// Playback length in seconds, summed from the playlist's EXTINF tags.
	// Not derived from TotalFrames: the recorder varies its frame rate with
	// motion, so frames divided by a nominal fps is not the wall-clock length.
	DurationSeconds float64 `json:"durationSeconds"`
	LastWrite       string  `json:"lastWrite"` // TODO: check m3u8 moderation time, should be a string like "2026-05-05"
}

// playlistDuration sums the EXTINF durations in a segment's index.m3u8.
// Returns 0 if the playlist cannot be read or holds no entries.
func playlistDuration(dcimRoot, date, segment string) float64 {
	path, err := recording.MediaFilePath(dcimRoot, date, segment, "index.m3u8")
	if err != nil {
		return 0
	}
	f, err := os.Open(path)
	if err != nil {
		return 0
	}
	defer f.Close()

	var total float64
	sc := bufio.NewScanner(f)
	for sc.Scan() {
		line := sc.Text()
		if !strings.HasPrefix(line, "#EXTINF:") {
			continue
		}
		v := strings.TrimSuffix(strings.TrimPrefix(line, "#EXTINF:"), ",")
		if d, err := strconv.ParseFloat(strings.TrimSpace(v), 64); err == nil {
			total += d
		}
	}
	return total
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
		// Send the headers before waiting for a frame. Without this the
		// handler blocks in the select below and the client sees nothing at
		// all — an <img> then fires neither load nor error and the page sits
		// on "Connecting..." indefinitely when the producer is down.
		w.WriteHeader(http.StatusOK)
		flusher.Flush()

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

func statusHandler(broker *stream.Broker, start time.Time, dcimRoot string) func(http.ResponseWriter, *http.Request) {
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
		// unix.Sysinfo_t's numeric fields are uint64 on 64-bit but uint32 on
		// 32-bit, which is what the RV1106 target is, so convert explicitly.
		//
		// Loads are fixed point with 16 fractional bits; dividing as an
		// integer reported 0 for every load average below 1.00.
		status.WorkLoad = float64(sysInfo.Loads[0]) / 65536.0

		// Totalram and Freeram are counts of Unit bytes, not bytes. Unit is 1
		// on Linux today, so this changes nothing here, but the field is
		// reported as bytes and should actually be bytes.
		unit := uint64(sysInfo.Unit)
		if unit == 0 {
			unit = 1
		}
		status.Totalram = uint64(sysInfo.Totalram) * unit
		status.Freeram = uint64(sysInfo.Freeram) * unit
		status.CpuTemp = readCPUTemp()
		status.LastRecord = lastRecordDate(dcimRoot)

		// Two seconds is comfortably longer than a frame interval at the
		// lowest capture rate this records at.
		if age := broker.SecondsSinceFrame(); age >= 0 && age < 2 {
			status.StreamLive = true
		}

		w.Header().Set("Content-Type", "application/json")
		err := json.NewEncoder(w).Encode(status)
		if err != nil {
			log.Printf("json error")
		}
	}
}

func recordingDayHandler(storage *database.Storage, dcimRoot string) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		date := r.PathValue("date")
		if !recording.ValidDate(date) {
			http.Error(w, "Invalid date", http.StatusBadRequest)
			return
		}
		segments, err := storage.SegmentsForDay(r.Context(), date)
		// A day that never recorded is an empty day, not a failure. Reply 200
		// with no segments so the client renders "nothing here" rather than
		// having to parse an error body.
		if errors.Is(err, database.ErrNoRecordings) {
			segments = nil
			err = nil
		}
		if err != nil {
			http.Error(w, "Get Segments failed", http.StatusInternalServerError)
			return
		}
		events, err := storage.EventsOfDay(r.Context(), date)
		if errors.Is(err, database.ErrNoRecordings) {
			events = nil
			err = nil
		}
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

				DurationSeconds: playlistDuration(dcimRoot, date, segment.Folder),
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
