package database

import (
	"context"
	"database/sql"

	"github.com/motionsense/agent/internal/recording"
)

// Segment is one row of VideoSegments in a per-day EventLogs.db.
type Segment struct {
	ID          int64
	Folder      string // 00001
	StartTime   string // 17:28:07.449658
	TotalFrames int64  // 131562
}

type EventDetail struct {
	ID          int64
	VideoID     int64 // 1
	MotionFrame int64 // 1315
}

// openDay opens the per-day EventLogs.db for a date like "2026-06-21".
// Per-day dbs are opened on demand and closed by the caller: days come and
// go as the C process records and cleans up.
func (s *Storage) openDay(date string) (*sql.DB, error) {
	// validate even though server already did: a raw date would be joined
	// into the DSN, where a stray "?" could override mode=ro.
	path, err := recording.EventLogsDBPath(s.dcimRoot, date)
	if err != nil {
		return nil, err
	}
	dsn := "file:" + path + "?mode=ro&_pragma=busy_timeout(5000)"
	db, err := sql.Open("sqlite", dsn)
	if err != nil {
		return nil, err
	}
	db.SetMaxOpenConns(1)
	return db, nil
}

// SegmentsForDay returns the recording segments of one day, in start order.
func (s *Storage) SegmentsForDay(ctx context.Context, date string) ([]Segment, error) {
	db, err := s.openDay(date)
	if err != nil {
		return nil, err
	}
	defer db.Close()

	rows, err := db.QueryContext(ctx,
		`SELECT id, folder, start_time, total_frames FROM VideoSegments ORDER BY start_time`)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var segs []Segment
	for rows.Next() {
		var seg Segment
		if err := rows.Scan(&seg.ID, &seg.Folder, &seg.StartTime, &seg.TotalFrames); err != nil {
			return nil, err
		}
		segs = append(segs, seg)
	}
	return segs, rows.Err()
}

// EventsOfSegment returns the motion events of one Segment, not in use
func (s *Storage) EventsOfSegment(ctx context.Context, date string, videoID int64) ([]EventDetail, error) {
	db, err := s.openDay(date)
	if err != nil {
		return nil, err
	}
	defer db.Close()

	rows, err := db.QueryContext(ctx,
		`SELECT id,video_id,motion_frame FROM EventDetails WHERE video_id = ? ORDER BY motion_frame`, videoID)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var events []EventDetail
	for rows.Next() {
		var seg EventDetail
		if err := rows.Scan(&seg.ID, &seg.VideoID, &seg.MotionFrame); err != nil {
			return nil, err
		}
		events = append(events, seg)
	}
	return events, rows.Err()
}

// EventsOfDay returns the motion events of one day.
func (s *Storage) EventsOfDay(ctx context.Context, date string) ([]EventDetail, error) {
	db, err := s.openDay(date)
	if err != nil {
		return nil, err
	}
	defer db.Close()

	rows, err := db.QueryContext(ctx,
		`SELECT id,video_id,motion_frame FROM EventDetails ORDER BY video_id`)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var events []EventDetail
	for rows.Next() {
		var seg EventDetail
		if err := rows.Scan(&seg.ID, &seg.VideoID, &seg.MotionFrame); err != nil {
			return nil, err
		}
		events = append(events, seg)
	}
	return events, rows.Err()
}
