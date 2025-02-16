#ifndef __VIDEO_H__
#define __VIDEO_H__

#ifdef __cplusplus
extern "C"
{
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
#include "db_comm.h"
#include "storage_comm.h"
#include "util_comm.h"
#include "web_server.h"
#include "image_contx.h"

#define MOTION_DETECTED 1
#define MOTION_UNDETECTED 0
#define HIGH_FRAME_RATE 30
#define LOW_FRAME_RATE 1

    int rk_video_deinit();
    int rk_video_init();

#ifdef __cplusplus
#if __cplusplus
}
#endif

#endif /* End of #ifdef __cplusplus */

#endif