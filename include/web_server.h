#ifndef __WEB_SERVER_H__
#define __WEB_SERVER_H__

#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <util_comm.h>
#include "mongoose.h"
#include "db_comm.h"

// Function prototypes
static void cb(struct mg_connection *c, int ev, void *ev_data);
static void broadcast_mjpeg_frame(struct mg_mgr *mgr);
static void timer_callback(void *arg);
int web_server_init();
void web_server_deinit();
static void *web_server_run(void *arg);
void image_reciver(void *data, size_t len);
static void api_handler(struct mg_connection *c, int ev, void *ev_data);
static void motion_count_handler(struct mg_connection *c, int ev, void *ev_data);
static void motion_points_handler(struct mg_connection *c, int ev, void *ev_data);
static void streamer_handler(struct mg_connection *c, int ev, void *ev_data);
static void video_segments_handler(struct mg_connection *c, int ev, void *ev_data);


#endif