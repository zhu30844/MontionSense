#ifndef MOTIONSENSE_STORAGE_H
#define MOTIONSENSE_STORAGE_H

#include "motionsense/app_ctx.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Starts the writer thread that drains ctx->vq into HLS ts/m3u8 files
 * and records metadata in SQLite. Also ensures the MJPEG symlink is valid
 * and the VideoMetadata.db exists.
 */
int  storage_init(app_ctx_t *ctx);

/* Close the frame queue and join the writer thread. Idempotent. */
void storage_stop(app_ctx_t *ctx);

/* Flush final segment, close HLS + SQLite, free resources. */
void storage_deinit(app_ctx_t *ctx);

#ifdef __cplusplus
}
#endif
#endif
