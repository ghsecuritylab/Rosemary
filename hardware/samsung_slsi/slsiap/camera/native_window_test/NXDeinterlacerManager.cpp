#define LOG_TAG "NXDeinterlacerManager"

#include <unistd.h>
#include <fcntl.h>
#include <utils/Log.h>
#include <gralloc_priv.h>
#include <ion/ion.h>

#include "NXDeinterlacerManager.h"

using namespace android;

#define DEINTERLACER_DEVICE_NODE_NAME   "/dev/deinterlacer"

NXDeinterlacerManager::NXDeinterlacerManager(int srcWidth, int srcHeight)
    : mSrcWidth(srcWidth),
      mSrcHeight(srcHeight)
{
	mRunCount	=	0;
    mHandle = -1;
    mIonHandle = -1;
    mCurrentField = NXDeinterlacerManager::FIELD_EVEN;
    //mCurrentField = NXDeinterlacerManager::FIELD_ODD;
    mSrcType = NXDeinterlacerManager::SRC_TYPE_FRAME;
}

NXDeinterlacerManager::~NXDeinterlacerManager(void)
{
    if (mHandle >= 0)
        close(mHandle);

    if (mIonHandle >= 0)
        close(mIonHandle);
}

bool NXDeinterlacerManager::qSrcBuf(int index, struct nxp_vid_buffer *buf)
{
    NXDeinterlacerManager::SrcBufferType entry;
    entry.index = index;
    entry.buf = buf;
    mSrcBufferQueue.queue(entry);
    ALOGV("qSrcBuf index %d, buf %p", index, buf);
    return true;
}

bool NXDeinterlacerManager::dqSrcBuf(int *pIndex, struct nxp_vid_buffer **pBuf)
{
    if (mRunCount >= 2) {
        NXDeinterlacerManager::SrcBufferType entry = mSrcBufferQueue.dequeue();

        *pIndex = entry.index;
        *pBuf = entry.buf;
        mRunCount -= 2;
        ALOGV("dqSrcBuf index %d, buf %p, mRunCount %d", entry.index, entry.buf, mRunCount);
        return true;
    }
    return false;
}

bool NXDeinterlacerManager::qDstBuf(struct nxp_vid_buffer *buf)
{
  	 ALOGV("qDstBuf %p", buf);
    mDstBufferQueue.queue(buf);
    return true;
}

bool NXDeinterlacerManager::dqDstBuf(struct nxp_vid_buffer **pBuf)
{
    *pBuf = mDstBufferQueue.dequeue();
    ALOGV("dqDstBuf %p", *pBuf);
    return true;
}

bool NXDeinterlacerManager::run(void)
{
    if (mSrcBufferQueue.size() >= 2) {
        if (mHandle < 0) {
            mHandle = open(DEINTERLACER_DEVICE_NODE_NAME, O_RDWR);
            if (mHandle < 0) {
              ALOGE("Fatal Error: can't open device node %s", DEINTERLACER_DEVICE_NODE_NAME);
                return false;
            }
        }

        if (mIonHandle < 0) {
             mIonHandle = ion_open();
             if (mIonHandle < 0) {
                ALOGE("Fatal Error: failed to ion_open()");
                 return false;
             }
        }

        int runCount = mSrcBufferQueue.size() * 2 - 2;
        for (int i = 0; i < runCount; i++) {
            makeFrameInfo(i/2);
            if (ioctl(mHandle, IOCTL_DEINTERLACE_SET_AND_RUN, &mFrameInfo) == -1) {
               	ALOGE("Critical Error: IOCTL_DEINTERLACE_SET_AND_RUN failed");
                return false;
            }
        }

        mRunCount += runCount;
        return true;
    }
    return false;
}

void NXDeinterlacerManager::makeFrameInfo(int index)
{
    frame_data_info *pInfo = &mFrameInfo;

    // get src buffer
    NXDeinterlacerManager::SrcBufferType srcBuf0, srcBuf1;

    srcBuf0 = mSrcBufferQueue.getItem(mSrcBufferQueue.size() - index - 1);
    srcBuf1 = mSrcBufferQueue.getItem(mSrcBufferQueue.size() - (index + 1) - 1);

    struct nxp_vid_buffer *pSrcBuf0, *pSrcBuf1;
    pSrcBuf0 = srcBuf0.buf;
    pSrcBuf1 = srcBuf1.buf;
    ALOGD("srcbuf: %p, %p", pSrcBuf0, pSrcBuf1);

    struct nxp_vid_buffer *pDstBuf;

    frame_data *pSrcFrame0 = &pInfo->src_bufs[0];
    frame_data *pSrcFrame1 = &pInfo->src_bufs[1];
    frame_data *pSrcFrame2 = &pInfo->src_bufs[2];
    frame_data *pDstFrame  = &pInfo->dst_bufs[0];

    pInfo->command = ACT_DIRECT;
    pInfo->width = mSrcWidth;
    pInfo->height = mSrcHeight;
    pInfo->plane_mode = PLANE3;

    if (mSrcType == NXDeinterlacerManager::SRC_TYPE_FRAME) {
        if (mCurrentField == NXDeinterlacerManager::FIELD_EVEN) {
            // EVEN - ODD - EVEN
            pDstBuf = mDstBufferQueue.getItem(mDstBufferQueue.size() - index - 1);
            ALOGD("Even dstBuf: %p", pDstBuf);

            // EVEN
            pSrcFrame0->frame_num = 1;
            pSrcFrame0->plane_num = 3;
            pSrcFrame0->frame_type = FRAME_SRC;
            pSrcFrame0->frame_factor = 1;
            pSrcFrame0->plane3.src_stride[0] = YUV_YSTRIDE(mSrcWidth);
            pSrcFrame0->plane3.src_stride[1] = YUV_STRIDE(mSrcWidth/2);
            pSrcFrame0->plane3.src_stride[2] = YUV_STRIDE(mSrcWidth/2);
            pSrcFrame0->plane3.phys[0] = pSrcBuf0->phys[0];
            pSrcFrame0->plane3.phys[1] = pSrcBuf0->phys[1];
            pSrcFrame0->plane3.phys[2] = pSrcBuf0->phys[2];

            // ODD
            pSrcFrame1->frame_num = 0;
            pSrcFrame1->plane_num = 3;
            pSrcFrame1->frame_type = FRAME_SRC;
            pSrcFrame1->frame_factor = 1;
            pSrcFrame1->plane3.src_stride[0] = YUV_YSTRIDE(mSrcWidth);
            pSrcFrame1->plane3.src_stride[1] = YUV_STRIDE(mSrcWidth/2);
            pSrcFrame1->plane3.src_stride[2] = YUV_STRIDE(mSrcWidth/2);
            pSrcFrame1->plane3.phys[0] = pSrcBuf0->phys[0] + (pSrcBuf0->stride[0] * mSrcHeight);
            pSrcFrame1->plane3.phys[1] = pSrcBuf0->phys[1] + (pSrcBuf0->stride[1] * (mSrcHeight/2));
            pSrcFrame1->plane3.phys[2] = pSrcBuf0->phys[2] + (pSrcBuf0->stride[2] * (mSrcHeight/2));

            // EVEN
            pSrcFrame2->frame_num = 1;
            pSrcFrame2->plane_num = 3;
            pSrcFrame2->frame_type = FRAME_SRC;
            pSrcFrame2->frame_factor = 1;
            pSrcFrame2->plane3.src_stride[0] = YUV_YSTRIDE(mSrcWidth);
            pSrcFrame2->plane3.src_stride[1] = YUV_STRIDE(mSrcWidth/2);
            pSrcFrame2->plane3.src_stride[2] = YUV_STRIDE(mSrcWidth/2);
            pSrcFrame2->plane3.phys[0] = pSrcBuf1->phys[0];
            pSrcFrame2->plane3.phys[1] = pSrcBuf1->phys[1];
            pSrcFrame2->plane3.phys[2] = pSrcBuf1->phys[2];
        } else {

            // ODD - EVEN - ODD
            pDstBuf = mDstBufferQueue.getItem(mDstBufferQueue.size() - (index + 1) - 1);
            ALOGD("ODD dstBuf: %p", pDstBuf);

            // ODD
            pSrcFrame0->frame_num = 0;
            pSrcFrame0->plane_num = 3;
            pSrcFrame0->frame_type = FRAME_SRC;
            pSrcFrame0->frame_factor = 1;
            pSrcFrame0->plane3.src_stride[0] = YUV_YSTRIDE(mSrcWidth);
            pSrcFrame0->plane3.src_stride[1] = YUV_STRIDE(mSrcWidth/2);
            pSrcFrame0->plane3.src_stride[2] = YUV_STRIDE(mSrcWidth/2);
            pSrcFrame0->plane3.phys[0] = pSrcBuf0->phys[0] + (pSrcBuf0->stride[0] * mSrcHeight);
            pSrcFrame0->plane3.phys[1] = pSrcBuf0->phys[1] + (pSrcBuf0->stride[1] * (mSrcHeight/2));
            pSrcFrame0->plane3.phys[2] = pSrcBuf0->phys[2] + (pSrcBuf0->stride[2] * (mSrcHeight/2));

            // EVEN
            pSrcFrame1->frame_num = 1;
            pSrcFrame1->plane_num = 3;
            pSrcFrame1->frame_type = FRAME_SRC;
            pSrcFrame1->frame_factor = 1;
            pSrcFrame1->plane3.src_stride[0] = YUV_YSTRIDE(mSrcWidth);
            pSrcFrame1->plane3.src_stride[1] = YUV_STRIDE(mSrcWidth/2);
            pSrcFrame1->plane3.src_stride[2] = YUV_STRIDE(mSrcWidth/2);
            pSrcFrame1->plane3.phys[0] = pSrcBuf1->phys[0];
            pSrcFrame1->plane3.phys[1] = pSrcBuf1->phys[1];
            pSrcFrame1->plane3.phys[2] = pSrcBuf1->phys[2];

            // ODD
            pSrcFrame2->frame_num = 0;
            pSrcFrame2->plane_num = 3;
            pSrcFrame2->frame_type = FRAME_SRC;
            pSrcFrame2->frame_factor = 1;
            pSrcFrame2->plane3.src_stride[0] = YUV_YSTRIDE(mSrcWidth);
            pSrcFrame2->plane3.src_stride[1] = YUV_STRIDE(mSrcWidth/2);
            pSrcFrame2->plane3.src_stride[2] = YUV_STRIDE(mSrcWidth/2);
            pSrcFrame2->plane3.phys[0] = pSrcBuf1->phys[0] + (pSrcBuf1->stride[0] * mSrcHeight);
            pSrcFrame2->plane3.phys[1] = pSrcBuf1->phys[1] + (pSrcBuf1->stride[1] * (mSrcHeight/2));
            pSrcFrame2->plane3.phys[2] = pSrcBuf1->phys[2] + (pSrcBuf1->stride[2] * (mSrcHeight/2));
        }
    } else {
        // SRC_TYPE_FIELD
        if (mCurrentField == NXDeinterlacerManager::FIELD_EVEN) {
            // EVEN - ODD - EVEN
            pDstBuf = mDstBufferQueue.getItem(mDstBufferQueue.size() - index - 1);
            ALOGD("Even dstBuf: %p", pDstBuf);

            // EVEN
            pSrcFrame0->frame_num = 1;
            pSrcFrame0->plane_num = 3;
            pSrcFrame0->frame_type = FRAME_SRC;
            pSrcFrame0->frame_factor = 1;
            pSrcFrame0->plane3.src_stride[0] = YUV_YSTRIDE(mSrcWidth) * 2;
            pSrcFrame0->plane3.src_stride[1] = YUV_STRIDE(mSrcWidth/2) * 2;
            pSrcFrame0->plane3.src_stride[2] = YUV_STRIDE(mSrcWidth/2) * 2;
            pSrcFrame0->plane3.phys[0] = pSrcBuf0->phys[0];
            pSrcFrame0->plane3.phys[1] = pSrcBuf0->phys[1];
            pSrcFrame0->plane3.phys[2] = pSrcBuf0->phys[2];

            // ODD
            pSrcFrame1->frame_num = 0;
            pSrcFrame1->plane_num = 3;
            pSrcFrame1->frame_type = FRAME_SRC;
            pSrcFrame1->frame_factor = 1;
            pSrcFrame1->plane3.src_stride[0] = YUV_YSTRIDE(mSrcWidth) * 2;
            pSrcFrame1->plane3.src_stride[1] = YUV_STRIDE(mSrcWidth/2) * 2;
            pSrcFrame1->plane3.src_stride[2] = YUV_STRIDE(mSrcWidth/2) * 2;
            pSrcFrame1->plane3.phys[0] = pSrcBuf0->phys[0] + pSrcBuf0->stride[0];
            pSrcFrame1->plane3.phys[1] = pSrcBuf0->phys[1] + pSrcBuf0->stride[1];
            pSrcFrame1->plane3.phys[2] = pSrcBuf0->phys[2] + pSrcBuf0->stride[2];

            // EVEN
            pSrcFrame2->frame_num = 1;
            pSrcFrame2->plane_num = 3;
            pSrcFrame2->frame_type = FRAME_SRC;
            pSrcFrame2->frame_factor = 1;
            pSrcFrame2->plane3.src_stride[0] = YUV_YSTRIDE(mSrcWidth) * 2;
            pSrcFrame2->plane3.src_stride[1] = YUV_STRIDE(mSrcWidth/2) * 2;
            pSrcFrame2->plane3.src_stride[2] = YUV_STRIDE(mSrcWidth/2) * 2;
            pSrcFrame2->plane3.phys[0] = pSrcBuf1->phys[0];
            pSrcFrame2->plane3.phys[1] = pSrcBuf1->phys[1];
            pSrcFrame2->plane3.phys[2] = pSrcBuf1->phys[2];
        } else {

            // ODD - EVEN - ODD
            pDstBuf = mDstBufferQueue.getItem(mDstBufferQueue.size() - (index + 1) - 1);
            ALOGD("ODD dstBuf: %p", pDstBuf);

            // ODD
            pSrcFrame0->frame_num = 0;
            pSrcFrame0->plane_num = 3;
            pSrcFrame0->frame_type = FRAME_SRC;
            pSrcFrame0->frame_factor = 1;
            pSrcFrame0->plane3.src_stride[0] = YUV_YSTRIDE(mSrcWidth) * 2;
            pSrcFrame0->plane3.src_stride[1] = YUV_STRIDE(mSrcWidth/2) * 2;
            pSrcFrame0->plane3.src_stride[2] = YUV_STRIDE(mSrcWidth/2) * 2;
            pSrcFrame0->plane3.phys[0] = pSrcBuf0->phys[0] + pSrcBuf0->stride[0];
            pSrcFrame0->plane3.phys[1] = pSrcBuf0->phys[1] + pSrcBuf0->stride[1];
            pSrcFrame0->plane3.phys[2] = pSrcBuf0->phys[2] + pSrcBuf0->stride[2];

            // EVEN
            pSrcFrame1->frame_num = 1;
            pSrcFrame1->plane_num = 3;
            pSrcFrame1->frame_type = FRAME_SRC;
            pSrcFrame1->frame_factor = 1;
            pSrcFrame1->plane3.src_stride[0] = YUV_YSTRIDE(mSrcWidth) * 2;
            pSrcFrame1->plane3.src_stride[1] = YUV_STRIDE(mSrcWidth/2) * 2;
            pSrcFrame1->plane3.src_stride[2] = YUV_STRIDE(mSrcWidth/2) * 2;
            pSrcFrame1->plane3.phys[0] = pSrcBuf1->phys[0];
            pSrcFrame1->plane3.phys[1] = pSrcBuf1->phys[1];
            pSrcFrame1->plane3.phys[2] = pSrcBuf1->phys[2];

            // ODD
            pSrcFrame2->frame_num = 0;
            pSrcFrame2->plane_num = 3;
            pSrcFrame2->frame_type = FRAME_SRC;
            pSrcFrame2->frame_factor = 1;
            pSrcFrame2->plane3.src_stride[0] = YUV_YSTRIDE(mSrcWidth) * 2;
            pSrcFrame2->plane3.src_stride[1] = YUV_STRIDE(mSrcWidth/2) * 2;
            pSrcFrame2->plane3.src_stride[2] = YUV_STRIDE(mSrcWidth/2) * 2;
            pSrcFrame2->plane3.phys[0] = pSrcBuf1->phys[0] + pSrcBuf1->stride[0];
            pSrcFrame2->plane3.phys[1] = pSrcBuf1->phys[1] + pSrcBuf1->stride[1];
            pSrcFrame2->plane3.phys[2] = pSrcBuf1->phys[2] + pSrcBuf1->stride[2];
        }
    }

    // set dest
    pDstFrame->frame_num = 0;
    pDstFrame->plane_num = 3;
    pDstFrame->frame_type = FRAME_DST;
    pDstFrame->frame_factor = 256;
    pDstFrame->plane3.dst_stride[0] = 1024;
    pDstFrame->plane3.dst_stride[1] = 512;
    pDstFrame->plane3.dst_stride[2] = 512;
    pDstFrame->plane3.phys[0] = pDstBuf->phys[0];
    pDstFrame->plane3.phys[1] = pDstBuf->phys[1];
    pDstFrame->plane3.phys[2] = pDstBuf->phys[2];

    // psw0523 HACK
    mCurrentField ^= 1;
}

void NXDeinterlacerManager::makeFrameBySW(int index)
{
    // get src buffer
    NXDeinterlacerManager::SrcBufferType srcBuf0, srcBuf1;

    srcBuf0 = mSrcBufferQueue.getItem(mSrcBufferQueue.size() - index - 1);
    srcBuf1 = mSrcBufferQueue.getItem(mSrcBufferQueue.size() - (index + 1) - 1);

    struct nxp_vid_buffer *pSrcBuf0, *pSrcBuf1;
    pSrcBuf0 = srcBuf0.buf;
    pSrcBuf1 = srcBuf1.buf;

    struct nxp_vid_buffer *pDstBuf = mDstBufferQueue.getItem(mDstBufferQueue.size() - index - 1);

    char *pSrcY = pSrcBuf1->virt[0];
    char *pSrcCb = pSrcBuf1->virt[1];
    char *pSrcCr = pSrcBuf1->virt[2];

    char *pDstY = pDstBuf->virt[0];
    char *pDstCb = pDstBuf->virt[1];
    char *pDstCr = pDstBuf->virt[2];

    int i;
    for (i = 0; i < mSrcHeight; i++) {
        memcpy(pDstY, pSrcY, mSrcWidth);
        pDstY +=  1024;
        memcpy(pDstY, pSrcY, mSrcWidth);

        pSrcY += YUV_YSTRIDE(mSrcWidth);
        pDstY += 1024;
    }

    for (i = 0; i < mSrcHeight/2; i++) {
        memcpy(pDstCb, pSrcCb, mSrcWidth/2);
        pDstCb += 512;
        memcpy(pDstCb, pSrcCb, mSrcWidth/2);

        pSrcCb += YUV_STRIDE(mSrcWidth/2);
        pDstCb += 512;
    }

    for (i = 0; i < mSrcHeight/2; i++) {
        memcpy(pDstCr, pSrcCr, mSrcWidth/2);
        pDstCr += 512;
        memcpy(pDstCr, pSrcCr, mSrcWidth/2);

        pSrcCr += YUV_STRIDE(mSrcWidth/2);
        pDstCr += 512;
    }
}

#include <linux/v4l2-mediabus.h>
#include "NXScaler.h"

static int scaler_fd = -1;
void NXDeinterlacerManager::makeFrameByScaler(int index)
{
    if (scaler_fd < 0) {
        scaler_fd = open("/dev/nxp-scaler", O_RDWR);
    }

    NXDeinterlacerManager::SrcBufferType srcBuf0, srcBuf1;

    srcBuf0 = mSrcBufferQueue.getItem(mSrcBufferQueue.size() - index - 1);
    srcBuf1 = mSrcBufferQueue.getItem(mSrcBufferQueue.size() - (index + 1) - 1);

    struct nxp_vid_buffer *pSrcBuf0, *pSrcBuf1;
    pSrcBuf0 = srcBuf0.buf;
    pSrcBuf1 = srcBuf1.buf;

    struct nxp_vid_buffer *pDstBuf = mDstBufferQueue.getItem(mDstBufferQueue.size() - index - 1);
    ALOGV("makeFrameByScaler dst %p, src %p", pDstBuf, pSrcBuf1);

    struct nxp_scaler_ioctl_data data;
    memset(&data, 0, sizeof(struct nxp_scaler_ioctl_data));

    data.src_phys[0] = pSrcBuf1->phys[0];
    data.src_phys[1] = pSrcBuf1->phys[1];
    data.src_phys[2] = pSrcBuf1->phys[2];
    data.src_stride[0] = YUV_YSTRIDE(mSrcWidth);
    data.src_stride[1] = YUV_STRIDE(mSrcWidth/2);
    data.src_stride[2] = YUV_STRIDE(mSrcWidth/2);
    data.src_width = mSrcWidth;
    data.src_height = mSrcHeight;
    data.src_code = PIXCODE_YUV420_PLANAR;
    data.dst_phys[0] = pDstBuf->phys[0];
    data.dst_phys[1] = pDstBuf->phys[1];
    data.dst_phys[2] = pDstBuf->phys[2];
    data.dst_stride[0] = 1024;
    data.dst_stride[1] = 512;
    data.dst_stride[2] = 512;
    data.dst_width = mSrcWidth;
    data.dst_height = mSrcHeight * 2;
    data.dst_code = PIXCODE_YUV420_PLANAR;

    ioctl(scaler_fd, IOCTL_SCALER_SET_AND_RUN, &data);
}
