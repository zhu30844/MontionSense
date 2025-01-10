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
#include <vector>
#include "sample_comm.h"
#include "pthread.h"
#include "video.h"

static int g_main_run_ = 1;

static void sig_proc(int);

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
	// init mpi
	RK_MPI_SYS_Init();
	rk_video_init();
	while (g_main_run_ == 1)
	{
		sleep(100);
	}
	rk_video_deinit();
	RK_MPI_SYS_Exit();
	return 0;
}
