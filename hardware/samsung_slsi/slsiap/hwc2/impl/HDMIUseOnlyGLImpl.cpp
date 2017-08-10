#undef LOG_TAG
#define LOG_TAG     "HDMIUseOnlyGLImpl"

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
#include "HWCCommonRenderer.h"

#include "HWCImpl.h"
#include "HDMICommonImpl.h"
#include "HDMIUseOnlyGLImpl.h"

using namespace android;

HDMIUseOnlyGLImpl::HDMIUseOnlyGLImpl(int rgbID)
    :HDMICommonImpl(rgbID, -1),
    mRGBLayerIndex(-1),
    mRGBRenderer(NULL),
    mRGBHandle(NULL)
{
    init();
}

HDMIUseOnlyGLImpl::HDMIUseOnlyGLImpl(int rgbID, int width, int height)
    :HDMICommonImpl(rgbID, -1, width, height, width, height),
    mRGBLayerIndex(-1),
    mRGBRenderer(NULL),
    mRGBHandle(NULL)
{
    init();
}

HDMIUseOnlyGLImpl::~HDMIUseOnlyGLImpl()
{
    if (mRGBRenderer)
        delete mRGBRenderer;
}

void HDMIUseOnlyGLImpl::init()
{
    mRGBRenderer = new HWCCommonRenderer(mRgbID, 3, 1);
    if (!mRGBRenderer)
        ALOGE("FATAL: can't create RGBRenderer");
}

int HDMIUseOnlyGLImpl::enable()
{
    if (!mEnabled) {
        ALOGD("Enable");
        int ret = v4l2_link(mSubID, mMyDevice);
        if (ret < 0) {
            ALOGE("can't link %d-->%d", mSubID, mMyDevice);
            // return -EINVAL;
        }
        mEnabled = true;
    }
    return 0;
}

int HDMIUseOnlyGLImpl::disable()
{
    // if (mEnabled) {
        ALOGD("Disable");
        mRGBRenderer->stop();
        v4l2_unlink(mSubID, mMyDevice);
        HDMICommonImpl::disable();
        mEnabled = false;
    // }
    return 0;
}

int HDMIUseOnlyGLImpl::prepare(hwc_display_contents_1_t *contents)
{
    for (size_t i = 0; i < contents->numHwLayers; i++) {
        hwc_layer_1_t &layer = contents->hwLayers[i];

        if (layer.compositionType == HWC_FRAMEBUFFER_TARGET) {
            continue;
        }

        if (layer.compositionType == HWC_BACKGROUND)
            continue;

        layer.compositionType = HWC_FRAMEBUFFER;
    }

    ALOGV("prepare: rgb index %d", mRGBLayerIndex);
    return 0;
}

int HDMIUseOnlyGLImpl::set(hwc_display_contents_1_t *contents, __attribute__((__unused__)) void *unused)
{
    // int fbTargetIndex = -1;
    // for (int i = (contents->numHwLayers - 1); i >= 0; i--) {
    //     hwc_layer_1_t &layer = contents->hwLayers[i];
    //
    //     if (layer.compositionType == HWC_FRAMEBUFFER_TARGET) {
    //         ALOGD("HWC_FRAMEBUFFER_TARGET index %d, numLayers %d", i, contents->numHwLayers);
    //         fbTargetIndex = i;
    //         break;
    //     }
    // }
    //
    // if (fbTargetIndex < 0) {
    //     ALOGE("%s: can't find fbTargetIndex\n", __func__);
    //     return -1;
    // }
    // hwc_layer_1_t *framebufferHWCLayer = &contents->hwLayers[fbTargetIndex];

    hwc_layer_1_t *framebufferHWCLayer = &contents->hwLayers[contents->numHwLayers - 1];
    mRGBHandle = reinterpret_cast<private_handle_t const *>(framebufferHWCLayer->handle);
    if (!mRGBHandle) {
        ALOGE("%s: No vaild RGB Handle, hwc_layer_1_t %p\n", __func__, framebufferHWCLayer);
        return -1;
    }
    configHDMI(mWidth, mHeight);
    configRgb(*framebufferHWCLayer);
    mRGBRenderer->setHandle(mRGBHandle);
    ALOGV("%s: handle %p\n", __func__, mRGBHandle);
    return 0;
}

private_handle_t const *HDMIUseOnlyGLImpl::getRgbHandle()
{
    return mRGBHandle;
}

private_handle_t const *HDMIUseOnlyGLImpl::getVideoHandle()
{
    return NULL;
}

int HDMIUseOnlyGLImpl::render()
{
    if (mRGBHandle)
        return mRGBRenderer->render();
    else
        ALOGE("%s --> No valid RGBHandle\n", __func__);
    return 0;
}
