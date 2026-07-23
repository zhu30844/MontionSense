#include "motionsense/frame_queue.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    uint8_t *data;
    size_t len;
    bool is_key;
    int64_t  pts_us;
} slot_t;

struct frame_queue
{
    slot_t *slots;
    size_t capacity;
    size_t head;
    size_t tail;
    size_t count;
    size_t dropped;
    bool closed;
    bool dropping;
    pthread_mutex_t mutex;
    pthread_cond_t cond_nonempty;
};

frame_queue *fq_create(size_t capacity)
{
    if (capacity == 0)
        return NULL;
    frame_queue *q = calloc(1, sizeof(*q));
    if (!q)
        return NULL;

    q->slots = calloc(capacity, sizeof(slot_t));
    if (!q->slots)
    {
        free(q);
        return NULL;
    }

    q->capacity = capacity;

    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->cond_nonempty, NULL);

    return q;
}

void fq_destroy(frame_queue *q)
{
    if (!q)
        return;

    for (size_t i = 0; i < q->capacity; i++)
        free(q->slots[i].data);

    free(q->slots);
    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->cond_nonempty);
    free(q);
}

static void drop_frames(frame_queue *q)
{
    if (q->count == 0)
        return;
    q->dropping = true;
    do
    {
        // Drop the oldest frame first
        slot_t *s = &q->slots[q->head];
        free(s->data);
        s->data = NULL;
        s->len = 0;
        s->is_key = false;

        q->head = (q->head + 1) % q->capacity;
        q->count--;
        q->dropped++;
        // Then keep dropping until we drop a keyframe or the queue is empty
    } while (!(q->slots[q->head].is_key) && q->count > 0);
}

int fq_push(frame_queue *q, const uint8_t *data, size_t len, bool is_key, int64_t pts_us)
{
    if (!q || !data || len == 0)
        return -1;

    uint8_t *copy = malloc(len);
    if (!copy)
        return -1;
    memcpy(copy, data, len);

    pthread_mutex_lock(&q->mutex);

    if (q->closed)
        goto discard;

    if (q->dropping)
    {
        if (!is_key){
            // Count the forced dropped-frame
            q->dropped++;
            goto discard;
        } else {
            // Stop dropping once we see a keyframe
            q->dropping = false;
        }
    }

    if (q->count == q->capacity)
        drop_frames(q);

    q->slots[q->tail].data = copy;
    q->slots[q->tail].len = len;
    q->slots[q->tail].is_key = is_key;
    q->slots[q->tail].pts_us = pts_us;
    q->tail = (q->tail + 1) % q->capacity;
    q->count++;
    pthread_cond_signal(&q->cond_nonempty);
    pthread_mutex_unlock(&q->mutex);
    return 0;

discard:
    pthread_mutex_unlock(&q->mutex);
    free(copy);
    return 0;
}

int fq_pop(frame_queue *q, fq_frame *out)
{
    if (!q || !out) return -1;
    pthread_mutex_lock(&q->mutex);
    while (q->count == 0 && !q->closed)
        pthread_cond_wait(&q->cond_nonempty, &q->mutex);
    if (q->count == 0) {
        pthread_mutex_unlock(&q->mutex);
        return -1;
    }
    out->data   = q->slots[q->head].data;
    out->len    = q->slots[q->head].len;
    out->is_key = q->slots[q->head].is_key;
    out->pts_us = q->slots[q->head].pts_us;
    q->slots[q->head].data = NULL;
    q->head = (q->head + 1) % q->capacity;
    q->count--;
    pthread_mutex_unlock(&q->mutex);
    return 0;
}

void fq_close(frame_queue *q)
{
    if (!q) return;
    pthread_mutex_lock(&q->mutex);
    q->closed = true;
    pthread_cond_broadcast(&q->cond_nonempty);
    pthread_mutex_unlock(&q->mutex);
}

size_t fq_dropped(frame_queue *q)
{
    size_t v;
    pthread_mutex_lock(&q->mutex);
    v = q->dropped;
    pthread_mutex_unlock(&q->mutex);
    return v;
}

size_t fq_depth(frame_queue *q)
{
    size_t v;
    pthread_mutex_lock(&q->mutex);
    v = q->count;
    pthread_mutex_unlock(&q->mutex);
    return v;
}
