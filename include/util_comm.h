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