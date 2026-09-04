package database

import (
	"database/sql"
	"errors"
	"io/fs"
	"os"
	"path/filepath"
	"sync"

	_ "modernc.org/sqlite"
)

type Storage struct {
	dcimRoot string

	mu sync.Mutex
	db *sql.DB
}

func (s *Storage) path() string {
	return filepath.Join(s.dcimRoot, "VideoMetadata.db")
}

// Open prepares access to VideoMetadata.db, expecting /mnt/sdcard/DCIM as
// dcimRoot. It always returns a usable Storage, and the error is for the log
// rather than a reason to give up: the caller's fallback used to be a nil
// Storage, which the handlers then dereferenced, so a card that was not ready
// at startup turned every calendar request into a panic.
//
// Not being ready is the normal case on a freshly formatted card. The daemon
// creates the database when it records its first segment, mode=ro fails
// outright on a file that does not exist, and the agent starts first. So the
// connection is attempted here for the log and retried on use, and the board
// heals itself once recording begins instead of needing the agent restarted.
func Open(dcimRoot string) (*Storage, error) {
	s := &Storage{dcimRoot: dcimRoot}
	return s, s.connect()
}

func (s *Storage) connect() error {
	s.mu.Lock()
	defer s.mu.Unlock()
	if s.db != nil {
		return nil
	}
	// read-only: C process is the sole writer (db is WAL, reads never block its writes).
	// busy_timeout covers the brief lock during WAL checkpoints.
	dsn := "file:" + s.path() + "?mode=ro&_pragma=busy_timeout(5000)"
	db, err := sql.Open("sqlite", dsn)
	if err != nil {
		return err
	}
	if err := db.Ping(); err != nil {
		db.Close()
		return err
	}
	// set up connection paras
	db.SetConnMaxLifetime(0)
	db.SetMaxIdleConns(1)
	db.SetMaxOpenConns(1)
	s.db = db
	return nil
}

// conn hands back the connection, making it first if an earlier attempt found
// no database.
func (s *Storage) conn() (*sql.DB, error) {
	s.mu.Lock()
	db := s.db
	s.mu.Unlock()
	if db != nil {
		return db, nil
	}
	if err := s.connect(); err != nil {
		return nil, err
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	return s.db, nil
}

// absent reports whether the failure to connect is simply that nothing has
// been recorded yet, which is an answer rather than an error.
func (s *Storage) absent() bool {
	_, err := os.Stat(s.path())
	return errors.Is(err, fs.ErrNotExist)
}

func (s *Storage) Close() error {
	s.mu.Lock()
	defer s.mu.Unlock()
	if s.db == nil {
		return nil
	}
	return s.db.Close()
}

func (s *Storage) Ping() error {
	db, err := s.conn()
	if err != nil {
		return err
	}
	return db.Ping()
}
