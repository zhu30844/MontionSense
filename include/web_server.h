// Webserver for MontionSense
// Copyright (C) 2025 ZIXUAN ZHU
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2
// as published by the Free Software Foundation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// For the terms of this license, see <http://www.gnu.org/licenses/>.


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
#include "image_contx.h"
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#define DCIM_DIR_VALID 0
#define DCIM_DIR_NOT_VALID -1
#define HLS_SYMLINK_VALID 1
#define HLS_SYMLINK_NOT_VALID -2

int web_server_init();
void web_server_deinit();
void web_send_image_init(image_addr_t *pimage_addr);


#endif