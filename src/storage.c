/*
 * storage.c — writer thread: drains ctx->vq into HLS segments and records
 * metadata directly into SQLite. The writer also owns:
 *   - daily folder / EventLogs.db rollover
 *
 * Concurrency model: the writer thread is the *only* writer of the
 * HLS segment files and of the SQLite databases. Web handlers open their
 * own read-only sqlite3 connections; WAL mode makes readers and the
 * writer safely coexist without any app-level locking.
 *
 * SQLite schema and query logic lives in storage_db.c.
 */

#define _XOPEN_SOURCE 500 /* mkstemp */
#define _DEFAULT_SOURCE   /* localtime_r, DT_DIR */

#include "motionsense/storage.h"
#include "storage_internal.h"

#include "motionsense/app_ctx.h"
#include "motionsense/config.h"
#include "motionsense/frame_queue.h"
#include "motionsense/utils.h"

#include "log.h"

#include "hls-m3u8.h"
#include "hls-media.h"
#include "hls-param.h"
#include "mpeg-ts.h"

#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "storage"

/* ------------------------------------------------------------------ */
/* Module state                                                        */
/* ------------------------------------------------------------------ */
static pthread_t s_writer_thr;
static bool s_writer_started;
static writer_state_t s_ws;
static pthread_mutex_t s_writer_boot_mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t s_writer_boot_cv = PTHREAD_COND_INITIALIZER;
static bool s_writer_boot_done;
static bool s_writer_boot_ok;

/* Buffer for m3u8 playlist serialization (2MB). Owned by writer. */
static char s_m3u_buf[2 * 1024 * 1024];

/* ------------------------------------------------------------------ */
/* Time / path helpers                                                 */
/* ------------------------------------------------------------------ */
static void today_string(char out[DATE_STR_SIZE], int *yday_out)
{
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    strftime(out, 16, "%Y-%m-%d", &tm);
    if (yday_out)
        *yday_out = tm.tm_yday;
}

static int count_subdirs(const char *path)
{
    DIR *d = opendir(path);
    if (!d)
        return 0;
    int n = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL)
    {
        if (e->d_type != DT_DIR)
            continue;
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
            continue;
        n++;
    }
    closedir(d);
    return n;
}

bool failpoint_hit(const char *name)
{
    const char *fp = getenv("MS_STORAGE_FAILPOINT");
    return fp && strcmp(fp, name) == 0;
}

static int write_file_atomic(const char *path, const void *data, size_t len)
{
    char tmp[TMP_PATH_SIZE];
    int n = snprintf(tmp, sizeof(tmp), "%s.tmp.XXXXXX", path);
    if (n <= 0 || (size_t)n >= sizeof(tmp))
        return -1;

    int fd = mkstemp(tmp);
    if (fd < 0)
        return -1;

    int rc = 0;
    if (write_all(fd, data, len) != 0)
        rc = -1;
    if (rc == 0 && fsync(fd) != 0)
        rc = -1;
    if (close(fd) != 0)
        rc = -1;

    if (rc == 0 && failpoint_hit("write-before-rename"))
    {
        errno = EIO;
        rc = -1;
    }
    if (rc == 0 && rename(tmp, path) != 0)
        rc = -1;
    if (rc == 0 && failpoint_hit("write-after-rename"))
    {
        errno = EIO;
        rc = -1;
    }
    if (rc == 0 && fsync_parent_dir(path) != 0)
        rc = -1;

    if (rc != 0)
    {
        unlink(tmp);
        return -1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* HLS handler                                                         */
/* ------------------------------------------------------------------ */
static int hls_cb(void *m3u8, const void *data, size_t bytes,
                  int64_t pts, int64_t dts, int64_t duration)
{
    (void)dts;
    writer_state_t *ws = &s_ws;

    static int64_t last_dts = -1;
    int discontinuity = (last_dts < 0) ? 0
                                       : (dts > last_dts + (int64_t)HLS_DURATION / 2 ? 1 : 0);
    last_dts = dts;

    char ts_name[TS_NAME_SIZE];
    char ts_path[MEDIA_PATH_SIZE];
    char m3u_path[MEDIA_PATH_SIZE];
    snprintf(ts_name, sizeof(ts_name), "%05d.ts", ws->ts_idx);
    snprintf(ts_path, sizeof(ts_path), "%s%s", ws->segment_dir, ts_name);
    snprintf(m3u_path, sizeof(m3u_path), "%sindex.m3u8", ws->segment_dir);
    ws->ts_idx++;

    hls_m3u8_add((hls_m3u8_t *)m3u8, ts_name, pts, duration, discontinuity);
    hls_m3u8_playlist((hls_m3u8_t *)m3u8, 1, s_m3u_buf, sizeof(s_m3u_buf));

    size_t m3u_len = strlen(s_m3u_buf);
    if (write_file_atomic(ts_path, data, bytes) != 0)
    {
        MS_LOG_ERROR("atomic write failed for %s: %s\n", ts_path, strerror(errno));
        return -1;
    }
    if (write_file_atomic(m3u_path, s_m3u_buf, m3u_len) != 0)
    {
        MS_LOG_ERROR("atomic write failed for %s: %s\n", m3u_path, strerror(errno));
        return -1;
    }

    ws->ts_since_cleanup++;
    return 0;
}

static int hls_create(writer_state_t *ws)
{
    ws->m3u = hls_m3u8_create(0, 3);
    /* duration=0 → libhls cuts a segment on every keyframe (length follows the
     * encoder GOP). Bounds power-loss to one open segment (~one GOP); the open
     * segment is buffered in RAM until it is cut. See docs/adaptive-timelapse-pts.md. */
    ws->hls = hls_media_create((int64_t)g_cfg.storage.hls_duration_s * 1000, hls_cb, ws->m3u);
    ws->ts_idx = 0;
    if (!ws->hls || !ws->m3u)
        return -1;
    return 0;
}

static void hls_flush_and_destroy(writer_state_t *ws)
{
    if (ws->hls)
    {
        hls_media_input(ws->hls, PSI_STREAM_H264, NULL, 0, 0, 0, 0);
        hls_media_destroy(ws->hls);
        ws->hls = NULL;
    }
    if (ws->m3u)
    {
        hls_m3u8_destroy(ws->m3u);
        ws->m3u = NULL;
    }
}

/* ------------------------------------------------------------------ */
/* Day / segment rollover                                              */
/* ------------------------------------------------------------------ */
static int open_today(writer_state_t *ws)
{
    today_string(ws->date, &ws->day_of_year);
    snprintf(ws->date_dir, sizeof(ws->date_dir), "%s%s/", CFG_DCIM_ROOT, ws->date);
    ensure_dir(CFG_DCIM_ROOT);
    ensure_dir(ws->date_dir);

    ws->interrupt_idx = count_subdirs(ws->date_dir);

    snprintf(ws->segment_dir, sizeof(ws->segment_dir),
             "%s%05d/", ws->date_dir, ws->interrupt_idx);
    if (ensure_dir(ws->segment_dir) != 0)
    {
        MS_LOG_ERROR("mkdir %s\n", ws->segment_dir);
        return -1;
    }

    if (event_db_open(ws) != 0)
    {
        MS_LOG_ERROR("event_db_open failed for %s\n", ws->date_dir);
        return -1;
    }
    ws->current_segment_id = event_db_add_segment(ws);
    if (ws->current_segment_id < 0)
    {
        MS_LOG_ERROR("event_db_add_segment failed\n");
        event_db_close(ws);
        return -1;
    }
    ws->frame_count = 0;
    ws->motion_count = 0;
    ws->last_fps = g_cfg.ivs.fps_low;
    ws->pts_ms = 0;

    if (hls_create(ws) != 0)
    {
        event_db_close(ws);
        return -1;
    }
    MS_LOG_INFO("opened %s (segment %05d)\n", ws->date, ws->interrupt_idx);
    return 0;
}

static int rollover_day(writer_state_t *ws)
{
    hls_flush_and_destroy(ws);
    event_db_update_total_frames(ws);
    event_db_close(ws);
    int rc = open_today(ws);
    if (rc == 0)
        meta_db_ensure_day(ws);
    return rc;
}

/* ------------------------------------------------------------------ */
/* Disk cleanup                                                        */
/* ------------------------------------------------------------------ */
static void check_and_cleanup(writer_state_t *ws)
{
    if (ws->ts_since_cleanup < g_cfg.storage.cleanup_every_ts)
        return;
    ws->ts_since_cleanup = 0;

    /* Always continue pending deletion work, even if space looks healthy. */
    retention_process_tasks(ws, 1);

    uint32_t free_mb = fs_free_mb(CFG_SDCARD_ROOT);
    if (free_mb >= g_cfg.storage.disk_free_min_mb)
        return;

    char target[DATE_STR_SIZE];
    if (meta_db_oldest_retention_candidate(ws, ws->date, target) != 0)
    {
        MS_LOG_WARN("disk low but no cleanup candidate (free=%u MB)\n", free_mb);
        return;
    }

    int queued = retention_schedule_delete(ws, target);
    if (queued < 0)
    {
        MS_LOG_ERROR("cleanup: failed to schedule delete for %s\n", target);
        return;
    }
    if (queued > 0)
    {
        MS_LOG_INFO("cleanup: queued %s (free=%u MB)\n", target, free_mb);
    }
    retention_process_tasks(ws, 1);
}

/* ------------------------------------------------------------------ */
/* Frame ingestion                                                     */
/* ------------------------------------------------------------------ */
static void ingest_frame(writer_state_t *ws, app_ctx_t *ctx, const fq_frame *f)
{
    int yday;
    char date_now[DATE_STR_SIZE];
    today_string(date_now, &yday);
    if (yday != ws->day_of_year)
    {
        MS_LOG_INFO("day rollover: %s -> %s\n", ws->date, date_now);
        rollover_day(ws);
    }

    ws->pts_ms = f->pts_us / 1000;
    hls_media_input(ws->hls, PSI_STREAM_H264, f->data, f->len,
                    ws->pts_ms, ws->pts_ms,
                    f->is_key ? HLS_FLAGS_KEYFRAME : 0);
    ws->frame_count++;

    int fps = atomic_load(&ctx->current_fps);
    ws->last_fps = fps;
    if (fps == g_cfg.ivs.fps_high)
    {
        int64_t now = now_ms();
        if (now - ws->last_motion_ms >= g_cfg.ivs.motion_debounce_ms)
        {
            ws->motion_count++;
            event_db_add_motion(ws, ws->current_segment_id, ws->frame_count);
            meta_db_upsert_motion_count(ws, ws->motion_count);
            ws->last_motion_ms = now;
        }
        /* Motion: update every (fps_high/fps_low) frames → ~1 write/second */
        int ratio = g_cfg.ivs.fps_high / g_cfg.ivs.fps_low;
        if (ws->frame_count % ratio == 0)
            event_db_update_total_frames(ws);
    }
    else
    {
        /* Idle: 1fps, update every frame → ~1 write/second */
        event_db_update_total_frames(ws);
    }
    check_and_cleanup(ws);
}

/* ------------------------------------------------------------------ */
/* Writer thread                                                       */
/* ------------------------------------------------------------------ */
static void *writer_thread(void *arg)
{
    app_ctx_t *ctx = arg;
    writer_state_t *ws = &s_ws;

    if (open_today(ws) != 0)
    {
        pthread_mutex_lock(&s_writer_boot_mu);
        s_writer_boot_done = true;
        s_writer_boot_ok = false;
        pthread_cond_signal(&s_writer_boot_cv);
        pthread_mutex_unlock(&s_writer_boot_mu);
        return NULL;
    }

    if (meta_db_init(ws) != 0)
    {
        pthread_mutex_lock(&s_writer_boot_mu);
        s_writer_boot_done = true;
        s_writer_boot_ok = false;
        pthread_cond_signal(&s_writer_boot_cv);
        pthread_mutex_unlock(&s_writer_boot_mu);
        hls_flush_and_destroy(ws);
        event_db_close(ws);
        return NULL;
    }

    meta_db_ensure_day(ws);

    pthread_mutex_lock(&s_writer_boot_mu);
    s_writer_boot_done = true;
    s_writer_boot_ok = true;
    pthread_cond_signal(&s_writer_boot_cv);
    pthread_mutex_unlock(&s_writer_boot_mu);

    fq_frame f;
    while (fq_pop(ctx->vq, &f) == 0)
    {
        ingest_frame(ws, ctx, &f);
        free(f.data);
    }

    hls_flush_and_destroy(ws);
    event_db_update_total_frames(ws);
    event_db_close(ws);

    MS_LOG_INFO("writer drained, dropped=%zu\n", fq_dropped(ctx->vq));
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */
int storage_init(app_ctx_t *ctx)
{
    memset(&s_ws, 0, sizeof(s_ws));
    s_writer_boot_done = false;
    s_writer_boot_ok = false;

    if (pthread_create(&s_writer_thr, NULL, writer_thread, ctx) != 0)
    {
        MS_LOG_ERROR("pthread_create writer failed: %s\n", strerror(errno));
        return -1;
    }
    s_writer_started = true;

    pthread_mutex_lock(&s_writer_boot_mu);
    while (!s_writer_boot_done)
        pthread_cond_wait(&s_writer_boot_cv, &s_writer_boot_mu);
    bool boot_ok = s_writer_boot_ok;
    pthread_mutex_unlock(&s_writer_boot_mu);

    if (!boot_ok)
    {
        pthread_join(s_writer_thr, NULL);
        s_writer_started = false;
        MS_LOG_ERROR("writer startup handshake failed\n");
        return -1;
    }

    return 0;
}

void storage_stop(app_ctx_t *ctx)
{
    if (!s_writer_started)
        return;
    fq_close(ctx->vq);
    pthread_join(s_writer_thr, NULL);
    s_writer_started = false;
}

void storage_deinit(app_ctx_t *ctx)
{
    storage_stop(ctx);
    if (s_ws.meta_db)
    {
        sqlite3_close(s_ws.meta_db);
        s_ws.meta_db = NULL;
    }
    MS_LOG_INFO("storage down\n");
}
