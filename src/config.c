#include "motionsense/config.h"
#include "log.h"

#include <yaml.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "config"

/* ------------------------------------------------------------------ */
/* Global config instance — defaults set in ms_config_load()          */
/* ------------------------------------------------------------------ */
ms_config_t g_cfg;

/* ------------------------------------------------------------------ */
/* libyaml document helpers                                           */
/* ------------------------------------------------------------------ */
static yaml_node_t *map_get(yaml_document_t *doc, yaml_node_t *map, const char *key)
{
    if (!map || map->type != YAML_MAPPING_NODE) return NULL;
    for (yaml_node_pair_t *p = map->data.mapping.pairs.start;
         p < map->data.mapping.pairs.top; p++) {
        yaml_node_t *k = yaml_document_get_node(doc, p->key);
        if (k && k->type == YAML_SCALAR_NODE &&
            strcmp((char *)k->data.scalar.value, key) == 0)
            return yaml_document_get_node(doc, p->value);
    }
    return NULL;
}

static const char *str_val(yaml_document_t *doc, yaml_node_t *map, const char *key)
{
    yaml_node_t *n = map_get(doc, map, key);
    if (n && n->type == YAML_SCALAR_NODE)
        return (const char *)n->data.scalar.value;
    return NULL;
}

static int int_val(yaml_document_t *doc, yaml_node_t *map, const char *key, int def)
{
    const char *s = str_val(doc, map, key);
    return s ? atoi(s) : def;
}

static uint32_t u32_val(yaml_document_t *doc, yaml_node_t *map, const char *key, uint32_t def)
{
    const char *s = str_val(doc, map, key);
    return s ? (uint32_t)strtoul(s, NULL, 10) : def;
}

static float float_val(yaml_document_t *doc, yaml_node_t *map, const char *key, float def)
{
    const char *s = str_val(doc, map, key);
    return s ? (float)atof(s) : def;
}

static bool bool_val(yaml_document_t *doc, yaml_node_t *map, const char *key, bool def)
{
    const char *s = str_val(doc, map, key);
    if (!s) return def;
    return strcmp(s, "true") == 0 || strcmp(s, "yes") == 0 || strcmp(s, "1") == 0;
}

static osd_position_t pos_val(yaml_document_t *doc, yaml_node_t *map,
                               const char *key, osd_position_t def)
{
    const char *s = str_val(doc, map, key);
    if (!s) return def;
    if (strcmp(s, "top-left")     == 0) return OSD_POS_TOP_LEFT;
    if (strcmp(s, "top-right")    == 0) return OSD_POS_TOP_RIGHT;
    if (strcmp(s, "bottom-left")  == 0) return OSD_POS_BOTTOM_LEFT;
    if (strcmp(s, "bottom-right") == 0) return OSD_POS_BOTTOM_RIGHT;
    MS_LOG_WARN("unknown osd.position '%s', using default\n", s);
    return def;
}

/* ------------------------------------------------------------------ */
/* Apply defaults                                                      */
/* ------------------------------------------------------------------ */
static void config_defaults(ms_config_t *c)
{
    memset(c, 0, sizeof(*c));

    c->h264.width        = CFG_VI_W;
    c->h264.height       = CFG_VI_H;
    c->h264.fps          = CFG_VI_BASE_FPS;
    c->h264.bitrate_kbps = CFG_H264_BITRATE;
    c->h264.gop          = CFG_FPS_HIGH * 2;

    c->mjpeg.width       = CFG_VI_W;
    c->mjpeg.height      = CFG_VI_H;
    c->mjpeg.fps         = 15;
    c->mjpeg.qfactor_max = 80;
    c->mjpeg.qfactor_min = 65;

    c->ivs.width         = CFG_IVS_W;
    c->ivs.height        = CFG_IVS_H;
    c->ivs.fps           = 15;
    c->ivs.md_interval   = 4;
    c->ivs.md_sensibility= 3;
    c->ivs.md_night_mode = true;
    c->ivs.thresh_sad    = 40;
    c->ivs.thresh_move   = 2;
    c->ivs.area_ratio    = CFG_MD_AREA_RATIO;
    c->ivs.poll_ms       = CFG_IVS_POLL_MS;
    c->ivs.fps_low       = CFG_FPS_LOW;
    c->ivs.fps_high      = CFG_FPS_HIGH;
    c->ivs.motion_debounce_ms = CFG_MOTION_DEBOUNCE_MS;

    c->osd.font_size     = CFG_OSD_FONT_SIZE;
    c->osd.margin        = CFG_OSD_MARGIN;
    c->osd.position      = OSD_POS_BOTTOM_RIGHT;

    c->storage.hls_duration_s   = CFG_HLS_DURATION_S;
    c->storage.disk_free_min_mb = CFG_DISK_FREE_MIN_MB;
    c->storage.cleanup_every_ts = CFG_CLEANUP_EVERY_TS;
}

/* ------------------------------------------------------------------ */
/* Parse sections                                                      */
/* ------------------------------------------------------------------ */
static void parse_h264(yaml_document_t *doc, yaml_node_t *sec, ms_config_t *c)
{
    c->h264.width        = u32_val(doc, sec, "width",        c->h264.width);
    c->h264.height       = u32_val(doc, sec, "height",       c->h264.height);
    c->h264.fps          = int_val(doc, sec, "fps",          c->h264.fps);
    c->h264.bitrate_kbps = u32_val(doc, sec, "bitrate_kbps", c->h264.bitrate_kbps);
    c->h264.gop          = int_val(doc, sec, "gop",          c->h264.gop);
}

static void parse_mjpeg(yaml_document_t *doc, yaml_node_t *sec, ms_config_t *c)
{
    c->mjpeg.width       = u32_val(doc, sec, "width",        c->mjpeg.width);
    c->mjpeg.height      = u32_val(doc, sec, "height",       c->mjpeg.height);
    c->mjpeg.fps         = int_val(doc, sec, "fps",          c->mjpeg.fps);
    c->mjpeg.qfactor_max = u32_val(doc, sec, "qfactor_max",  c->mjpeg.qfactor_max);
    c->mjpeg.qfactor_min = u32_val(doc, sec, "qfactor_min",  c->mjpeg.qfactor_min);
}

static void parse_ivs(yaml_document_t *doc, yaml_node_t *sec, ms_config_t *c)
{
    c->ivs.width          = u32_val  (doc, sec, "width",          c->ivs.width);
    c->ivs.height         = u32_val  (doc, sec, "height",         c->ivs.height);
    c->ivs.fps            = int_val  (doc, sec, "fps",            c->ivs.fps);
    c->ivs.md_interval    = int_val  (doc, sec, "md_interval",    c->ivs.md_interval);
    c->ivs.md_sensibility = int_val  (doc, sec, "md_sensibility", c->ivs.md_sensibility);
    c->ivs.md_night_mode  = bool_val (doc, sec, "md_night_mode",  c->ivs.md_night_mode);
    c->ivs.thresh_sad     = int_val  (doc, sec, "thresh_sad",     c->ivs.thresh_sad);
    c->ivs.thresh_move    = int_val  (doc, sec, "thresh_move",    c->ivs.thresh_move);
    c->ivs.area_ratio     = float_val(doc, sec, "area_ratio",     c->ivs.area_ratio);
    c->ivs.poll_ms        = int_val  (doc, sec, "poll_ms",        c->ivs.poll_ms);
    c->ivs.fps_low        = int_val  (doc, sec, "fps_low",        c->ivs.fps_low);
    c->ivs.fps_high       = int_val  (doc, sec, "fps_high",       c->ivs.fps_high);
    c->ivs.motion_debounce_ms = int_val(doc, sec, "motion_debounce_ms", c->ivs.motion_debounce_ms);
}

static void parse_osd(yaml_document_t *doc, yaml_node_t *sec, ms_config_t *c)
{
    c->osd.font_size = int_val(doc, sec, "font_size", c->osd.font_size);
    c->osd.margin    = int_val(doc, sec, "margin",    c->osd.margin);
    c->osd.position  = pos_val(doc, sec, "position",  c->osd.position);
}

static void parse_storage(yaml_document_t *doc, yaml_node_t *sec, ms_config_t *c)
{
    c->storage.hls_duration_s   = int_val(doc, sec, "hls_duration_s",   c->storage.hls_duration_s);
    c->storage.disk_free_min_mb = u32_val(doc, sec, "disk_free_min_mb", c->storage.disk_free_min_mb);
    c->storage.cleanup_every_ts = int_val(doc, sec, "cleanup_every_ts", c->storage.cleanup_every_ts);
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */
void ms_config_load(const char *path)
{
    config_defaults(&g_cfg);

    FILE *f = fopen(path, "r");
    if (!f) {
        MS_LOG_WARN("config file not found at %s, using defaults\n", path);
        return;
    }

    yaml_parser_t   parser;
    yaml_document_t doc;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_file(&parser, f);

    if (!yaml_parser_load(&parser, &doc)) {
        MS_LOG_WARN("config YAML parse error at line %zu, using defaults\n",
                 parser.problem_mark.line + 1);
        yaml_parser_delete(&parser);
        fclose(f);
        return;
    }

    yaml_node_t *root = yaml_document_get_root_node(&doc);
    if (root && root->type == YAML_MAPPING_NODE) {
        yaml_node_t *sec;
        if ((sec = map_get(&doc, root, "h264")))    parse_h264   (&doc, sec, &g_cfg);
        if ((sec = map_get(&doc, root, "mjpeg")))   parse_mjpeg  (&doc, sec, &g_cfg);
        if ((sec = map_get(&doc, root, "ivs")))     parse_ivs    (&doc, sec, &g_cfg);
        if ((sec = map_get(&doc, root, "osd")))     parse_osd    (&doc, sec, &g_cfg);
        if ((sec = map_get(&doc, root, "storage"))) parse_storage(&doc, sec, &g_cfg);
    }

    yaml_document_delete(&doc);
    yaml_parser_delete(&parser);
    fclose(f);

    MS_LOG_INFO("loaded from %s (h264 %ux%u@%d %ukbps, ivs area=%.2f)\n",
             path,
             g_cfg.h264.width, g_cfg.h264.height, g_cfg.h264.fps,
             g_cfg.h264.bitrate_kbps, g_cfg.ivs.area_ratio);
}
