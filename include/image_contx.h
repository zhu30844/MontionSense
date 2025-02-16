#ifndef __IMAGE_CONTX_H__
#define __IMAGE_CONTX_H__

#include <pthread.h>
#include "util_comm.h"

typedef struct
{
    void *p;
    RK_U32 size;
    pthread_rwlock_t lock;
} image_addr_t;


#endif