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

RK_U64 TEST_COMM_GetNowUs()
{
	struct timespec time = {0, 0};
	clock_gettime(CLOCK_MONOTONIC, &time);
	return (RK_U64)time.tv_sec * 1000000 + (RK_U64)time.tv_nsec / 1000; /* microseconds */
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

int64_t get_timestamp() {
    time_t current_time;
	int64_t timestamp;
    current_time = time(NULL);
    if (current_time == ((time_t)-1)) 
        timestamp = 0;
	else
		timestamp = (int64_t)current_time;
    return timestamp;
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
	union {
		unsigned int a;
		unsigned char b;
	}c;
	c.a = 1;
	// if return 1, little endian
	// if return 0, big endian
	return (c.b == 1);
}