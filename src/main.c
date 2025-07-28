#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <time.h>
#include <unistd.h>
#include "sample_comm.h"
#include "pthread.h"
#include "video.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "main.c"

enum { LOG_ERROR, LOG_WARN, LOG_INFO, LOG_DEBUG };

int enable_minilog = 0;
int rkipc_log_level = LOG_DEBUG;

static int g_main_run_ = 1;

static void sig_proc(int signo)
{
	LOG_INFO("received signo %d \n", signo);
	g_main_run_ = 0;
}

int main(int argc, char *argv[])
{
	signal(SIGTERM, sig_proc);
	signal(SIGINT, sig_proc);
	system("RkLunch-stop.sh");
	LOG_INFO("System date: %s\n", get_date_string());
	int ret = 0;
	ret = storage_init();
	ret |= RK_MPI_SYS_Init();
	ret |= rk_video_init();
	ret |= web_server_init();
	if (ret != RK_SUCCESS)
	{
		LOG_ERROR("Init failed!\n");
		return -1;
	}
	while (g_main_run_ == 1)
	{
		sleep(1000);
	}
	web_server_deinit();
	rk_video_deinit();
	RK_MPI_SYS_Exit();
	storage_deinit();
	databases_deinit();
	return 0;
}
