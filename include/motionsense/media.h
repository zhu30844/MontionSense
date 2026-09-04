#ifndef MOTIONSENSE_MEDIA_H
#define MOTIONSENSE_MEDIA_H


#include "motionsense/app_ctx.h"

#ifdef __cplusplus
extern "C" {
#endif


// VI Entities
#define VI_DEV_ID                   0   // VI device, 1 device from 0 only
#define VI_PIPE_ID                  0   // VI pipe, 1 pipe from 0 only

#define VI_PHY_CHN_MAIN             0   // recording,   rkisp_mainpath,              supports NV12/NV16/UYVY          
#define VI_PHY_CHN_SELF             1   // streaming,   rkisp_selfpath,              supports NV12/NV16/UYVY/RGB565/XBGR
#define VI_PHY_CHN_BYPATH           2   // not in use,  rkisp_bypath,                supports NV12/NV16/UYVY
#define VI_PHY_CHN_MAIN_SAMPLING    3   // not in use,  rkisp_mainpath_4x4sampling,  supports NV12/NV16/UYVY
#define VI_PHY_CHN_BYPATH_SAMPLING  4   // Motion,      rkisp_bypath_4x4sampling,    supports NV12/NV16/UYVY


// IVS Entities
#define IVS_DEV_ID                  0   // IVS group, 1 group from 0 only
#define IVS_CHN_ID                  0   // IVS channel, 1 channel from 0


// VENC Entities
#define VENC_DEV_ID                 0   // VENC device, 1 device from 0 only
#define VENC_CHN_H264               0   // H264 channel, 0
#define VENC_CHN_MJPEG              1   // MJPEG(stream) channel, 1

// RGN Entities
#define OSD_RGN_H264                0   // OSD region for H264 channel
#define OSD_RGN_MJPEG               1   // OSD region for MJPEG channel

/* CFG_ISP_IQ_DIR and CFG_SOCKET_PATH live in config.h; they used to be
 * duplicated here, and the socket had a second name (MJPEG_SOCK_PATH) that
 * disagreed with the one the Go agent dials. */

/* Bring VI/VENC/IVS up and start the three producer threads. */
int  media_init(app_ctx_t *ctx);

/* Cooperative stop: signals VENC to return pending GetStream calls and
 * joins the producer threads. Safe to call before media_deinit. */
void media_stop(app_ctx_t *ctx);

/* Tear down all Rockchip MPI resources. */
void media_deinit(app_ctx_t *ctx);

#ifdef __cplusplus
}
#endif
#endif
