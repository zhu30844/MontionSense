#ifndef __VIDEO_H__
#define __VIDEO_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include "sample_comm.h"
#include "sqlite_comm.h"
#include "storage_comm.h"
#include "util_comm.h"


#define MOTION_DETECTED 1
#define MOTION_UNDETECTED 0
#define HIGH_FRAME_RATE 30
#define LOW_FRAME_RATE 1

static void *rkipc_get_venc_2(void *arg);
static void *rkipc_get_venc_0(void *arg);
static void *rkipc_get_venc_1(void *arg);
static int motion_detecter(int);
static int rkipc_aiq_init();
static int rkipc_vi_dev_init();
static int rkipc_vi_dev_deinit();
static int rkipc_pipe_0_init();
static int rkipc_pipe_0_deinit();
static int rkipc_pipe_1_init();
static int rkipc_pipe_1_deinit();
static int rkipc_pipe_2_init();
static int rkipc_pipe_2_deinit();
static int rkipc_ivs_init();
static int rkipc_ivs_deinit();
static int frame_rate_setter(int, int);
int rk_video_deinit();
int rk_video_init();

#ifdef __cplusplus
#if __cplusplus
}
#endif

#endif /* End of #ifdef __cplusplus */

#endif