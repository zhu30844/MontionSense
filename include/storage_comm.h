#ifndef __STORAGE_COMM_H__
#define __STORAGE_COMM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include "util_comm.h"
#include "rk_type.h"
#include "ts.h"


int storage_init();
int storage_deinit();
int write_ts_2_SD(RK_U8 * data, RK_U32 len, RK_BOOL is_key_frame);
int write_raw(RK_U8 * data, RK_U32 len);
static int packet_output(uint8_t *buf, uint32_t len);






#ifdef __cplusplus
#if __cplusplus
}
#endif

#endif /* End of #ifdef __cplusplus */
#endif