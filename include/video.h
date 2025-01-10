#ifndef __VIDEO_H__
#define __VIDEO_H__

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
#include <vector>
#include "sample_comm.h"
#include "pthread.h"

static void *frame_rate_updater(void *arg);
static void *rkipc_get_venc_0(void *arg);
static void *rkipc_get_venc_1(void *arg);
int motion_detecter(int);
int rkipc_aiq_init();
int rkipc_vi_dev_init();
int rkipc_vi_dev_deinit();
int rkipc_pipe_0_init();
int rkipc_pipe_0_deinit();
int rkipc_pipe_1_init();
int rkipc_pipe_1_deinit();
int rkipc_pipe_2_init();
int rkipc_pipe_2_deinit();
int rkipc_ivs_init();
int rkipc_ivs_deinit();
int frame_rate_setter(int, int);
int rk_video_deinit();
int rk_video_init();

#endif /* VIDEO_H */