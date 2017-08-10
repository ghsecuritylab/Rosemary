#define LOG_TAG "RecordThread"

#include <linux/videodev2.h>
#include <linux/v4l2-mediabus.h>
#include <linux/ion.h>
#include <linux/nxp_ion.h>

#include <ion/ion.h>
#include <ion-private.h>
#include <gralloc_priv.h>

#include "Constants.h"
#include "RecordThread.h"

namespace android {

RecordThread::RecordThread(nxp_v4l2_id id,
        int width,
        int height,
        sp<NXZoomController> &zoomController,
        sp<NXStreamManager> &streamManager)
    : NXStreamThread(id, width, height, zoomController, streamManager)
{
    init(id);
    UseZoom = ZoomController->useZoom();
    if (UseZoom) {
        PlaneNum = 3;
        Format = V4L2_PIX_FMT_YUV420M;
    } else {
        PlaneNum = 1;
        Format = V4L2_PIX_FMT_YUV420;
    }
}

RecordThread::~RecordThread()
{
}

void RecordThread::init(nxp_v4l2_id id)
{
    NXStreamThread::init(id);
}

void RecordThread::onCommand(int32_t streamId, camera_metadata_t *metadata)
{
    if (streamId == STREAM_ID_RECORD) {
        if (metadata)
            start(streamId, (char *)"RecordThread");
    } else {
        ALOGE("Invalid Stream ID: %d", streamId);
    }
}

status_t RecordThread::readyToRun()
{
    ALOGD("recordThread readyToRun entered: %dx%d", Width, Height);

    NXStream *stream = getActiveStream();
    if (!stream) {
        ALOGE("No ActiveStream!!!");
        return NO_INIT;
    }

    status_t res = stream->initBuffer();
    if (res != NO_ERROR) {
        ALOGE("failed to initBuffer");
        return NO_INIT;
    }

    int ret = v4l2_set_format(Id, Width, Height, Format);
    if (ret < 0) {
        ALOGE("failed to v4l2_set_format for %d", Id);
        return NO_INIT;
    }

    ret = v4l2_set_crop(Id, 0, 0, Width, Height);
    if (ret < 0) {
        ALOGE("failed to v4l2_set_crop for %d", Id);
        return NO_INIT;
    }

    sp<NXStreamThread> previewThread = StreamManager->getStreamThread(STREAM_ID_PREVIEW);
    if (previewThread == NULL || !previewThread->isRunning()) {
        ALOGD("Preview thread is not running... wait");
        usleep(100*1000);
        return true;
    }

    if (UseZoom) {
        if (false == ZoomController->allocBuffer(MAX_RECORD_ZOOM_BUFFER, Width, Height, PIXINDEX2PIXFORMAT(PixelIndex))) {
            ALOGE("failed to allocate record zoom buffer");
            return NO_MEMORY;
        }
        ret = v4l2_reqbuf(Id, ZoomController->getBufferCount());
        if (ret < 0) {
            ALOGE("failed to v4l2_reqbuf for record");
            return NO_INIT;
        }

        PlaneNum = ZoomController->getBuffer(0)->plane_num;
        for (int i = 0; i < ZoomController->getBufferCount(); i++) {
            ret = v4l2_qbuf(Id, PlaneNum, i, ZoomController->getBuffer(i), -1, NULL);
            if (ret < 0) {
                ALOGE("failed to v4l2_qbuf for record %d", i);
                return NO_INIT;
            }
        }
    } else {
        size_t queuedSize = stream->getQueuedSize();
        ret = v4l2_reqbuf(Id, queuedSize);
        if (ret < 0) {
            ALOGE("failed to v4l2_reqbuf for %d", Id);
            return NO_INIT;
        }

        for (size_t i = 0; i < queuedSize; i++) {
            const buffer_handle_t *b = stream->getQueuedBuffer(queuedSize - i - 1);
            ret = v4l2_qbuf(Id, PlaneNum, i, reinterpret_cast<private_handle_t const *>(*b), -1, NULL);
            if (ret < 0) {
                ALOGE("failed to v4l2_qbuf for %d", Id);
                return NO_INIT;
            }
        }
    }

    ret = v4l2_streamon(Id);
    if (ret < 0) {
        ALOGE("failed to v4l2_streamon for %d", Id);
        return NO_INIT;
    }

    InitialSkipCount = get_board_preview_skip_frame(SensorId);

    ALOGD("readyToRun exit");
    return NO_ERROR;
}

bool RecordThread::threadLoop()
{
    int dqIdx;
    int ret;
    nsecs_t timestamp;
    buffer_handle_t *buf;

    NXStream *stream = getActiveStream();
    if (!stream) {
        ALOGE("can't getActiveStream()");
        ERROR_EXIT();
    }

    ALOGV("dqEnter");
    ret = v4l2_dqbuf(Id, PlaneNum, &dqIdx, NULL);
    if (ret < 0) {
        ALOGE("failed to v4l2_dqbuf for %d", Id);
        return false;
    }
    ALOGV("dqIdx: %d", dqIdx);

    CHECK_AND_EXIT();

    if (InitialSkipCount) {
        InitialSkipCount--;
        ALOGV("Recorder Skip Frame: %d", InitialSkipCount);
        stream->cancelBuffer();
    } else {
        if (UseZoom) {
            struct nxp_vid_buffer *srcBuf = ZoomController->getBuffer(dqIdx);
            private_handle_t const *dstHandle = stream->getNextBuffer();
            ZoomController->handleZoom(srcBuf, dstHandle);
        }

#ifdef USE_SYSTEM_TIMESTAMP
        ret = stream->enqueueBuffer(systemTime(SYSTEM_TIME_MONOTONIC));
#else
        ret = v4l2_get_timestamp(Id, &timestamp);
        if (ret < 0)
            ALOGE("failed to v4l2_get_timestamp for %d", Id);

        ALOGV("timestamp: %llu", timestamp);
        stream->setTimestamp(timestamp);

        ret = stream->enqueueBuffer(timestamp);
#endif
        if (ret != NO_ERROR) {
            ALOGE("failed to enqueue_buffer (idx:%d)", dqIdx);
            ERROR_EXIT();
        }
        ALOGV("end enqueueBuffer");
    }

    ret = stream->dequeueBuffer(&buf);
    if (ret != NO_ERROR || buf == NULL) {
        ALOGE("failed to dequeue_buffer");
        ERROR_EXIT();
    }
    ALOGV("end dequeueBuffer");

    if (UseZoom)
        ret = v4l2_qbuf(Id, PlaneNum, dqIdx, ZoomController->getBuffer(dqIdx), -1, NULL);
    else
        ret = v4l2_qbuf(Id, PlaneNum, dqIdx, reinterpret_cast<private_handle_t const *>(*buf), -1, NULL);
    if (ret) {
        ALOGE("failed to v4l2_qbuf()");
        ERROR_EXIT();
    }
    ALOGV("end v4l2_qbuf");

    return true;
}

}; // namespace android
