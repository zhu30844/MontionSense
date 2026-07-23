#ifndef MOTIONSENSE_FRAME_QUEUE_H
#define MOTIONSENSE_FRAME_QUEUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct frame_queue frame_queue;

typedef struct {
    uint8_t *data;
    size_t   len;
    bool     is_key;
    int64_t  pts_us;
} fq_frame;

frame_queue *fq_create(size_t capacity);
void           fq_destroy(frame_queue *q);

int fq_push(frame_queue *q, const uint8_t *data, size_t len, bool is_key, int64_t pts_us);

int fq_pop(frame_queue *q, fq_frame *out);

/* Wake any blocked poppers. Subsequent pops drain remaining items then fail. */
void   fq_close(frame_queue *q);
size_t fq_dropped(frame_queue *q);
size_t fq_depth(frame_queue *q);

#ifdef __cplusplus
}
#endif
#endif
