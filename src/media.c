// System headers
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// App headers
#include "motionsense/media.h"
#include "motionsense/app_ctx.h"
#include "motionsense/config.h"
#include "motionsense/frame_queue.h"
#include "motionsense/utils.h"

#include "log.h"

// Rockchip Media Process Interface (MPI) headers
#include "rk_mpi_ivs.h"
#include "rk_mpi_mb.h"
#include "rk_mpi_rgn.h"
#include "rk_mpi_sys.h"
#include "rk_mpi_venc.h"
#include "rk_mpi_vi.h"
#include "rk_aiq_user_api2_sysctl.h"

// From Rockchip example 'project'
#include "font_factory.h"
#include "osd_common.h"
#include "iconv.h"

// For socket communication with main.c
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>

#define OSD_RGN_H264 0  /* RGN handle for H264 channel */
#define OSD_RGN_MJPEG 1 /* RGN handle for MJPEG channel */

static atomic_bool s_producers_run;
static pthread_t s_h264_thr;
static pthread_t s_mjpeg_thr;
static pthread_t s_ivs_thr;
static pthread_t s_osd_thr;
static bool s_h264_started, s_mjpeg_started, s_ivs_started, s_osd_started;
static bool s_osd_inited;

// accepted client fd, -1 = no client
static int s_web_sock = -1;
// listening server fd, -1 = not initialized
static int s_web_srv_fd = -1;

/* Called once on first use: bind + listen (non-blocking accept). */
static void web_sock_srv_init(void)
{
    unlink(MJPEG_SOCK_PATH);

    int srv = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (srv < 0)
    {
        MS_LOG_ERROR("web_sock: socket: %m\n");
        return;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, MJPEG_SOCK_PATH, sizeof(addr.sun_path) - 1);

    if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        MS_LOG_ERROR("web_sock: bind: %m\n");
        close(srv);
        return;
    }
    if (listen(srv, 2) < 0)
    {
        MS_LOG_ERROR("web_sock: listen: %m\n");
        close(srv);
        return;
    }
    s_web_srv_fd = srv;
    MS_LOG_INFO("web_sock: listening on %s\n", MJPEG_SOCK_PATH);
}

static void web_sock_push(const void *data, size_t len)
{
    if (s_web_srv_fd < 0)
        return;

    /* Try to accept a (re)connecting client. */
    if (s_web_sock < 0)
    {
        int cli = accept(s_web_srv_fd, NULL, NULL);
        if (cli < 0)
            return; /* EAGAIN — no client yet, drop frame */
        MS_LOG_INFO("web_sock: Client connected\n");
        s_web_sock = cli;
    }

    uint32_t n = htonl((uint32_t)len);
    struct iovec iov[2] = {
        {&n, 4},
        {(void *)data, len},
    };
    struct msghdr msg = {.msg_iov = iov, .msg_iovlen = 2};
    if (sendmsg(s_web_sock, &msg, MSG_NOSIGNAL | MSG_DONTWAIT) < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return; /* Go is slow — drop this frame, keep connection */
        close(s_web_sock);
        s_web_sock = -1;
        MS_LOG_INFO("web_sock: Client disconnected\n");
    }
}

/* ------------------------------------------------------------------ */
/* VI device                                                          */
/* ------------------------------------------------------------------ */
static int vi_dev_init(void)
{
    VI_DEV_ATTR_S attr = {0};
    VI_DEV_BIND_PIPE_S bind_pipe = {0};

    int ret = RK_MPI_VI_GetDevAttr(VI_DEV_ID, &attr);
    if (ret == RK_ERR_VI_NOT_CONFIG)
    {
        ret = RK_MPI_VI_SetDevAttr(VI_DEV_ID, &attr);
        if (ret != RK_SUCCESS)
        {
            MS_LOG_ERROR("VI_SetDevAttr: %#x\n", ret);
            return -1;
        }
    }

    if (RK_MPI_VI_GetDevIsEnable(VI_DEV_ID) != RK_SUCCESS)
    {
        if (RK_MPI_VI_EnableDev(VI_DEV_ID) != RK_SUCCESS)
        {
            MS_LOG_ERROR("VI_EnableDev failed\n");
            return -1;
        }
        bind_pipe.u32Num = 1;
        bind_pipe.PipeId[0] = VI_PIPE_ID;
        if (RK_MPI_VI_SetDevBindPipe(VI_DEV_ID, &bind_pipe) != RK_SUCCESS)
        {
            MS_LOG_ERROR("VI_SetDevBindPipe failed\n");
            return -1;
        }
    }
    return 0;
}

static void vi_dev_deinit(void)
{
    RK_MPI_VI_DisableDev(VI_DEV_ID);
}

/* ------------------------------------------------------------------ */
/* ISP (rkaiq) — direct uAPI2 calls                                   */
/* ------------------------------------------------------------------ */
static rk_aiq_sys_ctx_t *s_aiq_ctx;

static int aiq_init(void)
{
    rk_aiq_working_mode_t mode = RK_AIQ_WORKING_MODE_NORMAL;
    setenv("HDR_MODE", "0", 1);

    rk_aiq_static_info_t info;
    if (rk_aiq_uapi2_sysctl_enumStaticMetasByPhyId(0, &info) != 0)
    {
        MS_LOG_ERROR("aiq enumStaticMetas failed\n");
        return -1;
    }
    rk_aiq_uapi2_sysctl_preInit_devBufCnt(info.sensor_info.sensor_name, "rkraw_rx", 2);

    s_aiq_ctx = rk_aiq_uapi2_sysctl_init(info.sensor_info.sensor_name,
                                         CFG_ISP_IQ_DIR, NULL, NULL);
    if (!s_aiq_ctx)
    {
        MS_LOG_ERROR("aiq sysctl_init failed\n");
        return -1;
    }
    if (rk_aiq_uapi2_sysctl_prepare(s_aiq_ctx, 0, 0, mode) != 0)
    {
        MS_LOG_ERROR("aiq sysctl_prepare failed\n");
        rk_aiq_uapi2_sysctl_deinit(s_aiq_ctx);
        s_aiq_ctx = NULL;
        return -1;
    }

    if (rk_aiq_uapi2_sysctl_start(s_aiq_ctx) != 0)
    {
        MS_LOG_ERROR("aiq sysctl_start failed\n");
        rk_aiq_uapi2_sysctl_deinit(s_aiq_ctx);
        s_aiq_ctx = NULL;
        return -1;
    }
    return 0;
}

static void aiq_stop(void)
{
    if (!s_aiq_ctx)
        return;
    rk_aiq_uapi2_sysctl_stop(s_aiq_ctx, false);
    rk_aiq_uapi2_sysctl_deinit(s_aiq_ctx);
    s_aiq_ctx = NULL;
}

static int venc_set_framerate(int chn, int fps)
{
    VENC_CHN_ATTR_S attr;
    int ret = RK_MPI_VENC_GetChnAttr(chn, &attr);
    if (ret != RK_SUCCESS)
    {
        MS_LOG_ERROR("VENC_GetChnAttr(%d) = %#x\n", chn, ret);
        return -1;
    }
    attr.stRcAttr.stH264Cbr.u32BitRate = g_cfg.h264.bitrate_kbps;
    attr.stRcAttr.stH264Cbr.u32SrcFrameRateNum = (uint32_t)fps;
    attr.stRcAttr.stH264Cbr.u32SrcFrameRateDen = 1;
    attr.stRcAttr.stH264Cbr.u32Gop = (uint32_t)g_cfg.h264.gop;
    ret = RK_MPI_VENC_SetChnAttr(chn, &attr);
    if (ret != RK_SUCCESS)
    {
        MS_LOG_ERROR("VENC_SetChnAttr(%d) = %#x\n", chn, ret);
        return -1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* H264 main record stream                                            */
/* ------------------------------------------------------------------ */
static int record_pipeline_init(void)
{
    VI_CHN_ATTR_S vi = {0};
    VENC_CHN_ATTR_S ve = {0};
    // vi configured
    {
        vi.stIspOpt.u32BufCount = 2;
        vi.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF;
        vi.stIspOpt.stMaxSize.u32Width = g_cfg.h264.width;
        vi.stIspOpt.stMaxSize.u32Height = g_cfg.h264.height;
        vi.stSize.u32Width = g_cfg.h264.width;
        vi.stSize.u32Height = g_cfg.h264.height;
        vi.enPixelFormat = RK_FMT_YUV420SP;
        vi.u32Depth = 1;
        vi.enCompressMode = COMPRESS_MODE_NONE;
        vi.stFrameRate.s32SrcFrameRate = g_cfg.h264.fps;
        vi.stFrameRate.s32DstFrameRate = g_cfg.h264.fps;
    }
    if (RK_MPI_VI_SetChnAttr(VI_DEV_ID, VENC_CHN_H264, &vi) != RK_SUCCESS ||
        RK_MPI_VI_EnableChn(VI_DEV_ID, VENC_CHN_H264) != RK_SUCCESS)
    {
        MS_LOG_ERROR("VI chn 0 failed\n");
        return -1;
    }

    // venc create
    {
        ve.stVencAttr.enType = RK_VIDEO_ID_AVC;
        ve.stVencAttr.u32Profile = 100;
        ve.stVencAttr.enPixelFormat = RK_FMT_YUV420SP;
        ve.stVencAttr.u32MaxPicWidth = g_cfg.h264.width;
        ve.stVencAttr.u32MaxPicHeight = g_cfg.h264.height;
        ve.stVencAttr.u32PicWidth = g_cfg.h264.width;
        ve.stVencAttr.u32PicHeight = g_cfg.h264.height;
        ve.stVencAttr.u32VirWidth = g_cfg.h264.width;
        ve.stVencAttr.u32VirHeight = g_cfg.h264.height;
        ve.stVencAttr.u32StreamBufCnt = 4;
        ve.stVencAttr.u32BufSize = g_cfg.h264.width * g_cfg.h264.height * 3 / 2;
        ve.stRcAttr.enRcMode = VENC_RC_MODE_H264CBR;
        ve.stRcAttr.stH264Cbr.u32Gop = (uint32_t)g_cfg.h264.gop;
        ve.stRcAttr.stH264Cbr.u32BitRate = g_cfg.h264.bitrate_kbps;
        ve.stRcAttr.stH264Cbr.u32SrcFrameRateNum = (uint32_t)g_cfg.h264.fps;
        ve.stRcAttr.stH264Cbr.u32SrcFrameRateDen = 1;
        ve.stGopAttr.enGopMode = VENC_GOPMODE_NORMALP;
    }
    if (RK_MPI_VENC_CreateChn(VENC_CHN_H264, &ve) != RK_SUCCESS)
    {
        MS_LOG_ERROR("VENC 0 create failed\n");
        return -1;
    }

    RK_MPI_VENC_EnableMotionDeblur(VENC_CHN_H264, RK_TRUE);
    RK_MPI_VENC_SetMotionDeblurStrength(VENC_CHN_H264, 3);

    VENC_DEBREATHEFFECT_S debrth = {.bEnable = RK_TRUE, .s32Strength1 = 16};
    RK_MPI_VENC_SetDeBreathEffect(VENC_CHN_H264, &debrth);

    VENC_RC_PARAM_S rcp;
    RK_MPI_VENC_GetRcParam(VENC_CHN_H264, &rcp);
    {
        rcp.stParamH264.u32MinQp = 10;
        rcp.stParamH264.u32FrmMinIQp = 26;
        rcp.stParamH264.u32FrmMinQp = 28;
        rcp.stParamH264.u32FrmMaxIQp = 51;
        rcp.stParamH264.u32FrmMaxQp = 51;
    }
    RK_MPI_VENC_SetRcParam(VENC_CHN_H264, &rcp);

    VENC_ANTI_RING_S ring;
    RK_MPI_VENC_GetAntiRing(VENC_CHN_H264, &ring);
    ring.u32AntiRing = 2;
    RK_MPI_VENC_SetAntiRing(VENC_CHN_H264, &ring);
    VENC_ANTI_LINE_S line;
    RK_MPI_VENC_GetAntiLine(VENC_CHN_H264, &line);
    line.u32AntiLine = 2;
    RK_MPI_VENC_SetAntiLine(VENC_CHN_H264, &line);
    VENC_LAMBDA_S lambda;
    RK_MPI_VENC_GetLambda(VENC_CHN_H264, &lambda);
    lambda.u32Lambda = 4;
    RK_MPI_VENC_SetLambda(VENC_CHN_H264, &lambda);

    VENC_FILTER_S flt;
    RK_MPI_VENC_GetFilter(VENC_CHN_H264, &flt);
    flt.u32StrengthI = 0;
    flt.u32StrengthP = 0;
    RK_MPI_VENC_SetFilter(VENC_CHN_H264, &flt);

    VENC_RECV_PIC_PARAM_S recv = {.s32RecvPicNum = -1};
    RK_MPI_VENC_StartRecvFrame(VENC_CHN_H264, &recv);

    return 0;
}

static void record_pipeline_deinit(void)
{
    RK_MPI_VENC_StopRecvFrame(VENC_CHN_H264);
    RK_MPI_VENC_DestroyChn(VENC_CHN_H264);
    RK_MPI_VI_DisableChn(VI_DEV_ID, VENC_CHN_H264);
}

/* ------------------------------------------------------------------ */
/* IVS motion detection                                               */
/* ------------------------------------------------------------------ */
static int motion_pipeline_init(void)
{
    /* VI_PHY_CHN_BYPATH_SAMPLING (Chn:4) requires VI_PHY_CHN_BYPATH (Chn:2)
     * to be started first — kernel returns "no start" otherwise.
     * Bypass path runs at h264/2 resolution; 4x4 sampling produces /4 of that. */
    VI_CHN_ATTR_S vi_bypass = {0};
    vi_bypass.stIspOpt.u32BufCount = 2;
    vi_bypass.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF;
    vi_bypass.stIspOpt.stMaxSize.u32Width = g_cfg.h264.width / 2;
    vi_bypass.stIspOpt.stMaxSize.u32Height = g_cfg.h264.height / 2;
    vi_bypass.stSize.u32Width = g_cfg.h264.width / 2;
    vi_bypass.stSize.u32Height = g_cfg.h264.height / 2;
    vi_bypass.enPixelFormat = RK_FMT_YUV420SP;
    vi_bypass.stFrameRate.s32SrcFrameRate = g_cfg.ivs.fps;
    vi_bypass.stFrameRate.s32DstFrameRate = g_cfg.ivs.fps;
    vi_bypass.enCompressMode = COMPRESS_MODE_NONE;
    if (RK_MPI_VI_SetChnAttr(VI_PIPE_ID, VI_PHY_CHN_BYPATH, &vi_bypass) != RK_SUCCESS ||
        RK_MPI_VI_EnableChn(VI_PIPE_ID, VI_PHY_CHN_BYPATH) != RK_SUCCESS)
    {
        MS_LOG_ERROR("VI VI_PHY_CHN_BYPATH failed\n");
        return -1;
    }

    VI_CHN_ATTR_S vi = {0};
    {
        vi.stIspOpt.u32BufCount = 3;
        vi.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF;
        vi.stIspOpt.stMaxSize.u32Width = g_cfg.ivs.width;
        vi.stIspOpt.stMaxSize.u32Height = g_cfg.ivs.height;
        vi.stSize.u32Width = g_cfg.ivs.width;
        vi.stSize.u32Height = g_cfg.ivs.height;
        vi.enPixelFormat = RK_FMT_YUV420SP;
        vi.stFrameRate.s32SrcFrameRate = g_cfg.ivs.fps;
        vi.stFrameRate.s32DstFrameRate = g_cfg.ivs.fps;
        vi.enCompressMode = COMPRESS_MODE_NONE;
    }
    if (RK_MPI_VI_SetChnAttr(VI_PIPE_ID, VI_PHY_CHN_BYPATH_SAMPLING, &vi) != RK_SUCCESS ||
        RK_MPI_VI_EnableChn(VI_PIPE_ID, VI_PHY_CHN_BYPATH_SAMPLING) != RK_SUCCESS)
    {
        RK_MPI_VI_DisableChn(VI_PIPE_ID, VI_PHY_CHN_BYPATH);
        MS_LOG_ERROR("VI VI_PHY_CHN_BYPATH_SAMPLING failed\n");
        return -1;
    }
    // IVS channel 0
    IVS_CHN_ATTR_S ivs = {0};
    {
        ivs.enMode = IVS_MODE_MD;
        ivs.u32PicWidth = g_cfg.h264.width;
        ivs.u32PicHeight = g_cfg.h264.height;
        ivs.enPixelFormat = RK_FMT_YUV420SP;
        ivs.bMDEnable = RK_TRUE;
        ivs.s32MDInterval = g_cfg.ivs.md_interval;
        // ivs.bMDNightMode     = g_cfg.ivs.md_night_mode ? RK_TRUE : RK_FALSE;
        ivs.u32MDSensibility = (uint32_t)g_cfg.ivs.md_sensibility;
    }
    if (RK_MPI_IVS_CreateChn(IVS_CHN_ID, &ivs) != RK_SUCCESS)
    {
        MS_LOG_ERROR("Create IVS_CHN_ID failed\n");
        return -1;
    }
    IVS_MD_ATTR_S md = {0};
    RK_MPI_IVS_GetMdAttr(IVS_CHN_ID, &md);
    {
        md.s32ThreshSad = g_cfg.ivs.thresh_sad;
        md.s32ThreshMove = g_cfg.ivs.thresh_move;
        md.s32SwitchSad = 0;
    }
    RK_MPI_IVS_SetMdAttr(IVS_CHN_ID, &md);

    // Bind VI to IVS
    if (RK_MPI_SYS_Bind(
            &(MPP_CHN_S){RK_ID_VI, VI_PIPE_ID, VI_PHY_CHN_BYPATH_SAMPLING},
            &(MPP_CHN_S){RK_ID_IVS, IVS_DEV_ID, IVS_CHN_ID}) != RK_SUCCESS)
    {
        MS_LOG_ERROR("bind VI-IVS failed\n");
        return -1;
    }
    return 0;
}

static void motion_pipeline_deinit(void)
{
    RK_MPI_SYS_UnBind(
        &(MPP_CHN_S){RK_ID_VI, VI_PIPE_ID, VI_PHY_CHN_BYPATH_SAMPLING},
        &(MPP_CHN_S){RK_ID_IVS, IVS_DEV_ID, IVS_CHN_ID});
    RK_MPI_IVS_DestroyChn(IVS_CHN_ID);
    RK_MPI_VI_DisableChn(VI_PIPE_ID, VI_PHY_CHN_BYPATH_SAMPLING);
    RK_MPI_VI_DisableChn(VI_PIPE_ID, VI_PHY_CHN_BYPATH);
}

/* ------------------------------------------------------------------ */
/* MJPEG preview                                                      */
/* ------------------------------------------------------------------ */
static int stream_pipeline_init(void)
{
    VI_CHN_ATTR_S vi = {0};
    VENC_CHN_ATTR_S ve = {0};

    {
        vi.stIspOpt.u32BufCount = 2;
        vi.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF;
        vi.stIspOpt.stMaxSize.u32Width = g_cfg.mjpeg.width;
        vi.stIspOpt.stMaxSize.u32Height = g_cfg.mjpeg.height;
        vi.stSize.u32Width = g_cfg.mjpeg.width;
        vi.stSize.u32Height = g_cfg.mjpeg.height;
        vi.enPixelFormat = RK_FMT_YUV420SP;
        vi.enCompressMode = COMPRESS_MODE_NONE;
        vi.u32Depth = 1;
        vi.stFrameRate.s32SrcFrameRate = g_cfg.h264.fps;
        vi.stFrameRate.s32DstFrameRate = g_cfg.h264.fps;
    }
    if (RK_MPI_VI_SetChnAttr(VI_PIPE_ID, VI_PHY_CHN_SELF, &vi) != RK_SUCCESS ||
        RK_MPI_VI_EnableChn(VI_PIPE_ID, VI_PHY_CHN_SELF) != RK_SUCCESS)
    {
        MS_LOG_ERROR("VI VI_PHY_CHN_SELF failed\n");
        return -1;
    }

    {
        ve.stVencAttr.enType = RK_VIDEO_ID_MJPEG;
        ve.stVencAttr.enPixelFormat = RK_FMT_YUV420SP;
        ve.stVencAttr.u32PicWidth = g_cfg.mjpeg.width;
        ve.stVencAttr.u32PicHeight = g_cfg.mjpeg.height;
        ve.stVencAttr.u32VirWidth = g_cfg.mjpeg.width;
        ve.stVencAttr.u32VirHeight = g_cfg.mjpeg.height;
        ve.stVencAttr.u32StreamBufCnt = 3;
        ve.stVencAttr.u32BufSize = g_cfg.mjpeg.width * g_cfg.mjpeg.height;
    }

    VENC_CHN_PARAM_S param = {0};
    {
        param.stFrameRate.bEnable = RK_TRUE;
        param.stFrameRate.s32SrcFrmRateNum = g_cfg.h264.fps;
        param.stFrameRate.s32SrcFrmRateDen = 1;
        param.stFrameRate.s32DstFrmRateNum = g_cfg.mjpeg.fps;
        param.stFrameRate.s32DstFrmRateDen = 1;
    }
    RK_MPI_VENC_SetChnParam(VENC_CHN_MJPEG, &param);

    VENC_RC_PARAM_S rcp;
    RK_MPI_VENC_GetRcParam(VENC_CHN_MJPEG, &rcp);
    {
        rcp.stParamMjpeg.u32MaxQfactor = g_cfg.mjpeg.qfactor_max;
        rcp.stParamMjpeg.u32MinQfactor = g_cfg.mjpeg.qfactor_min;
    }
    RK_MPI_VENC_SetRcParam(VENC_CHN_MJPEG, &rcp);

    if (RK_MPI_VENC_CreateChn(VENC_CHN_MJPEG, &ve) != RK_SUCCESS)
    {
        MS_LOG_ERROR("VENC MJPEG preview create failed\n");
        return -1;
    }

    VENC_RECV_PIC_PARAM_S recv = {.s32RecvPicNum = -1};
    RK_MPI_VENC_StartRecvFrame(VENC_CHN_MJPEG, &recv);

    if (RK_MPI_SYS_Bind(
            &(MPP_CHN_S){RK_ID_VI, VI_PIPE_ID, VI_PHY_CHN_SELF},
            &(MPP_CHN_S){RK_ID_VENC, VENC_DEV_ID, VENC_CHN_MJPEG}) != RK_SUCCESS)
    {
        MS_LOG_ERROR("bind VI-VENC failed\n");
        return -1;
    }
    return 0;
}

static void stream_pipeline_deinit(void)
{
    RK_MPI_SYS_UnBind(
        &(MPP_CHN_S){RK_ID_VI, VI_PIPE_ID, VI_PHY_CHN_SELF},
        &(MPP_CHN_S){RK_ID_VENC, VENC_DEV_ID, VENC_CHN_MJPEG});
    RK_MPI_VENC_StopRecvFrame(VENC_CHN_MJPEG);
    RK_MPI_VENC_DestroyChn(VENC_CHN_MJPEG);
    RK_MPI_VI_DisableChn(VI_PIPE_ID, VI_PHY_CHN_SELF);
}

/* ------------------------------------------------------------------ */
/* Producer threads                                                   */
/* ------------------------------------------------------------------ */

static void *h264_thread(void *arg)
{
    app_ctx_t *ctx = arg;
    VENC_STREAM_S venc_frame = {0};
    VIDEO_FRAME_INFO_S vi_frame = {0};
    venc_frame.pstPack = malloc(sizeof(VENC_PACK_S));
    if (!venc_frame.pstPack)
        return NULL;

    /* Variable-rate time-lapse: the sensor runs at a fixed rate; we downsample to
     * fps_high while moving / fps_low while idle, and rewrite each kept frame's PTS
     * so that:
     *   motion : timeline advances by the real elapsed time  → real-time playback
     *   idle   : advances by real_elapsed × fps_low/fps_high  → compressed by
     *            fps_high/fps_low (time-lapse); playback frame rate stays fps_high.
     * Everything is derived from the hardware PTS, so it stays correct even if the
     * sensor under-runs (e.g. 27 fps instead of 30). See docs/adaptive-timelapse-pts.md. */
    int64_t pts          = 0;    // synthesized timeline PTS (us), fed to the encoder
    int64_t last_kept_hw = -1;   // hw PTS (us) of the previous *kept* frame
    int64_t next_keep_hw = 0;    // hw PTS deadline for the next keep (rate limiter)
    int     prev_motion_state = -1;

    while (atomic_load(&s_producers_run))
    {
        if (RK_MPI_VI_GetChnFrame(VI_PIPE_ID, VENC_CHN_H264, &vi_frame, 2000) != RK_SUCCESS)
            continue;

        int64_t hw_pts = (int64_t)vi_frame.stVFrame.u64PTS;
        int motion_state = atomic_load(&ctx->motion_state);

        int fps_high = g_cfg.ivs.fps_high < 1 ? 1 : g_cfg.ivs.fps_high;
        int fps_low  = g_cfg.ivs.fps_low  < 1 ? 1 : g_cfg.ivs.fps_low;
        int target_fps = (motion_state == MOTION_DETECTED) ? fps_high : fps_low;
        int64_t keep_interval = 1000000 / target_fps;

        int transition = (motion_state != prev_motion_state);
        prev_motion_state = motion_state;

        /* The encoder's CBR SrcFrameRate is left constant at fps_high (set once at
         * startup): the rewritten PTS already presents fps_high to the rate control,
         * so it needs no per-transition retune. Experiment confirmed a lower declared
         * fps only inflates idle segments (~1.5x) with no benefit, and per-transition
         * SetChnAttr conflicts with the VENC guide §6.7. See docs/adaptive-timelapse-pts.md. */

        /* Keep gate: a hw-PTS rate limiter that downsamples the sensor to target_fps.
         * Always keep the first frame and the first frame after a mode switch. */
        if (last_kept_hw >= 0 && !transition)
        {
            if (hw_pts < next_keep_hw)
            {
                RK_MPI_VI_ReleaseChnFrame(VI_PIPE_ID, VENC_CHN_H264, &vi_frame);
                continue;   // drop — do not advance pts / last_kept_hw
            }
            next_keep_hw += keep_interval;
            if (next_keep_hw <= hw_pts)          // fell behind (long gap / rate change)
                next_keep_hw = hw_pts + keep_interval;
        }
        else
        {
            next_keep_hw = hw_pts + keep_interval;
        }

        /* Advance the synthesized timeline for this kept frame. */
        if (last_kept_hw < 0 || transition)
            pts += 1000000 / fps_high;                                   // nominal step, no boundary jump
        else if (motion_state == MOTION_DETECTED)
            pts += hw_pts - last_kept_hw;                                // real time
        else
            pts += (hw_pts - last_kept_hw) * fps_low / fps_high;         // time-lapse
        last_kept_hw = hw_pts;

        vi_frame.stVFrame.u64PTS = (uint64_t)pts;

        RK_MPI_VENC_SendFrame(VENC_CHN_H264, &vi_frame, -1);

        if (RK_MPI_VENC_GetStream(VENC_CHN_H264, &venc_frame, 2000) == RK_SUCCESS)
        {
            bool is_key = (venc_frame.pstPack->DataType.enH264EType == H264E_NALU_IDRSLICE) ||
                          (venc_frame.pstPack->DataType.enH264EType == H264E_NALU_ISLICE);
            void *p = RK_MPI_MB_Handle2VirAddr(venc_frame.pstPack->pMbBlk);
            fq_push(ctx->vq, p, venc_frame.pstPack->u32Len, is_key,
                    (int64_t)venc_frame.pstPack->u64PTS);
            RK_MPI_VENC_ReleaseStream(VENC_CHN_H264, &venc_frame);
        }

        RK_MPI_VI_ReleaseChnFrame(VI_PIPE_ID, VENC_CHN_H264, &vi_frame);
    }

    free(venc_frame.pstPack);
    return NULL;
}

static void *mjpeg_thread(void *arg)
{
    (void)arg;
    VENC_STREAM_S frame = {0};
    frame.pstPack = malloc(sizeof(VENC_PACK_S));
    if (!frame.pstPack)
        return NULL;

    while (atomic_load(&s_producers_run))
    {
        int ret = RK_MPI_VENC_GetStream(VENC_CHN_MJPEG, &frame, 0);
        if (ret != RK_SUCCESS)
            continue;
        void *p = RK_MPI_MB_Handle2VirAddr(frame.pstPack->pMbBlk);
        web_sock_push(p, frame.pstPack->u32Len);
        RK_MPI_VENC_ReleaseStream(VENC_CHN_MJPEG, &frame);
    }
    free(frame.pstPack);
    return NULL;
}

static void *ivs_thread(void *arg)
{
    app_ctx_t *ctx = arg;
    const long long cycle_ms = g_cfg.ivs.poll_ms;
    const uint32_t md_threshold =
        (uint32_t)((float)(g_cfg.ivs.width * g_cfg.ivs.height) * g_cfg.ivs.area_ratio);

    while (atomic_load(&s_producers_run))
    {
        long long t0 = now_ms();

        IVS_RESULT_INFO_S res = {0};
        if (RK_MPI_IVS_GetResults(0, &res, 1000) == RK_SUCCESS)
        {
            int state = MOTION_UNDETECTED;
            if (res.s32ResultNum == 1 &&
                res.pstResults->stMdInfo.u32Square > md_threshold)
            {
                state = MOTION_DETECTED;
            }
            int prev = atomic_load(&ctx->motion_state);
            if (state != prev)
            {
                int fps = state == MOTION_DETECTED ? g_cfg.ivs.fps_high : g_cfg.ivs.fps_low;
                atomic_store(&ctx->motion_state, state);
                atomic_store(&ctx->current_fps, fps);
                MS_LOG_INFO("motion %s, fps -> %d\n",
                         state == MOTION_DETECTED ? "START" : "STOP", fps);
            }
            RK_MPI_IVS_ReleaseResults(0, &res);
        }

        long long elapsed = now_ms() - t0;
        if (elapsed < cycle_ms)
            usleep((cycle_ms - elapsed) * 1000);
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* OSD timestamp overlay                                               */
/*                                                                     */
/* One OVERLAY_RGN per VENC channel (H264=0, MJPEG=1).                */
/* The osd_thread re-renders and uploads a new ARGB8888 bitmap once    */
/* per second. Region size is fixed to the rendered text width at      */
/* init time; width never changes for a HH:MM:SS string.              */
/* ------------------------------------------------------------------ */

/* Render "HH:MM:SS  YYYY-MM-DD" into wch[]. */
static void osd_get_time_wch(wchar_t *wch, int max_wch)
{
    char buf[64];
    time_t t = time(NULL);
    strftime(buf, sizeof(buf), "%H:%M:%S  %Y-%m-%d", localtime(&t));

    size_t src_len = strlen(buf);
    size_t dst_len = (size_t)max_wch * sizeof(wchar_t);
    char *src = buf;
    char *dst = (char *)wch;
    iconv_t cd = iconv_open("WCHAR_T", "UTF-8");
    if (cd == (iconv_t)-1)
        return;
    iconv(cd, &src, &src_len, &dst, &dst_len);
    iconv_close(cd);
    wch[(max_wch * sizeof(wchar_t) - dst_len) / sizeof(wchar_t)] = L'\0';
}

/* Create one OVERLAY_RGN and attach it to the given VENC channel. */
static int osd_rgn_create(RGN_HANDLE h, int venc_chn, int w, int height)
{
    RGN_ATTR_S attr = {0};
    {
        attr.enType = OVERLAY_RGN;
        attr.unAttr.stOverlay.enPixelFmt = RK_FMT_ARGB8888;
        attr.unAttr.stOverlay.u32CanvasNum = 2;
        attr.unAttr.stOverlay.stSize.u32Width = (RK_U32)w;
        attr.unAttr.stOverlay.stSize.u32Height = (RK_U32)height;
    }
    int ret = RK_MPI_RGN_Create(h, &attr);
    if (ret != RK_SUCCESS)
    {
        MS_LOG_ERROR("RGN_Create(%d) = %#x\n", h, ret);
        return -1;
    }

    int m = g_cfg.osd.margin;
    int x, y;
    switch (g_cfg.osd.position)
    {
    case OSD_POS_TOP_LEFT:
        x = UPALIGNTO16(m);
        y = UPALIGNTO16(m);
        break;
    case OSD_POS_TOP_RIGHT:
        x = UPALIGNTO16((int)g_cfg.h264.width - w - m);
        y = UPALIGNTO16(m);
        break;
    case OSD_POS_BOTTOM_LEFT:
        x = UPALIGNTO16(m);
        y = UPALIGNTO16((int)g_cfg.h264.height - height - m);
        break;
    case OSD_POS_BOTTOM_RIGHT:
    default:
        x = UPALIGNTO16((int)g_cfg.h264.width - w - m);
        y = UPALIGNTO16((int)g_cfg.h264.height - height - m);
        break;
    }

    MPP_CHN_S chn = {.enModId = RK_ID_VENC, .s32DevId = 0, .s32ChnId = venc_chn};
    RGN_CHN_ATTR_S cattr = {0};
    {
        cattr.bShow = RK_TRUE;
        cattr.enType = OVERLAY_RGN;
        cattr.unChnAttr.stOverlayChn.stPoint.s32X = x;
        cattr.unChnAttr.stOverlayChn.stPoint.s32Y = y;
        cattr.unChnAttr.stOverlayChn.u32BgAlpha = 0;
        cattr.unChnAttr.stOverlayChn.u32FgAlpha = 255;
        cattr.unChnAttr.stOverlayChn.u32Layer = h;
    }
    ret = RK_MPI_RGN_AttachToChn(h, &chn, &cattr);
    if (ret != RK_SUCCESS)
    {
        MS_LOG_ERROR("RGN_AttachToChn(%d->venc%d) = %#x\n", h, venc_chn, ret);
        RK_MPI_RGN_Destroy(h);
        return -1;
    }
    return 0;
}

static void osd_rgn_destroy(RGN_HANDLE h, int venc_chn)
{
    MPP_CHN_S chn = {.enModId = RK_ID_VENC, .s32DevId = 0, .s32ChnId = venc_chn};
    RK_MPI_RGN_DetachFromChn(h, &chn);
    RK_MPI_RGN_Destroy(h);
}

/* Upload bitmap to both handles. */
static void osd_upload(RGN_HANDLE h, unsigned char *buf, int w, int height)
{
    BITMAP_S bmp = {
        .enPixelFormat = RK_FMT_ARGB8888,
        .u32Width = (RK_U32)w,
        .u32Height = (RK_U32)height,
        .pData = buf,
    };
    int ret = RK_MPI_RGN_SetBitMap(h, &bmp);
    if (ret != RK_SUCCESS)
        MS_LOG_ERROR("RGN_SetBitMap(%d) = %#x\n", h, ret);
}

static int s_osd_w;
static int s_osd_h;

static void *osd_thread(void *arg)
{
    (void)arg;
    wchar_t wch[MAX_WCH_BYTE];
    int last_sec = -1;

    size_t bufsz = (size_t)(s_osd_w * s_osd_h * 4);
    unsigned char *buf = malloc(bufsz);
    if (!buf)
        return NULL;

    while (atomic_load(&s_producers_run))
    {
        time_t t = time(NULL);
        struct tm *tm = localtime(&t);
        if (tm->tm_sec == last_sec)
        {
            usleep(50 * 1000);
            continue;
        }
        last_sec = tm->tm_sec;

        osd_get_time_wch(wch, MAX_WCH_BYTE);
        memset(buf, 0, bufsz);
        draw_argb8888_text(buf, s_osd_w, s_osd_h, wch);

        osd_upload(OSD_RGN_H264, buf, s_osd_w, s_osd_h);
        osd_upload(OSD_RGN_MJPEG, buf, s_osd_w, s_osd_h);
    }

    free(buf);
    return NULL;
}

static int osd_init(void)
{
    if (create_font(CFG_OSD_FONT_PATH, g_cfg.osd.font_size) != 0)
    {
        MS_LOG_ERROR("create_font failed — OSD disabled\n");
        return -1;
    }
    set_font_color(0xFFFFFF); /* white */

    /* Measure width of the fixed-format timestamp string. */
    wchar_t wch[MAX_WCH_BYTE];
    osd_get_time_wch(wch, MAX_WCH_BYTE);
    s_osd_w = UPALIGNTO16(wstr_get_actual_advance_x(wch));
    s_osd_h = UPALIGNTO16(g_cfg.osd.font_size);

    if (osd_rgn_create(OSD_RGN_H264, VENC_CHN_H264, s_osd_w, s_osd_h) != 0)
        return -1;
    if (osd_rgn_create(OSD_RGN_MJPEG, VENC_CHN_MJPEG, s_osd_w, s_osd_h) != 0)
    {
        osd_rgn_destroy(OSD_RGN_H264, VENC_CHN_H264);
        return -1;
    }

    /* Render and upload the first frame before the thread starts. */
    size_t bufsz = (size_t)(s_osd_w * s_osd_h * 4);
    unsigned char *buf = calloc(1, bufsz);
    if (buf)
    {
        osd_get_time_wch(wch, MAX_WCH_BYTE);
        draw_argb8888_text(buf, s_osd_w, s_osd_h, wch);
        osd_upload(OSD_RGN_H264, buf, s_osd_w, s_osd_h);
        osd_upload(OSD_RGN_MJPEG, buf, s_osd_w, s_osd_h);
        free(buf);
    }

    MS_LOG_INFO("OSD up (%dx%d)\n", s_osd_w, s_osd_h);
    return 0;
}

static void osd_deinit(void)
{
    osd_rgn_destroy(OSD_RGN_H264, VENC_CHN_H264);
    osd_rgn_destroy(OSD_RGN_MJPEG, VENC_CHN_MJPEG);
    destroy_font();
    MS_LOG_INFO("OSD down\n");
}

int media_init(app_ctx_t *ctx)
{
    if (RK_MPI_SYS_Init() != RK_SUCCESS)
    {
        MS_LOG_ERROR("RK_MPI_SYS_Init failed\n");
        return -1;
    }
    if (vi_dev_init() != 0)
        goto fail_sys;
    if (aiq_init() != 0)
        goto fail_vi;
    if (record_pipeline_init() != 0)
        goto fail_aiq;
    if (stream_pipeline_init() != 0)
        goto fail_pipe0;
    if (motion_pipeline_init() != 0)
        goto fail_pipe1;

    /* Encoder CBR SrcFrameRate is fixed at fps_high for the whole run: the rewritten
     * PTS always presents fps_high to the rate control regardless of motion state,
     * so this is set once and never retuned (see h264_thread). */
    venc_set_framerate(VENC_CHN_H264, g_cfg.ivs.fps_high < 1 ? 1 : g_cfg.ivs.fps_high);

    atomic_store(&s_producers_run, true);

    web_sock_srv_init();

    /* OSD: non-fatal — log and continue if font file missing. */
    if (osd_init() == 0)
    {
        s_osd_inited = true;
        if (pthread_create(&s_osd_thr, NULL, osd_thread, NULL) == 0)
            s_osd_started = true;
    }

    if (pthread_create(&s_h264_thr, NULL, h264_thread, ctx) != 0)
        goto fail_threads;
    s_h264_started = true;
    if (pthread_create(&s_mjpeg_thr, NULL, mjpeg_thread, ctx) != 0)
        goto fail_threads;
    s_mjpeg_started = true;
    if (pthread_create(&s_ivs_thr, NULL, ivs_thread, ctx) != 0)
        goto fail_threads;
    s_ivs_started = true;

    MS_LOG_INFO("media up\n");
    return 0;

fail_threads:
    atomic_store(&s_producers_run, false);
    if (s_h264_started)
        pthread_join(s_h264_thr, NULL);
    if (s_mjpeg_started)
        pthread_join(s_mjpeg_thr, NULL);
    if (s_ivs_started)
        pthread_join(s_ivs_thr, NULL);
    s_h264_started = s_mjpeg_started = s_ivs_started = false;
    motion_pipeline_deinit();
fail_pipe1:
    stream_pipeline_deinit();
fail_pipe0:
    record_pipeline_deinit();
fail_aiq:
    aiq_stop();
fail_vi:
    vi_dev_deinit();
fail_sys:
    RK_MPI_SYS_Exit();
    return -1;
}

void media_stop(app_ctx_t *ctx)
{
    (void)ctx;
    if (!atomic_load(&s_producers_run))
        return;

    atomic_store(&s_producers_run, false);

    /* Unblock any GetStream call still parked in VENC. */
    RK_MPI_VENC_StopRecvFrame(VENC_CHN_H264);
    RK_MPI_VENC_StopRecvFrame(VENC_CHN_MJPEG);

    if (s_osd_started)
    {
        pthread_join(s_osd_thr, NULL);
        s_osd_started = false;
    }
    if (s_h264_started)
    {
        pthread_join(s_h264_thr, NULL);
        s_h264_started = false;
    }
    if (s_mjpeg_started)
    {
        pthread_join(s_mjpeg_thr, NULL);
        s_mjpeg_started = false;
    }
    if (s_ivs_started)
    {
        pthread_join(s_ivs_thr, NULL);
        s_ivs_started = false;
    }
    MS_LOG_INFO("media producers joined\n");

    if (s_web_sock >= 0)
    {
        close(s_web_sock);
        s_web_sock = -1;
    }
    if (s_web_srv_fd >= 0)
    {
        close(s_web_srv_fd);
        s_web_srv_fd = -1;
        unlink(MJPEG_SOCK_PATH);
    }
}

void media_deinit(app_ctx_t *ctx)
{
    (void)ctx;
    media_stop(ctx);
    if (s_osd_inited)
    {
        osd_deinit();
        s_osd_inited = false;
    }
    motion_pipeline_deinit();
    stream_pipeline_deinit();
    record_pipeline_deinit();
    aiq_stop();
    vi_dev_deinit();
    RK_MPI_SYS_Exit();
    MS_LOG_INFO("media down\n");
}
