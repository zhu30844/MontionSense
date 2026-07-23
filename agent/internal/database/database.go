package database

import (
	"database/sql"
	"path/filepath"

	_ "modernc.org/sqlite"
)

type Storage struct {
	db       *sql.DB
	dcimRoot string
}

// Open establishes a long connection with VideoMetadata.db.
// expect /mnt/sdcard/DCIM as dcimRoot.
func Open(dcimRoot string) (*Storage, error) {
	// read-only: C process is the sole writer (db is WAL, reads never block its writes).
	// busy_timeout covers the brief lock during WAL checkpoints.
	dsn := "file:" + filepath.Join(dcimRoot, "VideoMetadata.db") + "?mode=ro&_pragma=busy_timeout(5000)"
	db, err := sql.Open("sqlite", dsn)
	if err != nil {
		return nil, err
	}
	if err := db.Ping(); err != nil {
		db.Close()
		return nil, err
	}
	// set up connection paras
	db.SetConnMaxLifetime(0)
	db.SetMaxIdleConns(1)
	db.SetMaxOpenConns(1)
	return &Storage{db: db, dcimRoot: dcimRoot}, nil
}

func (s *Storage) Close() error {
	return s.db.Close()
}

func (s *Storage) Ping() error {
	return s.db.Ping()
}
