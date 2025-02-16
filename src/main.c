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

static int g_main_run_ = 1;

static void sig_proc(int signo)
{
	printf("received signo %d \n", signo);
	g_main_run_ = 0;
}

int main(int argc, char *argv[])
{
	signal(SIGTERM, sig_proc);
	signal(SIGINT, sig_proc);
	system("RkLunch-stop.sh");
	printf("System date: %s\n", get_date_string());
	int ret = 0;
	ret = storage_init();
	ret |= web_server_init();
	ret |= RK_MPI_SYS_Init();
	ret |= rk_video_init();
	if (ret != RK_SUCCESS)
	{
		printf("Init failed!\n");
		return -1;
	}
	while (g_main_run_ == 1)
	{
		sleep(1000);
	}
	sleep(2);
	web_server_deinit();
	rk_video_deinit();
	RK_MPI_SYS_Exit();
	storage_deinit();
	databases_deinit();
	return 0;
}
