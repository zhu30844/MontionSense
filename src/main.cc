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

#define video_hight 1920
#define video_width 1080
#define MD_hight 640
#define MD_width 360
#define MD_area_threshold 0.3




typedef struct _rkMpiCtx
{
	SAMPLE_VI_CTX_S vi;
} SAMPLE_MPI_CTX_S;

RK_U64 TEST_COMM_GetNowUs()
{
	struct timespec time = {0, 0};
	clock_gettime(CLOCK_MONOTONIC, &time);
	return (RK_U64)time.tv_sec * 1000000 + (RK_U64)time.tv_nsec / 1000; /* microseconds */
}

static MPP_CHN_S vi_chn, ivs_chn;
int md_area_threshold = 640 * 360 * 0.3;

int main(int argc, char *argv[])
{
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
	if (s32Ret)
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
	if (s32Ret)
	{
		while (1)
		{
			printf("ivs set mdattr failed:%x", s32Ret);
		}
	}
	IVS_RESULT_INFO_S stResults;
	memset(&stResults, 0, sizeof(IVS_RESULT_INFO_S));
	while (1)
	{
		s32Ret = RK_MPI_IVS_GetResults(0, &stResults, -1);
		if (s32Ret == RK_SUCCESS)
		{
			if (stResults.s32ResultNum == 1)
			{
				printf("MD u32RectNum: %u\n", stResults.pstResults->stMdInfo.u32RectNum);
				//for (int i = 0; i < stResults.pstResults->stMdInfo.u32RectNum; i++)
				{
					if (stResults.pstResults->stMdInfo.u32Square > md_area_threshold)
					{
						printf("MD: md_area is %d, md_area_threshold is %d\n",
					         stResults.pstResults->stMdInfo.u32Square, md_area_threshold);
						/*printf("%d: [%d, %d, %d, %d]\n", i,
							   stResults.pstResults->stMdInfo.stRect[i].s32X,
							   stResults.pstResults->stMdInfo.stRect[i].s32Y,
							   stResults.pstResults->stMdInfo.stRect[i].u32Width,
							   stResults.pstResults->stMdInfo.stRect[i].u32Height);*/
					}
				}
			}
			RK_MPI_IVS_ReleaseResults(0, &stResults);
		}
		else
		{
			RK_LOGE("RK_MPI_IVS_GetResults fail %x", s32Ret);
		}
	}


	// Destory Pool
	RK_MPI_VI_DisableChn(0, 0);
	RK_MPI_VI_DisableDev(0);
	RK_MPI_IVS_DestroyChn(0);

	SAMPLE_COMM_ISP_Stop(0);

	RK_MPI_SYS_Exit();

	return 0;
}
