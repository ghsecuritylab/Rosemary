#undef LOG_TAG
#define LOG_TAG     "LCDRGBRenderer"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/ioctl.h>

#include <linux/fb.h>
#include <linux/media.h>
#include <linux/v4l2-subdev.h>
#include <linux/v4l2-mediabus.h>
#include <linux/videodev2.h>
#include <linux/ion.h>
#include <linux/nxp_ion.h>
#include <linux/videodev2_nxp_media.h>

#include <ion/ion.h>
#include <android-nxp-v4l2.h>
#include <nxp-v4l2.h>

#include <cutils/log.h>

#include <hardware/hwcomposer.h>
#include <hardware/hardware.h>

#include <gralloc_priv.h>

#include "HWCRenderer.h"
#include "LCDRGBRenderer.h"

namespace android {

LCDRGBRenderer::LCDRGBRenderer(int id)
    :HWCRenderer(id),
    mFBFd(-1)
{
}

LCDRGBRenderer::~LCDRGBRenderer()
{
    if (mFBFd >= 0)
        close(mFBFd);
}

#define NXPFB_SET_FB_FD _IOW('N', 102, __u32)
int LCDRGBRenderer::render(__attribute__((__unused__)) int *fenceFd)
{
    if (mHandle) {
        if (mFBFd < 0) {
            mFBFd = open("/dev/graphics/fb0", O_RDWR);
            if (mFBFd < 0) {
                ALOGE("failed to open framebuffer");
                return -EINVAL;
            }
        }
        ioctl(mFBFd, NXPFB_SET_FB_FD, &mHandle->share_fd);
        mHandle = NULL;
    }

    return 0;
}

}; // namespace
