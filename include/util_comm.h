/*****************************************************************************
* | File        :   util_comm.h
* | Author      :   ZIXUAN ZHU
* | Function    :   A set of utility functions about time, date and endians
* | Info        :
*----------------
* |	This version:   V1.0
* | Date        :   2025-02-16
* | Info        :   Basic version
*
# The MIT License (MIT)
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documnetation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to  whom the Software is
# furished to do so, subject to the following conditions:
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


#ifndef __UTIL_COMM_H__
#define __UTIL_COMM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <inttypes.h> // PRId64
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include "version.h"
#include "rk_type.h"

#define DATE_STRING_LENGTH 11

#define PRINT_LINE() printf("==========================================\n")

long long get_curren_time_ms();
char *get_time_string();
char *get_date_string();
int64_t get_timestamp();
int get_days_in_year();
int check_endian();
void get_month_date_ranges(char *month_start, char *next_month_start, char *today) ;

#ifdef __cplusplus
#if __cplusplus
}
#endif

#endif /* End of #ifdef __cplusplus */

#endif