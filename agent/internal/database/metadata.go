package database

import "context"

// DayMetadata is one row of VideoMetadata in the master db: one recording day.
type DayMetadata struct {
	Date        string `json:"date"`
	MotionCount int64  `json:"motionCount"`
	State       int    `json:"state"`
}

// ListDays returns all recording days, newest first.
func (s *Storage) ListDays(ctx context.Context) ([]DayMetadata, error) {
	rows, err := s.db.QueryContext(ctx,
		`SELECT date, motion_count, state FROM VideoMetadata ORDER BY date DESC`)
	if err != nil {
		return nil, err
	}
	defer rows.Close()

	var days []DayMetadata
	for rows.Next() {
		var d DayMetadata
		if err := rows.Scan(&d.Date, &d.MotionCount, &d.State); err != nil {
			return nil, err
		}
		days = append(days, d)
	}
	return days, rows.Err()
}
