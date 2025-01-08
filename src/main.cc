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

#define MD_area_threshold 0.3
#define VIDEO_PIPE_2 2

static MPP_CHN_S vi_chn[3], ivs_chn;
float md_area_threshold_rate = 0.3;
static int g_main_run_ = 1;
static int pipe_id_ = 0;
pthread_t frame_rate_updater_thread;
volatile int motion_detected = 0;
volatile int motion_detected_pre = 0;
RK_U32 vi_video_hight = 1080;
RK_U32 vi_video_width = 1920;
RK_U32 MD_hight = 1080; // 640 360
RK_U32 MD_width = 1920; // 1920 1080 2304 1296

static void sig_proc(int);
static void *frame_rate_updater(void *arg);
int motion_detecter(int);
int rkipc_aiq_init();
int rkipc_vi_dev_init();
int rkipc_vi_dev_deinit();
int rkipc_pipe_2_init();
int rkipc_pipe_2_deinit();
int rkipc_ivs_init();
int rkipc_ivs_deinit();

RK_U64 TEST_COMM_GetNowUs()
{
	struct timespec time = {0, 0};
	clock_gettime(CLOCK_MONOTONIC, &time);
	return (RK_U64)time.tv_sec * 1000000 + (RK_U64)time.tv_nsec / 1000; /* microseconds */
}

static void sig_proc(int signo)
{
	printf("received signo %d \n", signo);
	g_main_run_ = 0;
	pthread_detach(frame_rate_updater_thread);
}

static void *frame_rate_updater(void *arg)
{
	while (g_main_run_ == 1)
	{
		usleep(33333);
		motion_detected = motion_detecter(0);
		if (motion_detected == 1)
		{
			if (motion_detected_pre == 0)
			{
				printf("update video rate to 30\n");
			}
		}
		else
		{
			if (motion_detected_pre == 1)
			{
				printf("update video rate to 1\n");
			}
			// record video
		}
		motion_detected_pre = motion_detected;
	}
	return NULL;
}

int motion_detecter(int chnId)
{
	int flag = 0;
	IVS_RESULT_INFO_S stResults;
	memset(&stResults, 0, sizeof(IVS_RESULT_INFO_S));
	RK_S32 s32Ret = RK_SUCCESS;
	s32Ret = RK_MPI_IVS_GetResults(chnId, &stResults, 1000);
	if (s32Ret == RK_SUCCESS)
	{
		if (stResults.s32ResultNum == 1)
		{
			printf("MD u32RectNum: %u\n", stResults.pstResults->stMdInfo.u32RectNum);
			if (stResults.pstResults->stMdInfo.u32Square > MD_hight * MD_width * md_area_threshold_rate)
			{
				flag = 1;
				printf("MD: md_area is %d, md_area_threshold is %f\n",
					   stResults.pstResults->stMdInfo.u32Square, md_area_threshold_rate);
			}
		}
		RK_MPI_IVS_ReleaseResults(0, &stResults);
		return flag;
	}
	else
	{
		printf("RK_MPI_IVS_GetResults fail %x", s32Ret);
		return RK_FAILURE;
	}
}

int rkipc_aiq_init()
{
	// init rkaiq
	rk_aiq_working_mode_t hdr_mode = RK_AIQ_WORKING_MODE_NORMAL;
	RK_BOOL multi_sensor = RK_FALSE;
	const char *iq_dir = "/etc/iqfiles";
	SAMPLE_COMM_ISP_Init(0, hdr_mode, multi_sensor, iq_dir);
	SAMPLE_COMM_ISP_Run(0);
	printf("ISP init success\n");
}

int rkipc_vi_dev_init()
{
	int ret = 0;
	VI_DEV_ATTR_S stDevAttr;
	VI_DEV_BIND_PIPE_S stBindPipe;
	memset(&stDevAttr, 0, sizeof(stDevAttr));
	memset(&stBindPipe, 0, sizeof(stBindPipe));
	// 0. get dev config status
	ret = RK_MPI_VI_GetDevAttr(0, &stDevAttr);
	if (ret == RK_ERR_VI_NOT_CONFIG)
	{
		// 0-1.config dev
		ret = RK_MPI_VI_SetDevAttr(0, &stDevAttr);
		if (ret != RK_SUCCESS)
		{
			printf("RK_MPI_VI_SetDevAttr %x\n", ret);
			return -1;
		}
	}
	else
	{
		printf("RK_MPI_VI_SetDevAttr already\n");
	}
	// 1.get dev enable status
	ret = RK_MPI_VI_GetDevIsEnable(0);
	if (ret != RK_SUCCESS)
	{
		// 1-2.enable dev
		ret = RK_MPI_VI_EnableDev(0);
		if (ret != RK_SUCCESS)
		{
			printf("RK_MPI_VI_EnableDev %x\n", ret);
			return -1;
		}
		// 1-3.bind dev/pipe
		stBindPipe.u32Num = pipe_id_;
		stBindPipe.PipeId[0] = pipe_id_;
		ret = RK_MPI_VI_SetDevBindPipe(0, &stBindPipe);
		if (ret != RK_SUCCESS)
		{
			printf("RK_MPI_VI_SetDevBindPipe %x\n", ret);
			return -1;
		}
	}
	else
	{
		printf("RK_MPI_VI_EnableDev already\n");
	}

	return 0;
}

int rkipc_vi_dev_deinit()
{
	RK_MPI_VI_DisableDev(pipe_id_);

	return 0;
}

int rkipc_pipe_2_init()
{
	int ret;
	int buf_cnt = 3;
	// VI FOR MD
	VI_CHN_ATTR_S vi_chn_MD_attr;
	memset(&vi_chn_MD_attr, 0, sizeof(vi_chn_MD_attr));
	vi_chn_MD_attr.stIspOpt.u32BufCount = buf_cnt;
	vi_chn_MD_attr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF;
	vi_chn_MD_attr.stIspOpt.stMaxSize.u32Width = vi_video_width;
	vi_chn_MD_attr.stIspOpt.stMaxSize.u32Height = vi_video_width;
	vi_chn_MD_attr.stSize.u32Width = vi_video_width;
	vi_chn_MD_attr.stSize.u32Height = vi_video_hight;
	vi_chn_MD_attr.enPixelFormat = RK_FMT_YUV420SP;
	vi_chn_MD_attr.stFrameRate.s32DstFrameRate = 30;
	vi_chn_MD_attr.stFrameRate.s32SrcFrameRate = 30;
	vi_chn_MD_attr.enCompressMode = COMPRESS_MODE_NONE;
	vi_chn_MD_attr.u32Depth = 0;
	ret = RK_MPI_VI_SetChnAttr(pipe_id_, VIDEO_PIPE_2, &vi_chn_MD_attr);
	if (ret)
	{
		printf("ERROR: create VI error! ret=%d\n", ret);
		return ret;
	}
	ret = RK_MPI_VI_EnableChn(pipe_id_, VIDEO_PIPE_2);
	if (ret)
	{
		printf("ERROR: create VI error! ret=%d\n", ret);
		return ret;
	}

	rkipc_ivs_init();
	// bind
	vi_chn[2].enModId = RK_ID_VI;
	vi_chn[2].s32DevId = 0;
	vi_chn[2].s32ChnId = VIDEO_PIPE_2;
	ivs_chn.enModId = RK_ID_IVS;
	ivs_chn.s32DevId = 0;
	ivs_chn.s32ChnId = 0;
	ret = RK_MPI_SYS_Bind(&vi_chn[2], &ivs_chn);
	if (ret)
		printf("Bind VI and IVS error! ret=%#x\n", ret);
	else
		printf("Bind VI and IVS success\n");
	return 0;
}

int rkipc_pipe_2_deinit()
{
	int ret;
	// unbind
	ret = RK_MPI_SYS_UnBind(&vi_chn[2], &ivs_chn);
	if (ret)
		printf("Unbind VI and IVS error! ret=%#x\n", ret);
	else
		printf("Unbind VI and IVS success\n");
	rkipc_ivs_deinit();
	ret = RK_MPI_VI_DisableChn(pipe_id_, VIDEO_PIPE_2);
	if (ret)
		printf("ERROR: Destroy VI error! ret=%#x\n", ret);

	return 0;
}

int rkipc_ivs_init()
{
	int ret;
	int buf_cnt = 2;
	RK_BOOL smear = RK_FALSE;
	RK_BOOL weightp = RK_FALSE;
	RK_BOOL md = RK_TRUE;
	if (!smear && !weightp && !md)
	{
		printf("no pp function enabled! end\n");
		return -1;
	}

	// IVS
	IVS_CHN_ATTR_S attr;
	memset(&attr, 0, sizeof(attr));
	attr.enMode = IVS_MODE_MD;
	attr.u32PicWidth = vi_video_width;
	attr.u32PicHeight = vi_video_hight;
	attr.enPixelFormat = RK_FMT_YUV420SP;
	attr.s32Gop = 30;
	attr.bSmearEnable = smear;
	attr.bWeightpEnable = weightp;
	attr.bMDEnable = md;
	attr.s32MDInterval = 5;
	attr.bMDNightMode = RK_TRUE;
	attr.u32MDSensibility = 1;
	ret = RK_MPI_IVS_CreateChn(0, &attr);
	if (ret)
	{
		printf("ERROR: RK_MPI_IVS_CreateChn error! ret=%#x\n", ret);
		return -1;
	}

	IVS_MD_ATTR_S stMdAttr;
	memset(&stMdAttr, 0, sizeof(stMdAttr));
	ret = RK_MPI_IVS_GetMdAttr(0, &stMdAttr);
	if (ret)
	{
		printf("ERROR: RK_MPI_IVS_GetMdAttr error! ret=%#x\n", ret);
		return -1;
	}
	stMdAttr.s32ThreshSad = 40;
	stMdAttr.s32ThreshMove = 2;
	stMdAttr.s32SwitchSad = 0;
	ret = RK_MPI_IVS_SetMdAttr(0, &stMdAttr);
	if (ret)
	{
		printf("ERROR: RK_MPI_IVS_SetMdAttr error! ret=%#x\n", ret);
		return -1;
	}
	if (md == RK_TRUE)
		pthread_create(&frame_rate_updater_thread, NULL, frame_rate_updater, NULL);

	return 0;
}

int rkipc_ivs_deinit()
{
	int ret;
	ret = RK_MPI_IVS_DestroyChn(0);
	if (ret != RK_SUCCESS)
		printf("ERROR: Destroy IVS error! ret=%#x\n", ret);
	else
		printf("RK_MPI_IVS_DestroyChn success\n");
	return 0;
}

int main(int argc, char *argv[])
{
	signal(SIGTERM, sig_proc);
	signal(SIGINT, sig_proc);
	system("RkLunch-stop.sh");
	// init mpi
	RK_MPI_SYS_Init();
	rkipc_vi_dev_init();
	printf("VI init success\n");
	rkipc_pipe_2_init();
	//pthread_create(&frame_rate_updater_thread, NULL, frame_rate_updater, NULL);
	while (g_main_run_ == 1)
	{
		sleep(100);
	}
	rkipc_pipe_2_deinit();
	pthread_join(frame_rate_updater_thread, NULL);
	rkipc_vi_dev_deinit();
	RK_MPI_SYS_Exit();
	return 0;
}
