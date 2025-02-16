/*****************************************************************************
* | File        :   util_comm.c
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


#include "util_comm.h"

long long get_curren_time_ms()
{
	long long msec = 0;
	char str[20] = {0};
	struct timespec current_time = {0, 0};
	clock_gettime(CLOCK_MONOTONIC, &current_time);
	sprintf(str, "%ld%03ld", current_time.tv_sec, (current_time.tv_nsec) / 1000000);
	for (size_t i = 0; i < strlen(str); i++)
	{
		msec = msec * 10 + (str[i] - '0');
	}

	return msec;
}

time_t get_time_stamp()
{
	time_t current_time;
	time(&current_time);
	return current_time;
}

char *get_time_string()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	struct tm *timeinfo = localtime(&tv.tv_sec);
	static __thread char time_string[64];
	sprintf(time_string, "%.2d:%.2d:%.2d.%.6ld", timeinfo->tm_hour, timeinfo->tm_min,
			timeinfo->tm_sec, tv.tv_usec);
	return time_string;
}

char *get_date_string()
{
	time_t now;
	struct tm *timeinfo;
	static __thread char date_string[DATE_STRING_LENGTH];
	time(&now);
	timeinfo = localtime(&now);
	strftime(date_string, DATE_STRING_LENGTH, "%Y-%m-%d", timeinfo);
	return date_string;
}

int get_days_in_year()
{
	time_t now;
	struct tm *timeinfo;
	time(&now);
	timeinfo = localtime(&now);
	return timeinfo->tm_yday;
}

int check_endian()
{
	union
	{
		unsigned int a;
		unsigned char b;
	} c;
	c.a = 1;
	// if return 1, little endian
	// if return 0, big endian
	return (c.b == 1);
}

void get_month_date_ranges(char *month_start, char *next_month_start, char *today)
{
	time_t now = time(NULL);
	struct tm *tm_now = localtime(&now);

	strftime(today, 11, "%Y-%m-%d", tm_now);

	tm_now->tm_mday = 1;
	mktime(tm_now);
	strftime(month_start, 11, "%Y-%m-%d", tm_now);

	tm_now->tm_mon += 1;
	tm_now->tm_mday = 1;
	mktime(tm_now);
	strftime(next_month_start, 11, "%Y-%m-%d", tm_now);
}