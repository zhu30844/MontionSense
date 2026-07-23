// Package recording encodes the C recorder's on-disk layout:
//
//	DCIM/YYYY-MM-DD/EventLogs.db
//	DCIM/YYYY-MM-DD/00000/index.m3u8
//	DCIM/YYYY-MM-DD/00000/00000.ts
//
// Path builders validate their inputs, so callers holding untrusted
// strings (HTTP path values) can never reach outside this layout.
package recording

import (
	"errors"
	"path/filepath"
	"regexp"
	"time"
)

// allowlist: names must look exactly like the C recorder's layout —
// everything else (db files, temp files) is unreachable.
var (
	dateRe    = regexp.MustCompile(`^[0-9]{4}-[0-9]{2}-[0-9]{2}$`)
	segmentRe = regexp.MustCompile(`^[0-9]{5}$`)
	fileRe    = regexp.MustCompile(`^([0-9]{5}\.ts|index\.m3u8)$`)
)

var ErrInvalidPath = errors.New("invalid recording path")

func ValidDate(date string) bool {
	if !dateRe.MatchString(date) {
		return false
	}
	_, err := time.Parse("2006-01-02", date)
	return err == nil
}

func ValidSegment(segment string) bool {
	return segmentRe.MatchString(segment)
}

func ValidMediaFile(file string) bool {
	return fileRe.MatchString(file)
}

// EventLogsDBPath returns the path of a day's EventLogs.db.
func EventLogsDBPath(dcimRoot, date string) (string, error) {
	if !ValidDate(date) {
		return "", ErrInvalidPath
	}
	return filepath.Join(dcimRoot, date, "EventLogs.db"), nil
}

// MediaFilePath returns the path of a .ts segment file or index.m3u8.
func MediaFilePath(dcimRoot, date, segment, file string) (string, error) {
	if !ValidDate(date) || !ValidSegment(segment) || !ValidMediaFile(file) {
		return "", ErrInvalidPath
	}
	return filepath.Join(dcimRoot, date, segment, file), nil
}
