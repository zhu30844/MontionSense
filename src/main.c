#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>


// App headers
#include "motionsense/app_ctx.h"
#include "motionsense/config.h"
#include "motionsense/media.h"
#include "motionsense/storage.h"

#include "log.h"

static app_ctx_t g_ctx;

static void on_signal(int signo)
{
    (void)signo;
    atomic_store(&g_ctx.is_running, false);
}

/* ------------------------------------------------------------------ */
int app_ctx_init(app_ctx_t *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    atomic_init(&ctx->is_running,   true);
    atomic_init(&ctx->motion_state, MOTION_UNDETECTED);
    atomic_init(&ctx->current_fps,  CFG_FPS_LOW);

    ctx->vq = fq_create(CFG_FQ_CAPACITY);
    if (!ctx->vq) return -1;
    return 0;
}

void app_ctx_deinit(app_ctx_t *ctx)
{
    if (ctx->vq) { fq_destroy(ctx->vq); ctx->vq = NULL; }
}

/* ------------------------------------------------------------------ */
int main(void)
{
    openlog(LOG_TAG, LOG_PID | LOG_CONS, LOG_DAEMON);
    setlogmask(LOG_UPTO(LOG_DEBUG));   /* let DEBUG/INFO through to syslog */

    struct sigaction sa = {0};
    sa.sa_handler = on_signal;
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    signal(SIGPIPE, SIG_IGN);

    ms_config_load(CFG_CONFIG_PATH);

    if (app_ctx_init(&g_ctx) != 0) {
        MS_LOG_ERROR("app_ctx_init failed\n");
        return 1;
    }

    if (storage_init(&g_ctx) != 0) { MS_LOG_ERROR("storage_init failed\n"); goto err_ctx; }
    if (media_init  (&g_ctx) != 0) { MS_LOG_ERROR("media_init failed\n");   goto err_storage; }

    MS_LOG_INFO("MotionSense running (pid %d)\n", getpid());
    while (atomic_load(&g_ctx.is_running)) sleep(1);
    MS_LOG_INFO("shutting down\n");

    media_stop(&g_ctx);
    storage_stop(&g_ctx);

    media_deinit(&g_ctx);
    storage_deinit(&g_ctx);
    app_ctx_deinit(&g_ctx);
    closelog();
    return 0;

err_storage: storage_deinit(&g_ctx);
err_ctx:     app_ctx_deinit(&g_ctx);
    return 1;
}
