#ifndef MOTIONSENSE_APP_CTX_H
#define MOTIONSENSE_APP_CTX_H

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

#include "motionsense/frame_queue.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The one and only application context.
 *
 * Lifetime is owned by main.c. Every module takes a pointer and must not
 * cache anything globally. Cross-thread communication goes through exactly
 * two channels:
 *
 *   - ctx->vq         H264 NALU frames: capture_thread -> writer_thread
 *   - atomic fields   low-frequency state (run flag, motion, fps)
 */
typedef struct {
    atomic_bool is_running;
    atomic_int  motion_state;   /* MOTION_* */
    atomic_int  current_fps;    /* CFG_FPS_LOW | CFG_FPS_HIGH */

    /* H264 data plane */
    frame_queue *vq;
} app_ctx_t;

int  app_ctx_init(app_ctx_t *ctx);
void app_ctx_deinit(app_ctx_t *ctx);

#ifdef __cplusplus
}
#endif
#endif
