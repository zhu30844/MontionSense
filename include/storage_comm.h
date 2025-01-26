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

#include "flv-proto.h"
#include "flv-reader.h"
#include "flv-demuxer.h"

#include "util_comm.h"
#include "rk_type.h"
//#include "ts.h"
#include "mpeg-ts.h"

#ifdef __cplusplus
extern "C" {
#endif


#define DATED_PATH_LENGTH 30
#define TS_FILE_PATH_LENGTH 46
#define M3U8_FILE_PATH_LENGTH 48


int storage_init();
int storage_deinit();
int write_ts_2_SD(RK_U8 * data, RK_U32 len, RK_BOOL is_key_frame);
static int hls_handler(void* m3u8, const void* data, size_t bytes, int64_t pts, int64_t dts, int64_t duration);
int folder_create(const char *folder);
static int get_disk_size(char *path, uint32_t *total_size, 
                        uint32_t *free_size, uint64_t *used_size);
int config_hls();
void *space_cleanup_thread(void *arg);
void free_up_disk_notify();






#ifdef __cplusplus
#if __cplusplus
}
#endif

#endif /* End of #ifdef __cplusplus */
#endif