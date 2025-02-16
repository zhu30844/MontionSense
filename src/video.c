/*
 * Copyright 2025 ZIXUAN ZHU
 * 
 * This file is based on the original work by Rockchip Electronics Co.,Ltd. and modified by ZIXUAN ZHU.
 * 
 * Original License: BSD License
 * Modified Work License: BSD License
 * Use of this source code is governed by a BSD-style license that can be found in the LICENSE file.
 */


#include "video.h"

#define MD_area_threshold 0.3
#define VIDEO_PIPE_2 2

static MPP_CHN_S vi_chn[3], venc_chn[2], ivs_chn;
float md_area_threshold_rate = 0.25;
static int g_video_run_ = 1;
static int pipe_id_ = 0;
pthread_t frame_rate_updater_thread, rkipc_get_venc_0_thread, rkipc_get_venc_1_thread;
pthread_rwlock_t frame_rate_rwlock;
volatile int motion_detected = 0;
int video_frame_rate = 1; // 1: 1fps 30: 30fps
RK_U32 vi_video_hight = 1080;
RK_U32 vi_video_width = 1920; // 640*360 1920*1080 2304*1296
RK_U32 ivs_video_hight = 360;
RK_U32 ivs_video_width = 640;

image_addr_t image_addr;

static int motion_detecter(int chnId)
{
	int flag = MOTION_UNDETECTED;
	IVS_RESULT_INFO_S stResults;
	memset(&stResults, 0, sizeof(IVS_RESULT_INFO_S));
	RK_S32 s32Ret = RK_MPI_IVS_GetResults(chnId, &stResults, 1000);
	if (s32Ret == RK_SUCCESS)
	{
		if (stResults.s32ResultNum == 1)
		{
			// printf("MD u32RectNum: %u\n", stResults.pstResults->stMdInfo.u32RectNum);
			if (stResults.pstResults->stMdInfo.u32Square > ivs_video_hight * ivs_video_hight * md_area_threshold_rate)
			{
				flag = MOTION_DETECTED;
				// printf("MD: md_area is %d, md_area_threshold is %f\n",
				// 	   stResults.pstResults->stMdInfo.u32Square, md_area_threshold_rate);
			}
		}
		RK_MPI_IVS_ReleaseResults(0, &stResults);
		return flag;
	}
	else
	{
		printf("RK_MPI_IVS_GetResults fail %x\n", s32Ret);
		return RK_FAILURE;
	}
}

static int frame_rate_setter(int chn, int frame_rate)
{
	int ret;
	VENC_CHN_ATTR_S venc_chn_attr;
	ret = RK_MPI_VENC_GetChnAttr(chn, &venc_chn_attr);
	if (ret)
	{
		printf("ERROR: RK_MPI_VENC_GetChnAttr chn %d error! ret=%#x\n", chn, ret);
		return RK_FAILURE;
	}
	venc_chn_attr.stRcAttr.stH264Cbr.u32SrcFrameRateNum = frame_rate;
	venc_chn_attr.stRcAttr.stH264Cbr.u32Gop = (frame_rate == HIGH_FRAME_RATE ? HIGH_FRAME_RATE * 2 : LOW_FRAME_RATE * 1);
	venc_chn_attr.stRcAttr.stH264Cbr.u32SrcFrameRateDen = 1;
	ret = RK_MPI_VENC_SetChnAttr(chn, &venc_chn_attr);
	if (ret)
	{
		printf("ERROR: RK_MPI_VENC_SetChnAttr error! ret=%#x\n", ret);
		return RK_FAILURE;
	}
	return 0;
}

static void *rkipc_get_venc_0(void *arg)
{
	VENC_STREAM_S stFrame;
	int ret = 0;
	// prepare the frame
	stFrame.pstPack = (VENC_PACK_S *)malloc(sizeof(VENC_PACK_S));
	// frame rate control
	int frame_cycle_time_ms = 1000;
	long long before_time, cost_time;
	while (g_video_run_)
	{
		before_time = get_curren_time_ms();
		// get current frame rate
		pthread_rwlock_rdlock(&frame_rate_rwlock);
		frame_cycle_time_ms = 1000 / video_frame_rate;
		pthread_rwlock_unlock(&frame_rate_rwlock);
		// printf("frame_cycle_time_ms is %d\n",
		//  frame_cycle_time_ms);
		//  get the frame
		ret = RK_MPI_VENC_GetStream(0, &stFrame, 2500);
		if (ret == RK_SUCCESS)
		{
			// printf("get frame success!\n");
			void *data = RK_MPI_MB_Handle2VirAddr(stFrame.pstPack->pMbBlk);
			// write_ts_2_SD((RK_U8 *)data, stFrame.pstPack->u32Len,1);
			//  release the frame
			// write_raw((RK_U8 *)data, stFrame.pstPack->u32Len);
			if ((stFrame.pstPack->DataType.enH264EType == H264E_NALU_IDRSLICE) ||
				(stFrame.pstPack->DataType.enH264EType == H264E_NALU_ISLICE))
			{
				// printf("write key frame\n");
				write_frame_2_SD((RK_U8 *)data, stFrame.pstPack->u32Len, RK_TRUE, frame_cycle_time_ms);
				// printf("key frame sent\n");
			}
			else
			{
				write_frame_2_SD((RK_U8 *)data, stFrame.pstPack->u32Len, RK_FALSE, frame_cycle_time_ms);
				// printf("none-key frame sent\n");
			}
			// printf("write frame to SD done\n");
			ret = RK_MPI_VENC_ReleaseStream(0, &stFrame);
			if (ret != RK_SUCCESS)
				printf("RK_MPI_VENC_ReleaseStream fail %x\n", ret);
		}
		else
		{
			printf("RK_MPI_VENC_0_GetStream timeout %x\n", ret);
		}
		cost_time = get_curren_time_ms() - before_time;
		// printf("cost_time is %lld\n", cost_time);
		if ((cost_time > 0) && (cost_time < frame_cycle_time_ms))
		{
			if (frame_cycle_time_ms == 1000 / HIGH_FRAME_RATE)
			{
				usleep((frame_cycle_time_ms - cost_time) * 1000);
			}
			else
			{
				for (int i = 0; i < 100; i++)
				{
					before_time = get_curren_time_ms();
					pthread_rwlock_rdlock(&frame_rate_rwlock);
					frame_cycle_time_ms = 1000 / video_frame_rate;
					pthread_rwlock_unlock(&frame_rate_rwlock);
					if (frame_cycle_time_ms == 1000 / HIGH_FRAME_RATE)
						break;
					cost_time = get_curren_time_ms() - before_time;
					// printf("cost_time is %lld\n", cost_time);
					usleep((10 - cost_time) * 1000);
				}
				continue;
			}
		}
	}
	printf("rkipc_get_venc_0 exit\n");
	if (stFrame.pstPack)
		free(stFrame.pstPack);
	printf("rkipc_get_venc_0 exit\n");
	return 0;
}

static void *rkipc_get_venc_1(void *arg)
{
	VENC_STREAM_S stFrame;
	int ret = 0;
	FILE *fp = NULL;
	stFrame.pstPack = (VENC_PACK_S *)malloc(sizeof(VENC_PACK_S));
	while (g_video_run_)
	{
		ret = RK_MPI_VENC_GetStream(1, &stFrame, 200);
		if (ret == RK_SUCCESS)
		{
			void *data = RK_MPI_MB_Handle2VirAddr(stFrame.pstPack->pMbBlk);
			pthread_rwlock_wrlock(&image_addr.lock);
			image_addr.p = data;
			image_addr.size = stFrame.pstPack->u32Len;
			pthread_rwlock_unlock(&image_addr.lock);
			usleep(1000 * 10);
			ret = RK_MPI_VENC_ReleaseStream(1, &stFrame);
			if (ret != RK_SUCCESS)
			{
				printf("RK_MPI_VENC_ReleaseStream fail %x\n", ret);
			}
		}
		else
		{
			printf("RK_MPI_VENC_1_GetStream timeout %x\n", ret);
		}
	}
	printf("rkipc_get_venc_1 exit\n");
	if (stFrame.pstPack)
		free(stFrame.pstPack);
	if (fp)
		fclose(fp);
	return 0;
}

static void *rkipc_get_ivs_0(void *arg)
{
	int frame_cycle_time_ms = 1000 / 4;
	long long before_time, cost_time;
	while (g_video_run_ == 1)
	{
		before_time = get_curren_time_ms();
		if (motion_detecter(0) != motion_detected)
		{
			pthread_rwlock_wrlock(&frame_rate_rwlock);
			motion_detected = (motion_detected + 1) % 2;
			video_frame_rate = (motion_detected == MOTION_DETECTED
									? HIGH_FRAME_RATE
									: LOW_FRAME_RATE);
			pthread_rwlock_unlock(&frame_rate_rwlock);
			// inform sqlite

			if (frame_rate_setter(0, video_frame_rate))
			{
				printf("frame_rate_setter error\n");
			}
			else
			{
				/*printf("update video rate to %d\n",
					   motion_detected == MOTION_DETECTED
						   ? HIGH_FRAME_RATE
						   : LOW_FRAME_RATE);*/
			}
		}
		cost_time = get_curren_time_ms() - before_time;
		if ((cost_time > 0) && (cost_time < frame_cycle_time_ms))
			usleep((frame_cycle_time_ms - cost_time) * 1000);
	}

	return NULL;
}

static int rkipc_ivs_init()
{
	int ret;
	RK_BOOL smear = RK_FALSE;
	RK_BOOL weightp = RK_FALSE;
	RK_BOOL md = RK_TRUE;
	if (!smear && !weightp && !md)
	{
		printf("no pp function enabled! end\n");
		return RK_FAILURE;
	}

	// IVS
	IVS_CHN_ATTR_S attr;
	memset(&attr, 0, sizeof(attr));
	attr.enMode = IVS_MODE_MD;
	attr.u32PicWidth = vi_video_width;
	attr.u32PicHeight = vi_video_hight;
	attr.enPixelFormat = RK_FMT_YUV420SP;
	attr.bSmearEnable = smear;
	attr.bWeightpEnable = weightp;
	attr.bMDEnable = md;
	attr.s32MDInterval = 4;
	attr.bMDNightMode = RK_TRUE;
	attr.u32MDSensibility = 3;
	ret = RK_MPI_IVS_CreateChn(0, &attr);
	if (ret)
	{
		printf("ERROR: RK_MPI_IVS_CreateChn error! ret=%#x\n", ret);
		return RK_FAILURE;
	}

	IVS_MD_ATTR_S stMdAttr;
	memset(&stMdAttr, 0, sizeof(stMdAttr));
	ret = RK_MPI_IVS_GetMdAttr(0, &stMdAttr);
	if (ret)
	{
		printf("ERROR: RK_MPI_IVS_GetMdAttr error! ret=%#x\n", ret);
		return RK_FAILURE;
	}
	stMdAttr.s32ThreshSad = 40;
	stMdAttr.s32ThreshMove = 2;
	stMdAttr.s32SwitchSad = 0;
	ret = RK_MPI_IVS_SetMdAttr(0, &stMdAttr);
	if (ret)
	{
		printf("ERROR: RK_MPI_IVS_SetMdAttr error! ret=%#x\n", ret);
		return RK_FAILURE;
	}
	if (md == RK_TRUE)
		pthread_create(&frame_rate_updater_thread, NULL, rkipc_get_ivs_0, NULL);

	return RK_SUCCESS;
}

static int rkipc_aiq_init()
{
	// init rkaiq
	rk_aiq_working_mode_t hdr_mode = RK_AIQ_WORKING_MODE_NORMAL;
	RK_BOOL multi_sensor = RK_FALSE;
	const char *iq_dir = "/etc/iqfiles";
	SAMPLE_COMM_ISP_Init(0, hdr_mode, multi_sensor, iq_dir);
	SAMPLE_COMM_ISP_Run(0);
	printf("ISP init success\n");
	return RK_SUCCESS;
}

static int rkipc_vi_dev_init()
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
			return RK_FAILURE;
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
			return RK_FAILURE;
		}
		// 1-3.bind dev/pipe
		stBindPipe.u32Num = pipe_id_;
		stBindPipe.PipeId[0] = pipe_id_;
		ret = RK_MPI_VI_SetDevBindPipe(0, &stBindPipe);
		if (ret != RK_SUCCESS)
		{
			printf("RK_MPI_VI_SetDevBindPipe %x\n", ret);
			return RK_FAILURE;
		}
	}
	else
	{
		printf("RK_MPI_VI_EnableDev already\n");
	}

	return RK_SUCCESS;
}

static int rkipc_vi_dev_deinit()
{
	RK_MPI_VI_DisableDev(pipe_id_);
	printf("RK_MPI_VI_DisableDev success\n");
	return RK_SUCCESS;
}

static int rkipc_pipe_0_init()
{
	int ret;
	int rotation = 0;
	int buf_cnt = 2;
	int frame_min_i_qp = 26;
	int frame_min_qp = 28;
	int frame_max_i_qp = 51;
	int frame_max_qp = 51;
	int scalinglist = 0;

	// VI
	VI_CHN_ATTR_S vi_chn_attr;
	memset(&vi_chn_attr, 0, sizeof(vi_chn_attr));
	vi_chn_attr.stIspOpt.u32BufCount = buf_cnt;
	vi_chn_attr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF;
	vi_chn_attr.stIspOpt.stMaxSize.u32Width = vi_video_width;
	vi_chn_attr.stIspOpt.stMaxSize.u32Height = vi_video_hight;
	vi_chn_attr.stSize.u32Width = vi_video_width;
	vi_chn_attr.stSize.u32Height = vi_video_hight;
	vi_chn_attr.enPixelFormat = RK_FMT_YUV420SP;
	vi_chn_attr.u32Depth = 1;
	vi_chn_attr.enCompressMode = COMPRESS_MODE_NONE;
	vi_chn_attr.stFrameRate.s32DstFrameRate = 30;
	vi_chn_attr.stFrameRate.s32SrcFrameRate = 30;
	ret = RK_MPI_VI_SetChnAttr(pipe_id_, 0, &vi_chn_attr);
	if (ret)
	{
		printf("ERROR: create VI error! ret=%d\n", ret);
		return RK_FAILURE;
	}

	ret = RK_MPI_VI_EnableChn(pipe_id_, 0);
	if (ret)
	{
		printf("ERROR: create VI error! ret=%d\n", ret);
		return RK_FAILURE;
	}

	// VENC
	VENC_CHN_ATTR_S venc_chn_attr;
	memset(&venc_chn_attr, 0, sizeof(venc_chn_attr));
	venc_chn_attr.stVencAttr.enType = RK_VIDEO_ID_AVC;
	venc_chn_attr.stVencAttr.u32Profile = 100;
	venc_chn_attr.stRcAttr.enRcMode = VENC_RC_MODE_H264CBR;
	venc_chn_attr.stRcAttr.stH264Cbr.u32Gop = HIGH_FRAME_RATE * 2;
	venc_chn_attr.stRcAttr.stH264Cbr.u32BitRate = -1;
	venc_chn_attr.stRcAttr.stH264Cbr.u32BitRate = -1;
	venc_chn_attr.stGopAttr.enGopMode = VENC_GOPMODE_NORMALP;

	venc_chn_attr.stVencAttr.enPixelFormat = RK_FMT_YUV420SP;
	venc_chn_attr.stVencAttr.u32MaxPicWidth = vi_video_width;
	venc_chn_attr.stVencAttr.u32MaxPicHeight = vi_video_hight;
	venc_chn_attr.stVencAttr.u32PicWidth = vi_video_width;
	venc_chn_attr.stVencAttr.u32PicHeight = vi_video_hight;
	venc_chn_attr.stVencAttr.u32VirWidth = vi_video_width;
	venc_chn_attr.stVencAttr.u32VirHeight = vi_video_hight;
	venc_chn_attr.stVencAttr.u32StreamBufCnt = 4;
	venc_chn_attr.stVencAttr.u32BufSize = vi_video_width * vi_video_hight * 3 / 2;
	ret = RK_MPI_VENC_CreateChn(0, &venc_chn_attr);
	if (ret)
	{
		printf("ERROR: create VENC error! ret=%#x\n", ret);
		return RK_FAILURE;
	}
	// rk_video_reset_frame_rate(VIDEO_PIPE_0);

	ret = RK_MPI_VENC_EnableMotionDeblur(0, RK_TRUE);
	if (ret)
		printf("RK_MPI_VENC_EnableMotionDeblur error! ret=%#x\n", ret);
	ret = RK_MPI_VENC_SetMotionDeblurStrength(0, 3);
	if (ret)
		printf("RK_MPI_VENC_SetMotionDeblurStrength error! ret=%#x\n", ret);

	VENC_DEBREATHEFFECT_S debfrath_effect;
	memset(&debfrath_effect, 0, sizeof(VENC_DEBREATHEFFECT_S));

	debfrath_effect.bEnable = RK_TRUE;
	debfrath_effect.s32Strength1 = 16;
	ret = RK_MPI_VENC_SetDeBreathEffect(0, &debfrath_effect);
	if (ret)
		printf("RK_MPI_VENC_SetDeBreathEffect error! ret=%#x\n", ret);

	VENC_RC_PARAM2_S rc_param2;
	ret = RK_MPI_VENC_GetRcParam2(0, &rc_param2);
	if (ret)
		printf("RK_MPI_VENC_GetRcParam2 error! ret=%#x\n", ret);

	ret = RK_MPI_VENC_SetRcParam2(0, &rc_param2);
	if (ret)
		printf("RK_MPI_VENC_SetRcParam2 error! ret=%#x\n", ret);

	VENC_H264_QBIAS_S qbias;
	qbias.bEnable = RK_FALSE;
	qbias.u32QbiasI = 683;
	qbias.u32QbiasP = 341;
	ret = RK_MPI_VENC_SetH264Qbias(0, &qbias);
	if (ret)
		printf("RK_MPI_VENC_SetH264Qbias error! ret=%#x\n", ret);
	VENC_FILTER_S pstFilter;
	RK_MPI_VENC_GetFilter(0, &pstFilter);
	pstFilter.u32StrengthI = 0;
	pstFilter.u32StrengthP = 0;
	RK_MPI_VENC_SetFilter(0, &pstFilter);

	VENC_RC_PARAM_S venc_rc_param;
	RK_MPI_VENC_GetRcParam(0, &venc_rc_param);
	venc_rc_param.stParamH264.u32MinQp = 10;
	venc_rc_param.stParamH264.u32FrmMinIQp = frame_min_i_qp;
	venc_rc_param.stParamH264.u32FrmMinQp = frame_min_qp;
	venc_rc_param.stParamH264.u32FrmMaxIQp = frame_max_i_qp;
	venc_rc_param.stParamH264.u32FrmMaxQp = frame_max_qp;

	RK_MPI_VENC_SetRcParam(0, &venc_rc_param);

	VENC_H264_TRANS_S pstH264Trans;
	RK_MPI_VENC_GetH264Trans(0, &pstH264Trans);
	pstH264Trans.bScalingListValid = scalinglist;
	RK_MPI_VENC_SetH264Trans(0, &pstH264Trans);

	VENC_ANTI_RING_S anti_ring_s;
	RK_MPI_VENC_GetAntiRing(0, &anti_ring_s);
	anti_ring_s.u32AntiRing = 2;
	RK_MPI_VENC_SetAntiRing(0, &anti_ring_s);
	VENC_ANTI_LINE_S anti_line_s;
	RK_MPI_VENC_GetAntiLine(0, &anti_line_s);
	anti_line_s.u32AntiLine = 2;
	RK_MPI_VENC_SetAntiLine(0, &anti_line_s);
	VENC_LAMBDA_S lambds_s;
	RK_MPI_VENC_GetLambda(0, &lambds_s);
	lambds_s.u32Lambda = 4;
	RK_MPI_VENC_SetLambda(0, &lambds_s);

	VENC_CHN_REF_BUF_SHARE_S stVencChnRefBufShare;
	memset(&stVencChnRefBufShare, 0, sizeof(VENC_CHN_REF_BUF_SHARE_S));
	stVencChnRefBufShare.bEnable = RK_FALSE;
	RK_MPI_VENC_SetChnRefBufShareAttr(0, &stVencChnRefBufShare);
	RK_MPI_VENC_SetChnRotation(0, rotation/90);

	VENC_RECV_PIC_PARAM_S stRecvParam;
	memset(&stRecvParam, 0, sizeof(VENC_RECV_PIC_PARAM_S));
	stRecvParam.s32RecvPicNum = -1;
	RK_MPI_VENC_StartRecvFrame(0, &stRecvParam);
	pthread_create(&rkipc_get_venc_0_thread, NULL, rkipc_get_venc_0, NULL);
	//  bind
	vi_chn[0].enModId = RK_ID_VI;
	vi_chn[0].s32DevId = 0;
	vi_chn[0].s32ChnId = 0;
	venc_chn[0].enModId = RK_ID_VENC;
	venc_chn[0].s32DevId = 0;
	venc_chn[0].s32ChnId = 0;
	ret = RK_MPI_SYS_Bind(&vi_chn[0], &venc_chn[0]);
	if (ret)
	{
		printf("Bind VI and VENC error! ret=%#x\n", ret);
		return RK_FAILURE;
	}
	else
		printf("Bind VI and VENC success\n");

	return RK_SUCCESS;
}

static int rkipc_pipe_0_deinit()
{
	printf("Pipe 0 deinit start\n");
	pthread_join(rkipc_get_venc_0_thread, NULL);
	printf("Pipe 0: thr exit\n");
	int ret;
	// unbind
	vi_chn[0].enModId = RK_ID_VI;
	vi_chn[0].s32DevId = 0;
	vi_chn[0].s32ChnId = 0;
	venc_chn[0].enModId = RK_ID_VENC;
	venc_chn[0].s32DevId = 0;
	venc_chn[0].s32ChnId = 0;
	printf("Pipe 0: Unbind VI and VENC\n");
	ret = RK_MPI_SYS_UnBind(&vi_chn[0], &venc_chn[0]);
	if (ret)
		printf("Pipe 0: Unbind VI and VENC error! ret=%#x\n", ret);
	else
		printf("Pipe 0: Unbind VI and VENC success\n");
	// VENC
	printf("Pipe 0: StopRecvFrame\n");
	ret = RK_MPI_VENC_StopRecvFrame(0);
	if (ret)
		printf("Pipe 0:ERROR: StopRecvFrame error! ret=%#x\n", ret);
	printf("Pipe 0: Destroy VENC\n");
	ret = RK_MPI_VENC_DestroyChn(0);
	if (ret)
		printf("Pipe 0:ERROR: Destroy VENC error! ret=%#x\n", ret);
	else
		printf("Pipe 0:RK_MPI_VENC_DestroyChn success\n");
	// VI
	ret = RK_MPI_VI_DisableChn(pipe_id_, 0);
	if (ret)
		printf("Pipe 0:ERROR: Destroy VI error! ret=%#x\n", ret);
	printf("Pipe 0 deinit done\n");
	return 0;
}

static int rkipc_pipe_1_init()
{
	int ret;
	// VI
	VI_CHN_ATTR_S vi_chn_attr;
	memset(&vi_chn_attr, 0, sizeof(vi_chn_attr));
	vi_chn_attr.stIspOpt.u32BufCount = 2;
	vi_chn_attr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF;
	vi_chn_attr.stIspOpt.stMaxSize.u32Width = vi_video_width;
	vi_chn_attr.stIspOpt.stMaxSize.u32Height = vi_video_hight;
	vi_chn_attr.stSize.u32Width = vi_video_width;
	vi_chn_attr.stSize.u32Height = vi_video_hight;
	vi_chn_attr.enPixelFormat = RK_FMT_YUV420SP;
	vi_chn_attr.enCompressMode = COMPRESS_MODE_NONE;
	vi_chn_attr.stFrameRate.s32DstFrameRate = 30;
	vi_chn_attr.stFrameRate.s32SrcFrameRate = 30;
	vi_chn_attr.u32Depth = 0;
	if (1) // vo
		vi_chn_attr.u32Depth += 1;
	ret = RK_MPI_VI_SetChnAttr(pipe_id_, 1, &vi_chn_attr);
	ret |= RK_MPI_VI_EnableChn(pipe_id_, 1);
	if (ret)
	{
		printf("ERROR: create VI error! ret=%d\n", ret);
		return RK_FAILURE;
	}

	// VENC
	VENC_CHN_ATTR_S venc_chn_attr;
	memset(&venc_chn_attr, 0, sizeof(venc_chn_attr));
	venc_chn_attr.stVencAttr.enType = RK_VIDEO_ID_MJPEG;
	venc_chn_attr.stVencAttr.u32Profile = 0; // 77 66
	venc_chn_attr.stVencAttr.enPixelFormat = RK_FMT_YUV420SP;
	venc_chn_attr.stVencAttr.u32PicWidth = vi_video_width;
	venc_chn_attr.stVencAttr.u32PicHeight = vi_video_hight;
	venc_chn_attr.stVencAttr.u32VirWidth = vi_video_width;
	venc_chn_attr.stVencAttr.u32VirHeight = vi_video_hight;
	venc_chn_attr.stVencAttr.u32StreamBufCnt = 3;
	venc_chn_attr.stVencAttr.u32BufSize = vi_video_width * vi_video_hight;

	VENC_CHN_PARAM_S stParam;
	memset(&stParam, 0, sizeof(VENC_CHN_PARAM_S));
	stParam.stFrameRate.bEnable = RK_FALSE;
	stParam.stFrameRate.s32SrcFrmRateNum = 30;
	stParam.stFrameRate.s32SrcFrmRateDen = 1;
	stParam.stFrameRate.s32DstFrmRateNum = 20;
	stParam.stFrameRate.s32DstFrmRateDen = 1;
	RK_MPI_VENC_SetChnParam(1, &stParam);

	VENC_RC_PARAM_S venc_rc_param;
	memset(&venc_rc_param, 0, sizeof(VENC_RC_PARAM_S));
	RK_MPI_VENC_GetRcParam(1, &venc_rc_param);
	venc_rc_param.stParamMjpeg.u32MaxQfactor = 80;
	venc_rc_param.stParamMjpeg.u32MinQfactor = 65;
	RK_MPI_VENC_SetRcParam(1, &venc_rc_param);

	ret = RK_MPI_VENC_CreateChn(1, &venc_chn_attr);
	if (ret)
	{
		printf("ERROR: create VENC 1 error! ret=%#x\n", ret);
		return RK_FAILURE;
	}

	VENC_RECV_PIC_PARAM_S stRecvParam;
	memset(&stRecvParam, 0, sizeof(VENC_RECV_PIC_PARAM_S));
	stRecvParam.s32RecvPicNum = -1;
	ret = RK_MPI_VENC_StartRecvFrame(1, &stRecvParam);

	//  bind
	vi_chn[1].enModId = RK_ID_VI;
	vi_chn[1].s32DevId = 0;
	vi_chn[1].s32ChnId = 1;
	venc_chn[1].enModId = RK_ID_VENC;
	venc_chn[1].s32DevId = 0;
	venc_chn[1].s32ChnId = 1;
	ret = RK_MPI_SYS_Bind(&vi_chn[1], &venc_chn[1]);
	if (ret)
	{
		printf("ch 1 Bind VI and VENC error! ret=%#x\n", ret);
		return RK_FAILURE;
	}
	else
		printf("Bind VI and VENC success\n");

	if (pthread_create(&rkipc_get_venc_1_thread, NULL, rkipc_get_venc_1, NULL) != 0)
	{
		printf("create rkipc_get_venc_1_thread failed\n");
		return RK_FAILURE;
	}

	return RK_SUCCESS;
}

static int rkipc_pipe_1_deinit()
{
	printf("rkipc_pipe_1_deinit\n");
	pthread_join(rkipc_get_venc_1_thread, NULL);
	printf("rkipc_pipe_1_deinit done\n");
	int ret;
	// unbind
	vi_chn[1].enModId = RK_ID_VI;
	vi_chn[1].s32DevId = 0;
	vi_chn[1].s32ChnId = 1;
	venc_chn[1].enModId = RK_ID_VENC;
	venc_chn[1].s32DevId = 0;
	venc_chn[1].s32ChnId = 1;
	ret = RK_MPI_SYS_UnBind(&vi_chn[1], &venc_chn[1]);
	if (ret)
		printf("Pipe 1:Unbind VI and VENC error! ret=%#x\n", ret);
	else
		printf("Pipe 1:Unbind VI and VENC success\n");
	// VENC
	ret = RK_MPI_VENC_StopRecvFrame(1);
	ret |= RK_MPI_VENC_DestroyChn(1);
	if (ret)
		printf("Pipe 1:ERROR: Destroy VENC error! ret=%#x\n", ret);
	else
		printf("Pipe 1:RK_MPI_VENC_DestroyChn success\n");
	// VI
	ret = RK_MPI_VI_DisableChn(pipe_id_, 1);
	if (ret)
		printf("Pipe 1:ERROR: Destroy VI error! ret=%#x\n", ret);

	printf("rk pipe_1 deinit success\n");
	return 0;
}

static int rkipc_pipe_2_init()
{
	int ret;
	int buf_cnt = 3;
	// VI FOR MD
	VI_CHN_ATTR_S vi_chn_MD_attr;
	memset(&vi_chn_MD_attr, 0, sizeof(vi_chn_MD_attr));
	vi_chn_MD_attr.stIspOpt.u32BufCount = buf_cnt;
	vi_chn_MD_attr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF;
	vi_chn_MD_attr.stIspOpt.stMaxSize.u32Width = ivs_video_width;
	vi_chn_MD_attr.stIspOpt.stMaxSize.u32Height = ivs_video_width;
	vi_chn_MD_attr.stSize.u32Width = ivs_video_width;
	vi_chn_MD_attr.stSize.u32Height = ivs_video_hight;
	vi_chn_MD_attr.enPixelFormat = RK_FMT_YUV420SP;
	vi_chn_MD_attr.stFrameRate.s32DstFrameRate = 15;
	vi_chn_MD_attr.stFrameRate.s32SrcFrameRate = 15;
	vi_chn_MD_attr.enCompressMode = COMPRESS_MODE_NONE;
	vi_chn_MD_attr.u32Depth = 0;
	ret = RK_MPI_VI_SetChnAttr(pipe_id_, VIDEO_PIPE_2, &vi_chn_MD_attr);
	if (ret)
	{
		printf("ERROR: create VI error! ret=%d\n", ret);
		return RK_FAILURE;
	}
	ret = RK_MPI_VI_EnableChn(pipe_id_, VIDEO_PIPE_2);
	if (ret)
	{
		printf("ERROR: create VI error! ret=%d\n", ret);
		return RK_FAILURE;
	}

	ret |= rkipc_ivs_init();
	if (ret != RK_SUCCESS)
	{
		printf("rkipc_ivs_init failed\n");
		return RK_FAILURE;
	}
	// bind
	vi_chn[2].enModId = RK_ID_VI;
	vi_chn[2].s32DevId = 0;
	vi_chn[2].s32ChnId = VIDEO_PIPE_2;
	ivs_chn.enModId = RK_ID_IVS;
	ivs_chn.s32DevId = 0;
	ivs_chn.s32ChnId = 0;
	ret = RK_MPI_SYS_Bind(&vi_chn[2], &ivs_chn);
	if (ret)
	{
		printf("Bind VI and IVS error! ret=%#x\n", ret);
		return RK_FAILURE;
	}
	else
		printf("Bind VI and IVS success\n");

	pthread_rwlock_init(&frame_rate_rwlock, NULL);

	return RK_SUCCESS;
}

static int rkipc_ivs_deinit()
{
	int ret;
	ret = RK_MPI_IVS_DestroyChn(0);
	if (ret != RK_SUCCESS)
		printf("ERROR: Destroy IVS error! ret=%#x\n", ret);
	else
		printf("RK_MPI_IVS_DestroyChn success\n");
	return 0;
}

static int rkipc_pipe_2_deinit()
{
	pthread_join(frame_rate_updater_thread, NULL);
	int ret;
	// unbind
	ret = RK_MPI_SYS_UnBind(&vi_chn[2], &ivs_chn);
	if (ret)
		printf("Pipe 2:Unbind VI and IVS error! ret=%#x\n", ret);
	else
		printf("Pipe 2:Unbind VI and IVS success\n");
	rkipc_ivs_deinit();
	ret = RK_MPI_VI_DisableChn(pipe_id_, VIDEO_PIPE_2);
	if (ret)
		printf("Pipe 2:ERROR: Destroy VI error! ret=%#x\n", ret);
	pthread_rwlock_destroy(&frame_rate_rwlock);
	printf("Pipe 2:rk pipe_2 deinit success\n");
	return 0;
}

int rk_video_deinit()
{
	g_video_run_ = 0;
	SAMPLE_COMM_ISP_Stop(0);
	rkipc_pipe_2_deinit();
	rkipc_pipe_1_deinit();
	rkipc_pipe_0_deinit();
	rkipc_vi_dev_deinit();
	return 0;
}

int rk_video_init()
{
	int ret = 0;
	g_video_run_ = 1;
	pthread_rwlock_init(&image_addr.lock, NULL);
	web_send_image_init(&image_addr);
	ret = rkipc_vi_dev_init();
	printf("rk vi init done\n");
	ret |= rkipc_aiq_init();
	printf("rk aiq init done\n");
	ret |= rkipc_pipe_0_init();
	printf("rk pipe_0 init done\n");
	ret |= rkipc_pipe_1_init();
	ret |= rkipc_pipe_2_init();
	printf("rk video init done\n");
	return ret;
}