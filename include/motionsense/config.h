#ifndef MOTIONSENSE_CONFIG_H
#define MOTIONSENSE_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Fixed paths (not user-configurable)                                */
/* ------------------------------------------------------------------ */
#define CFG_SDCARD_ROOT      "/mnt/sdcard"
#define CFG_DCIM_ROOT        "/mnt/sdcard/DCIM/"
#define CFG_VIDEO_META_DB    "/mnt/sdcard/DCIM/VideoMetadata.db"
#define CFG_ISP_IQ_DIR       "/etc/iqfiles"
#define CFG_OSD_FONT_PATH    "/oem/usr/share/MotionSense/fonts/DejaVuSansMono.ttf"
#define CFG_CONFIG_PATH      "/mnt/sdcard/MotionSense/config.yaml"
#define CFG_SOCKET_PATH      "/tmp/motionsense-stream.sock"

/* ------------------------------------------------------------------ */
/* Compile-time defaults (used when config.yaml is absent or partial) */
/* ------------------------------------------------------------------ */
#define CFG_VI_W             1920
#define CFG_VI_H             1080
#define CFG_VI_BASE_FPS      30
#define CFG_H264_BITRATE     4000
#define CFG_IVS_W            640
#define CFG_IVS_H            360
#define CFG_FPS_LOW          1
#define CFG_FPS_HIGH         30
#define CFG_MD_AREA_RATIO    0.05f
#define CFG_IVS_POLL_MS      250
#define CFG_OSD_FONT_SIZE    36
#define CFG_OSD_MARGIN       16
#define CFG_HLS_DURATION_S   0    /* 0 = cut one segment per keyframe (length = GOP) */
#define CFG_DISK_FREE_MIN_MB 2048
#define CFG_CLEANUP_EVERY_TS 20
#define CFG_FQ_CAPACITY      64
#define CFG_MOTION_DEBOUNCE_MS  1000

/* Motion states */
#define MOTION_UNDETECTED    0
#define MOTION_DETECTED      1

/* ------------------------------------------------------------------ */
/* Runtime config struct                                               */
/* ------------------------------------------------------------------ */
typedef enum {
    OSD_POS_TOP_LEFT,
    OSD_POS_TOP_RIGHT,
    OSD_POS_BOTTOM_LEFT,
    OSD_POS_BOTTOM_RIGHT,
} osd_position_t;

typedef struct {
    uint32_t width;
    uint32_t height;
    int      fps;
    uint32_t bitrate_kbps;
    int      gop;
} ms_h264_cfg_t;

typedef struct {
    uint32_t width;
    uint32_t height;
    int      fps;
    uint32_t qfactor_max;
    uint32_t qfactor_min;
} ms_mjpeg_cfg_t;

typedef struct {
    uint32_t width;
    uint32_t height;
    int      fps;
    int      md_interval;
    int      md_sensibility;
    bool     md_night_mode;
    int      thresh_sad;
    int      thresh_move;
    float    area_ratio;
    int      poll_ms;
    int      fps_low;
    int      fps_high;
    int      motion_debounce_ms;
} ms_ivs_cfg_t;

typedef struct {
    int           font_size;
    int           margin;
    osd_position_t position;
} ms_osd_cfg_t;

typedef struct {
    int      hls_duration_s;
    uint32_t disk_free_min_mb;
    int      cleanup_every_ts;
} ms_storage_cfg_t;

typedef struct {
    ms_h264_cfg_t    h264;
    ms_mjpeg_cfg_t   mjpeg;
    ms_ivs_cfg_t     ivs;
    ms_osd_cfg_t     osd;
    ms_storage_cfg_t storage;
} ms_config_t;

extern ms_config_t g_cfg;

/* Load config.yaml; on any error falls back to compiled-in defaults. */
void ms_config_load(const char *path);

#endif /* MOTIONSENSE_CONFIG_H */
