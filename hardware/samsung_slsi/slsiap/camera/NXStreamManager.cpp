#define LOG_TAG "NXStreamManager"
#define	LOG_NDEBUG	0

#include <utils/Log.h>

#include "NXCameraHWInterface2.h"
#include "NXZoomController.h"
#include "NullZoomController.h"
#include "ScalerZoomController.h"
#ifdef WORKAROUND_128BYTE_ALIGN
#include "W128BAScalerZoomController.h"
#endif
#include "NXStreamThread.h"
#include "PreviewThread.h"
#include "CallbackThread.h"
#include "CaptureThread.h"
#include "RecordThread.h"
#include "InterlaceRecordThread.h"
#include "Constants.h"
#include "NXStreamManager.h"

using namespace android;

uint32_t NXStreamManager::whatStreamAllocate(int format)
{
    uint32_t streamId = STREAM_ID_INVALID;

    switch (format) {
    case CAMERA2_HAL_PIXEL_FORMAT_OPAQUE:
        if (!getStreamThread(STREAM_ID_PREVIEW))
            streamId = STREAM_ID_PREVIEW;
        else
            streamId = STREAM_ID_RECORD;
        break;
    case HAL_PIXEL_FORMAT_YV12:
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
        streamId = STREAM_ID_CALLBACK;
        break;
    case HAL_PIXEL_FORMAT_BLOB:
        streamId = STREAM_ID_CAPTURE;
        break;
    case CAMERA2_HAL_PIXEL_FORMAT_ZSL:
        streamId = STREAM_ID_ZSL;
        break;
    default:
        ALOGE("Unknown format 0x%x", format);
        break;
    }

    return streamId;
}

int NXStreamManager::allocateStream(uint32_t width, uint32_t height, int format, const camera2_stream_ops_t *streamOps,
        uint32_t *streamId, uint32_t *formatActual, uint32_t *usage, uint32_t *maxBuffers, bool useSensorZoom)
{
    uint32_t id = whatStreamAllocate(format);
    if (id == STREAM_ID_INVALID) {
        ALOGE("What stream can I allocate???");
        return INVALID_OPERATION;
    }

    if (getStreamThread(id)) {
        ALOGE("stream %d already exist", id);
        return ALREADY_EXISTS;
    }

    NXZoomController *zoomController = NULL;
    if (useSensorZoom) {
        zoomController = new NullZoomController();
    } else {
        uint32_t cropLeft, cropTop, cropWidth, cropHeight, baseWidth, baseHeight;
        cropLeft = cropTop = cropWidth = cropHeight = baseWidth = baseHeight = 0;
        Parent->getCrop(cropLeft, cropTop, cropWidth, cropHeight, baseWidth, baseHeight);
        zoomController = new ScalerZoomController(cropLeft, cropTop, cropWidth, cropHeight, baseWidth, baseHeight, width, height);
    }
    if (zoomController == NULL) {
        ALOGE("can't create NXZoomController!!!");
        return NO_MEMORY;
    }

    sp<NXZoomController> spZoomController(zoomController);
    sp<NXStreamManager> spStreamManager(this);

    NXStreamThread *streamThread = NULL;
	bool	interlace = false;

    switch (id) {
    case STREAM_ID_PREVIEW:
        ALOGD("allocate PreviewThread");
        *streamId = STREAM_ID_PREVIEW;
        *usage = GRALLOC_USAGE_HW_CAMERA_WRITE;
        *maxBuffers = MAX_STREAM_BUFFERS;
        *formatActual = DEFAULT_PIXEL_FORMAT;
#ifdef WORKAROUND_128BYTE_ALIGN
        uint32_t cropLeft, cropTop, cropWidth, cropHeight, baseWidth, baseHeight;
        cropLeft = cropTop = cropWidth = cropHeight = baseWidth = baseHeight = 0;
        Parent->getCrop(cropLeft, cropTop, cropWidth, cropHeight, baseWidth, baseHeight);
        spZoomController = new W128BAScalerZoomController(cropLeft, cropTop, cropWidth, cropHeight, baseWidth, baseHeight, width, height);
        //free(zoomController);
#endif
        streamThread = new PreviewThread((nxp_v4l2_id)get_board_preview_v4l2_id(Parent->getCameraId()),
                width,
                height,
                spZoomController,
                spStreamManager);
        break;

    case STREAM_ID_CAPTURE:
        ALOGD("allocate CaptureThread");
        *streamId = STREAM_ID_CAPTURE;
        *usage = GRALLOC_USAGE_SW_WRITE_OFTEN;
        *maxBuffers = MAX_STREAM_BUFFERS;
        *formatActual = HAL_PIXEL_FORMAT_BLOB;
        streamThread = new CaptureThread((nxp_v4l2_id)get_board_capture_v4l2_id(Parent->getCameraId()),
                Parent->getFrameQueueDstOps(),
                width,
                height,
                spZoomController,
                spStreamManager);
        break;

    case STREAM_ID_RECORD:
        ALOGD("allocate RecordThread");
        *streamId = STREAM_ID_RECORD;
        *usage = GRALLOC_USAGE_SW_WRITE_OFTEN;
        *maxBuffers = MAX_STREAM_BUFFERS;
        *formatActual = DEFAULT_PIXEL_FORMAT;

		interlace = Parent->Sensor->isInterlace();
#ifdef ARCH_S5P4418
		if (interlace || ((width % 128) != 0)) {
#else
		if (interlace) {
#endif
			streamThread = new InterlaceRecordThread((nxp_v4l2_id)get_board_record_v4l2_id(Parent->getCameraId()), 
				width,
				height,
				spZoomController, 
				spStreamManager);
		} else
			streamThread = new RecordThread((nxp_v4l2_id)get_board_record_v4l2_id(Parent->getCameraId()),
				  width,
				  height,
				  spZoomController,
				  spStreamManager);
        break;

    case STREAM_ID_ZSL:
        ALOGD("allocate ZslThread");
        *streamId = STREAM_ID_ZSL;
        *usage = GRALLOC_USAGE_SW_WRITE_OFTEN;
        *maxBuffers = MAX_STREAM_BUFFERS;
        *formatActual = DEFAULT_PIXEL_FORMAT;
        streamThread = getStreamThread(STREAM_ID_PREVIEW);
        break;

    case STREAM_ID_CALLBACK:
        ALOGD("allocate CallbackThread");
        *streamId = STREAM_ID_CALLBACK;
        *usage = GRALLOC_USAGE_HW_CAMERA_WRITE;
        *maxBuffers = MAX_STREAM_BUFFERS;
        *formatActual = format;
        streamThread = new CallbackThread(width, height, spStreamManager);
        break;
    }

    if (streamThread == NULL) {
        ALOGE("can't create NXStreamThread for %d!!!", *streamId);
        return NO_MEMORY;
    }

    sp<NXStream> stream = new NXStream(*streamId, width, height, *formatActual, streamOps, *maxBuffers);
    if (stream == NULL) {
        ALOGE("can't create NXStream for %d", *streamId);
        return NO_MEMORY;
    }

    if (NO_ERROR != streamThread->addStream(*streamId, stream)) {
        ALOGE("failed to add stream to streamThread %d", *streamId);
        return NO_INIT;
    }

    sp<NXStreamThread> spStreamThread(streamThread);
    StreamThreads.add(*streamId, spStreamThread);
    return NO_ERROR;
}

int NXStreamManager::removeStream(uint32_t streamId)
{
    if (StreamThreads.indexOfKey(streamId) < 0) {
        ALOGE("removeStream: streamId %d is not added", streamId);
        return NO_ERROR;
    }
    StreamThreads.removeItem(streamId);
    return NO_ERROR;
}
