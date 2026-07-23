/*
 * storage_db.c — SQLite layer for the writer thread.
 *
 * Owns two databases:
 *   meta_db   — VideoMetadata.db  (global, one row per recording day)
 *   event_db  — EventLogs.db      (per-day, one row per HLS segment / motion event)
 *
 * Also owns the retention logic: VideoMetadata.state marks days pending deletion
 * so Go stops serving them before files are physically removed.
 *
 * All functions are called exclusively from the writer thread.
 * Read-only access from the Go web process uses separate sqlite3 connections
 * in WAL mode — no app-level locking is needed.
 */

#define _DEFAULT_SOURCE    /* gettimeofday */

#include "storage_internal.h"
#include "motionsense/config.h"
#include "motionsense/utils.h"
#include "log.h"
#include "sqlite3.h"

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "storage"

/* ------------------------------------------------------------------ */
/* Low-level SQLite helpers                                            */
/* ------------------------------------------------------------------ */
static int db_open_rw(sqlite3 **out, const char *path, int sync_mode)
{
    int rc = sqlite3_open(path, out);
    if (rc != SQLITE_OK) {
        const char *msg = (*out) ? sqlite3_errmsg(*out) : "sqlite_open failed";
        MS_LOG_ERROR("sqlite_open %s: %s\n", path, msg);
        if (*out) sqlite3_close(*out);
        *out = NULL;
        return -1;
    }
    char *err = NULL;
    sqlite3_exec(*out, "PRAGMA journal_mode=WAL;",     NULL, NULL, &err); sqlite3_free(err); err = NULL;
    if (sync_mode == DB_SYNC_FULL)
        sqlite3_exec(*out, "PRAGMA synchronous=FULL;", NULL, NULL, &err);
    else
        sqlite3_exec(*out, "PRAGMA synchronous=NORMAL;", NULL, NULL, &err);
    sqlite3_free(err); err = NULL;
    sqlite3_exec(*out, "PRAGMA cache_size=1000;",      NULL, NULL, &err); sqlite3_free(err);
    return 0;
}

static int db_exec(sqlite3 *db, const char *sql)
{
    char *err = NULL;
    if (sqlite3_exec(db, sql, NULL, NULL, &err) != SQLITE_OK) {
        MS_LOG_ERROR("sqlite exec: %s\n", err ? err : "?");
        sqlite3_free(err);
        return -1;
    }
    return 0;
}



/* ------------------------------------------------------------------ */
/* meta_db — VideoMetadata.db                                         */
/* ------------------------------------------------------------------ */
int meta_db_init(writer_state_t *ws)
{
    if (db_open_rw(&ws->meta_db, CFG_VIDEO_META_DB, DB_SYNC_FULL) != 0) return -1;

    if (db_exec(ws->meta_db,
            "CREATE TABLE IF NOT EXISTS VideoMetadata ("
            " id INTEGER PRIMARY KEY AUTOINCREMENT,"
            " date TEXT NOT NULL UNIQUE,"
            " motion_count INTEGER NOT NULL DEFAULT -1,"
            " state INTEGER NOT NULL DEFAULT 0);") != 0)
        return -1;

    return 0;
}

void meta_db_ensure_day(writer_state_t *ws)
{
    if (!ws->meta_db) return;
    sqlite3_stmt *st = NULL;
    const char *sql =
        "INSERT OR IGNORE INTO VideoMetadata (date, motion_count, state)"
        " VALUES (?, 0, 0);";
    if (sqlite3_prepare_v2(ws->meta_db, sql, -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, ws->date, -1, SQLITE_STATIC);
        sqlite3_step(st);
        sqlite3_finalize(st);
    }
}

void meta_db_upsert_motion_count(writer_state_t *ws, int count)
{
    if (!ws->meta_db) return;
    sqlite3_stmt *st = NULL;
    const char *sql = "INSERT INTO VideoMetadata (date, motion_count) VALUES (?, ?)"
                      " ON CONFLICT(date) DO UPDATE SET motion_count=excluded.motion_count;";
    if (sqlite3_prepare_v2(ws->meta_db, sql, -1, &st, NULL) != SQLITE_OK) {
        MS_LOG_ERROR("upsert prepare failed: %s\n", sqlite3_errmsg(ws->meta_db));
        return;
    }
    sqlite3_bind_text(st, 1, ws->date, -1, SQLITE_STATIC);
    sqlite3_bind_int(st, 2, count);
    if (sqlite3_step(st) != SQLITE_DONE) {
        MS_LOG_ERROR("upsert motion_count failed for %s: %s\n",
                  ws->date, sqlite3_errmsg(ws->meta_db));
    }
    sqlite3_finalize(st);
}


int meta_db_oldest_retention_candidate(writer_state_t *ws, const char *today, char out[DATE_STR_SIZE])
{
    if (!ws->meta_db) return -1;
    sqlite3_stmt *st = NULL;
    int rc = -1;
    const char *sql =
        "SELECT date FROM VideoMetadata "
        "WHERE date != ? AND state = ? "
        "ORDER BY date ASC LIMIT 1;";
    if (sqlite3_prepare_v2(ws->meta_db, sql, -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, today, -1, SQLITE_STATIC);
        sqlite3_bind_int(st, 2, VM_STATE_ACTIVE);
        if (sqlite3_step(st) == SQLITE_ROW) {
            const unsigned char *txt = sqlite3_column_text(st, 0);
            if (txt) {
                strncpy(out, (const char *)txt, DATE_STR_SIZE - 1);
                out[DATE_STR_SIZE - 1] = '\0';
                rc = 0;
            }
        }
        sqlite3_finalize(st);
    }
    return rc;
}


/* ------------------------------------------------------------------ */
/* event_db — per-day EventLogs.db                                    */
/* ------------------------------------------------------------------ */
int event_db_open(writer_state_t *ws)
{
    char path[DB_PATH_SIZE];
    snprintf(path, sizeof(path), "%sEventLogs.db", ws->date_dir);
    if (db_open_rw(&ws->event_db, path, DB_SYNC_NORMAL) != 0) return -1;
    if (db_exec(ws->event_db,
            "CREATE TABLE IF NOT EXISTS VideoSegments ("
            " id INTEGER PRIMARY KEY AUTOINCREMENT,"
            " folder TEXT NOT NULL UNIQUE,"
            " start_time TEXT NOT NULL,"
            " total_frames INTEGER NOT NULL DEFAULT -1);") != 0) {
        sqlite3_close(ws->event_db);
        ws->event_db = NULL;
        return -1;
    }
    if (db_exec(ws->event_db,
            "CREATE TABLE IF NOT EXISTS EventDetails ("
            " id INTEGER PRIMARY KEY AUTOINCREMENT,"
            " video_id INTEGER NOT NULL,"
            " motion_frame INTEGER NOT NULL,"
            " FOREIGN KEY(video_id) REFERENCES VideoSegments(id));") != 0) {
        sqlite3_close(ws->event_db);
        ws->event_db = NULL;
        return -1;
    }
    if (db_exec(ws->event_db,
            "CREATE INDEX IF NOT EXISTS idx_eventdetails_video_id"
            " ON EventDetails(video_id);") != 0) {
        sqlite3_close(ws->event_db);
        ws->event_db = NULL;
        return -1;
    }
    return 0;
}

void event_db_close(writer_state_t *ws)
{
    if (ws->event_db) { sqlite3_close(ws->event_db); ws->event_db = NULL; }
}

int event_db_add_segment(writer_state_t *ws)
{
    if (!ws->event_db) return -1;
    char folder[8];
    snprintf(folder, sizeof(folder), "%05d", ws->interrupt_idx);

    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    struct timeval tv;
    gettimeofday(&tv, NULL);
    char start_time[32];
    snprintf(start_time, sizeof(start_time), "%02d:%02d:%02d.%06ld",
             tm.tm_hour, tm.tm_min, tm.tm_sec, (long)tv.tv_usec);

    sqlite3_stmt *st = NULL;
    const char *sql = "INSERT INTO VideoSegments (folder, start_time)"
                      " VALUES (?, ?);";
    int id = -1;
    if (sqlite3_prepare_v2(ws->event_db, sql, -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_text(st, 1, folder,     -1, SQLITE_STATIC);
        sqlite3_bind_text(st, 2, start_time, -1, SQLITE_STATIC);
        if (sqlite3_step(st) == SQLITE_DONE)
            id = (int)sqlite3_last_insert_rowid(ws->event_db);
        sqlite3_finalize(st);
    }
    return id;
}


void event_db_add_motion(writer_state_t *ws, int video_id, int motion_frame)
{
    if (!ws->event_db || video_id < 0) return;
    sqlite3_stmt *st = NULL;
    const char *sql = "INSERT INTO EventDetails (video_id, motion_frame)"
                      " VALUES (?, ?);";
    if (sqlite3_prepare_v2(ws->event_db, sql, -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_int(st, 1, video_id);
        sqlite3_bind_int(st, 2, motion_frame);
        sqlite3_step(st);
        sqlite3_finalize(st);
    }
}

void event_db_update_total_frames(writer_state_t *ws)
{
    if (!ws->event_db || ws->current_segment_id < 0) return;
    sqlite3_stmt *st = NULL;
    const char *sql = "UPDATE VideoSegments SET total_frames=? WHERE id=?;";
    if (sqlite3_prepare_v2(ws->event_db, sql, -1, &st, NULL) == SQLITE_OK) {
        sqlite3_bind_int(st, 1, ws->frame_count);
        sqlite3_bind_int(st, 2, ws->current_segment_id);
        sqlite3_step(st);
        sqlite3_finalize(st);
    }
}

/* ------------------------------------------------------------------ */
/* Retention — disk cleanup                                            */
/* ------------------------------------------------------------------ */

/* Mark a day as pending deletion (state=VM_STATE_PENDING).
 * Go filters state != VM_STATE_ACTIVE from all queries, so the day
 * disappears from the API immediately after this call. */
int retention_schedule_delete(writer_state_t *ws, const char *date)
{
    sqlite3_stmt *st = NULL;
    if (sqlite3_prepare_v2(ws->meta_db,
            "UPDATE VideoMetadata SET state=? WHERE date=? AND state=?;",
            -1, &st, NULL) != SQLITE_OK)
        return -1;
    sqlite3_bind_int(st, 1, VM_STATE_PENDING);
    sqlite3_bind_text(st, 2, date, -1, SQLITE_STATIC);
    sqlite3_bind_int(st, 3, VM_STATE_ACTIVE);
    int step_rc = sqlite3_step(st);
    sqlite3_finalize(st);
    if (step_rc != SQLITE_DONE) return -1;
    return sqlite3_changes(ws->meta_db) > 0 ? 1 : 0;
}

void retention_process_tasks(writer_state_t *ws, int budget)
{
    for (int i = 0; i < budget; i++) {
        sqlite3_stmt *st = NULL;
        char date[DATE_STR_SIZE] = {0};
        char path[DATE_DIR_SIZE] = {0};

        if (sqlite3_prepare_v2(ws->meta_db,
                "SELECT date FROM VideoMetadata WHERE state=? LIMIT 1;",
                -1, &st, NULL) != SQLITE_OK)
            return;
        sqlite3_bind_int(st, 1, VM_STATE_PENDING);

        if (sqlite3_step(st) != SQLITE_ROW) {
            sqlite3_finalize(st);
            return;
        }
        const unsigned char *date_txt = sqlite3_column_text(st, 0);
        if (date_txt) strncpy(date, (const char *)date_txt, sizeof(date) - 1);
        sqlite3_finalize(st);

        if (date[0] == '\0') return;

        /* state=VM_STATE_PENDING already means Go won't serve this day.
         * Delete files first; if we crash here, the row stays PENDING
         * and the next startup will retry. */
        snprintf(path, sizeof(path), "%s%s", CFG_DCIM_ROOT, date);
        if (remove_path_tree(path) != 0) {
            MS_LOG_ERROR("cleanup: delete %s failed: %s\n", path, strerror(errno));
            return;
        }

        if (sqlite3_prepare_v2(ws->meta_db,
                "DELETE FROM VideoMetadata WHERE date=?;",
                -1, &st, NULL) != SQLITE_OK) {
            MS_LOG_ERROR("cleanup: remove VideoMetadata row failed for %s\n", date);
            return;
        }
        sqlite3_bind_text(st, 1, date, -1, SQLITE_STATIC);
        if (sqlite3_step(st) != SQLITE_DONE) {
            sqlite3_finalize(st);
            MS_LOG_ERROR("cleanup: remove VideoMetadata row failed for %s\n", date);
            return;
        }
        sqlite3_finalize(st);

        MS_LOG_INFO("cleanup: deleted %s\n", path);
    }
}
