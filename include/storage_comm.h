/*****************************************************************************
* | File        :   storage_comm.h
* | Author      :   ZIXUAN ZHU
* | Function    :   Storage operations
* | Info        :
*----------------
* |	This version:   V1.0
* | Date        :   2025-02-16
* | Info        :   Basic version
*
# The MIT License (MIT)
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to  whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
******************************************************************************/

#ifndef __STORAGE_COMM_H__
#define __STORAGE_COMM_H__

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <dirent.h>
#include <fcntl.h>
#include <linux/netlink.h>
#include <sys/inotify.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/vfs.h>
#include <assert.h>

#include "hls-m3u8.h"
#include "hls-media.h"
#include "hls-param.h"

#include "util_comm.h"
#include "rk_type.h"
#include "mpeg-ts.h"
#include "db_comm.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define DATE_PATH_LENGTH 30      // Example: "/mnt/sdcard/DCIM/2021-07-01/"
#define TS_FILE_PATH_LENGTH 46    // Example: "/mnt/sdcard/DCIM/2021-07-01/0000/00001.ts"
#define M3U8_FILE_PATH_LENGTH 48  // Example: "/mnt/sdcard/DCIM/2021-07-01/0000/index.m3u8"
#define CLEANUP_INTERVAL 20       // clean up interval, number of ts files
#define DISK_SPACE_THRESHOLD 2048 // disk space threshold, in MB

    typedef enum
    {
        FOLDER_CREATE_SUCCESS = 0,
        FOLDER_CREATE_EXIST = 1,
        FOLDER_CREATE_FAIL = -1,
    } FOLDER_CREATE_STATUS;

    int storage_init();
    int storage_deinit();
    int write_frame_2_SD(RK_U8 *data, RK_U32 len, RK_BOOL is_key_frame, int frame_cycle_time_ms);
    int folder_create(const char *folder);
    int config_hls();
    void *space_cleanup_thread(void *arg);
    int switch_folder();
    int count_subdirectories(const char *dir_path);
    int new_day(int64_t *p_pts, int64_t *p_dts);

#ifdef __cplusplus
#if __cplusplus
}
#endif

#endif /* End of #ifdef __cplusplus */
#endif