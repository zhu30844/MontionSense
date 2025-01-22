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


#include "util_comm.h"
#include "rk_type.h"
//#include "ts.h"
#include "mpeg-ts.h"

#ifdef __cplusplus
extern "C" {
#endif


#define DATED_PATH_LENGTH 29
#define TS_FILE_PATH_LENGTH 37


int storage_init();
int storage_deinit();
int write_ts_2_SD(RK_U8 * data, RK_U32 len, RK_BOOL is_key_frame);
int write_raw(RK_U8 * data, RK_U32 len);
static int packet_output(uint8_t *buf, uint32_t len);
int folder_create(const char *folder);
static int get_disk_size(char *path, uint32_t *total_size, 
                        uint32_t *free_size, uint64_t *used_size);






#ifdef __cplusplus
#if __cplusplus
}
#endif

#endif /* End of #ifdef __cplusplus */
#endif