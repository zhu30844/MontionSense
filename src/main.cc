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

#define vi_video_hight 1920
#define vi_video_width 1080
#define MD_hight 640
#define MD_width 360
#define MD_area_threshold 0.3

typedef struct _rkMpiCtx
{
	SAMPLE_VI_CTX_S vi;
} SAMPLE_MPI_CTX_S;

static MPP_CHN_S vi_chn, ivs_chn;
int md_area_threshold_rate = 0.3;
static int g_main_run_ = 1;
pthread_t MD_thread, recorder_thread;
pthread_mutex_t mutex;
pthread_cond_t cond_var;
int motion_detected = 0;

RK_U64 TEST_COMM_GetNowUs()
{
	struct timespec time = {0, 0};
	clock_gettime(CLOCK_MONOTONIC, &time);
	return (RK_U64)time.tv_sec * 1000000 + (RK_U64)time.tv_nsec / 1000; /* microseconds */
}

int motion_detecter(int chnId, float md_area_threshold_rate)
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
				flag = 1;
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

static void sig_proc(int signo)
{
	printf("received signo %d \n", signo);
	g_main_run_ = 0;
	pthread_detach(MD_thread);
}

static void *MD_result_updater(void *arg)
{
	while (g_main_run_ == 1)
	{
		usleep(33333);
		pthread_mutex_lock(&mutex);
		motion_detected = motion_detecter(0, md_area_threshold_rate);
		pthread_cond_signal(&cond_var);
		pthread_mutex_unlock(&mutex);
	}
}

static void *video_recorder(void *arg)
{
	while (g_main_run_ == 1)
	{
		pthread_mutex_lock(&mutex);
		while (motion_detected == 0)
		{
			printf("waiting for motion\n");
			pthread_cond_wait(&cond_var, &mutex); // 等待运动检测信号
		}
		printf("motion detected, high rate recording...\n");
		printf("update rate 30 \n");
		for(int i = 29; i > 0; i--)
		{
			printf("recording...%d\n", i);
			sleep(1);
		}
		motion_detected = motion_detecter(0, md_area_threshold_rate);
		sleep(1);
		if(motion_detected == 0)
		{
			printf("motion end, low rate recording...\n");
			printf("update rate 1 \n");
		}
		else
		{
			printf("motion continue, high rate recording...\n");
		}
		pthread_mutex_unlock(&mutex);
	}
	return NULL;
}

int main(int argc, char *argv[])
{
	signal(SIGTERM, sig_proc);
	signal(SIGINT, sig_proc);
	system("RkLunch-stop.sh");
	// config vi
	RK_S32 s32Ret = RK_SUCCESS;
	SAMPLE_MPI_CTX_S *ctx;
	RK_U32 u32ViWidth = 640;
	RK_U32 u32ViHeight = 360;
	RK_CHAR *pOutPath = NULL;
	RK_CHAR *pDeviceName = NULL;
	RK_S32 s32CamId = 0;
	RK_S32 s32ChnId = 1;
	RK_S32 s32loopCnt = -1;
	PIXEL_FORMAT_E PixelFormat = RK_FMT_RGB565;
	COMPRESS_MODE_E CompressMode = COMPRESS_MODE_NONE;
	rk_aiq_working_mode_t hdr_mode = RK_AIQ_WORKING_MODE_NORMAL;
	ctx = (SAMPLE_MPI_CTX_S *)(malloc(sizeof(SAMPLE_MPI_CTX_S)));
	memset(ctx, 0, sizeof(SAMPLE_MPI_CTX_S));

	// init rkaiq
	RK_BOOL multi_sensor = RK_FALSE;
	const char *iq_dir = "/etc/iqfiles";
	// hdr_mode = RK_AIQ_WORKING_MODE_ISP_HDR2;
	SAMPLE_COMM_ISP_Init(0, hdr_mode, multi_sensor, iq_dir);
	SAMPLE_COMM_ISP_Run(0);

	// init mpi
	RK_MPI_SYS_Init();

	// Init VI
	{
		ctx->vi.u32Width = u32ViWidth;
		ctx->vi.u32Height = u32ViHeight;
		ctx->vi.s32DevId = s32CamId;
		ctx->vi.u32PipeId = ctx->vi.s32DevId;
		ctx->vi.s32ChnId = s32ChnId;
		ctx->vi.stChnAttr.stIspOpt.u32BufCount = 2;
		ctx->vi.stChnAttr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF;
		ctx->vi.stChnAttr.u32Depth = 1;
		ctx->vi.stChnAttr.enPixelFormat = PixelFormat;
		ctx->vi.stChnAttr.enCompressMode = CompressMode;
		ctx->vi.stChnAttr.stFrameRate.s32SrcFrameRate = -1;
		ctx->vi.stChnAttr.stFrameRate.s32DstFrameRate = -1;
		ctx->vi.dstFilePath = pOutPath;
		ctx->vi.s32loopCount = s32loopCnt;
	}

	SAMPLE_COMM_VI_CreateChn(&ctx->vi);
	RK_MPI_VI_EnableChn(0, 1);

	// init ivs
	IVS_CHN_ATTR_S attr;

	memset(&attr, 0, sizeof(attr));
	// config ivs
	{
		attr.enMode = IVS_MODE_MD_OD;
		attr.u32PicWidth = 640;
		attr.u32PicHeight = 360;
		attr.enPixelFormat = RK_FMT_YUV420SP;
		attr.s32Gop = 60;
		attr.bSmearEnable = RK_FALSE;
		attr.bWeightpEnable = RK_FALSE;
		attr.bMDEnable = RK_TRUE;
		attr.s32MDInterval = 5;
		attr.bMDNightMode = RK_FALSE;
		attr.u32MDSensibility = 1;
		attr.bODEnable = RK_TRUE;
		attr.s32ODInterval = 1;
		attr.s32ODPercent = 7;
	}
	RK_MPI_IVS_CreateChn(0, &attr);
	IVS_MD_ATTR_S stMdAttr;
	memset(&stMdAttr, 0, sizeof(stMdAttr));
	s32Ret = RK_MPI_IVS_GetMdAttr(0, &stMdAttr);
	if (s32Ret)
	{
		RK_LOGE("ivs get mdattr failed:%x", s32Ret);
		while (1)
		{
			printf("ivs get mdattr failed:%x", s32Ret);
		}
	}
	stMdAttr.s32ThreshSad = 400;
	stMdAttr.s32ThreshMove = 2;
	stMdAttr.s32SwitchSad = 0;
	// update configuration
	s32Ret = RK_MPI_IVS_SetMdAttr(0, &stMdAttr);
	if (s32Ret == RK_FAILURE)
	{
		while (1)
		{
			printf("ivs set mdattr failed:%x", s32Ret);
		}
	}
	// bind vi and ivs
	vi_chn.enModId = RK_ID_VI;
	vi_chn.s32DevId = 0;
	vi_chn.s32ChnId = 1;
	ivs_chn.enModId = RK_ID_IVS;
	ivs_chn.s32DevId = 0;
	ivs_chn.s32ChnId = 0;
	s32Ret = RK_MPI_SYS_Bind(&vi_chn, &ivs_chn);
	if (s32Ret == RK_FAILURE)
	{
		while (1)
		{
			printf("ivs set mdattr failed:%x", s32Ret);
		}
	}
	pthread_create(&MD_thread, NULL, MD_result_updater, NULL);
	pthread_create(&recorder_thread, NULL, video_recorder, NULL);
	pthread_join(MD_thread, NULL);
	pthread_join(recorder_thread, NULL);
	while (g_main_run_ == 1)
	{
		sleep(100);
	}
	pthread_mutex_destroy(&mutex);
	pthread_cond_destroy(&cond_var);

	// Destory Pool
	RK_MPI_VI_DisableChn(0, 0);
	RK_MPI_VI_DisableDev(0);
	RK_MPI_IVS_DestroyChn(0);

	SAMPLE_COMM_ISP_Stop(0);

	RK_MPI_SYS_Exit();

	return 0;
}
