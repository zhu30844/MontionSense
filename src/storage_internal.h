#ifndef STORAGE_INTERNAL_H
#define STORAGE_INTERNAL_H

/*
 * Private header shared between storage.c and storage_db.c.
 * Not part of the public API — do not include outside these two files.
 */

#include "motionsense/config.h"

#include "sqlite3.h"

/* Forward declarations — full headers included only by storage.c. */
typedef struct hls_media_t hls_media_t;
typedef struct hls_m3u8_t  hls_m3u8_t;

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Constants                                                           */
/* ------------------------------------------------------------------ */
#define VM_STATE_ACTIVE   0
#define VM_STATE_PENDING  1

#define DB_SYNC_NORMAL            0
#define DB_SYNC_FULL              1

#define DATE_STR_SIZE     16   /* "YYYY-MM-DD" + NUL (with slack) */
#define TS_NAME_SIZE      16   /* "<NNNNN>.ts" + NUL (with slack) */

#define DATE_DIR_SIZE     64   /* "<DCIM>/<date>/"                */
#define SEGMENT_DIR_SIZE  96   /* "<DCIM>/<date>/<interrupt>/"    */
#define DB_PATH_SIZE      96   /* full path to a per-day EventLogs.db */
#define MEDIA_PATH_SIZE   160  /* full path to a .ts / .m3u8 file */
#define TMP_PATH_SIZE     192  /* atomic-write temp: "<path>.tmp.XXXXXX" */

/* ------------------------------------------------------------------ */
/* Writer state (only the writer thread touches these)                */
/* ------------------------------------------------------------------ */
typedef struct {
    /* Day and path bookkeeping */
    char      date[DATE_STR_SIZE];      /* "YYYY-MM-DD" */
    int       day_of_year;
    int       interrupt_idx;             /* subfolder counter per day     */
    char      date_dir[DATE_DIR_SIZE];   /* "/mnt/sdcard/DCIM/<date>/"    */
    char      segment_dir[SEGMENT_DIR_SIZE]; /* ".../<date>/<interrupt>/" */

    /* HLS */
    hls_media_t *hls;
    hls_m3u8_t  *m3u;
    int          ts_idx;                 /* within current interrupt_idx  */
    int          ts_since_cleanup;
    int64_t      pts_ms;                 /* fixed 33ms/frame (time-lapse) */

    /* SQLite */
    sqlite3     *meta_db;                /* /mnt/sdcard/DCIM/VideoMetadata.db */
    sqlite3     *event_db;               /* per-day EventLogs.db              */
    int          current_segment_id;     /* PK in VideoSegments           */
    int          frame_count;            /* frame index within segment    */
    int          motion_count;           /* for VideoMetadata rollup      */
    int          last_fps;               /* current fps state             */
    int64_t      last_motion_ms;         /* monotonic ms of last EventDetails write — debounce */
    bool         seen_keyframe;          /* drop frames until the first IDR   */
} writer_state_t;


bool failpoint_hit(const char *name);

int  meta_db_init(writer_state_t *ws);
void meta_db_ensure_day(writer_state_t *ws);

int  event_db_open(writer_state_t *ws);
void event_db_close(writer_state_t *ws);
int  event_db_add_segment(writer_state_t *ws);
void event_db_add_motion(writer_state_t *ws, int video_id, int motion_frame);
void event_db_update_total_frames(writer_state_t *ws);

void meta_db_upsert_motion_count(writer_state_t *ws, int count);
int  meta_db_oldest_retention_candidate(writer_state_t *ws, const char *today, char out[DATE_STR_SIZE]);
int  retention_schedule_delete(writer_state_t *ws, const char *date);
void retention_process_tasks(writer_state_t *ws, int budget);


#endif /* STORAGE_INTERNAL_H */
