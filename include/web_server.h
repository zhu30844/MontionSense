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

int web_server_init();
void web_server_deinit();
void web_send_image_init(image_addr_t *pimage_addr);


#endif