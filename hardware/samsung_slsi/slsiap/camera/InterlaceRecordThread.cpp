#define LOG_TAG "InterlaceRecordThread"

#include <linux/videodev2.h>
#include <linux/v4l2-mediabus.h>
#include <linux/ion.h>
#include <linux/nxp_ion.h>

#include <ion/ion.h>
#include <ion-private.h>
#include <gralloc_priv.h>

#include <NXUtil.h>

#include "Constants.h"
#include "InterlaceRecordThread.h"

namespace android {
	InterlaceRecordThread::InterlaceRecordThread(nxp_v4l2_id id, 
		int width,
		int height,
		sp<NXZoomController> &zoomController,
		sp<NXStreamManager> &streamManager)
	: NXStreamThread(id, width, height, zoomController, streamManager)
{
	init(id);
}

InterlaceRecordThread::~InterlaceRecordThread()
{
}

void InterlaceRecordThread::init(nxp_v4l2_id id)
{
}

void InterlaceRecordThread::onCommand(int32_t streamId, camera_metadata_t *metadata)
{
	if(streamId == STREAM_ID_RECORD) {
		if( metadata )
			start(streamId, (char *)"InterlaceRecordThread");
	} else{
		ALOGE("Invalid Stream ID: %d", streamId);
	}

	ALOGV("end onCommand...!!");
}

status_t InterlaceRecordThread::readyToRun()
{
	ALOGV("InterlaceRecordThread readyToRun entered: %dx%d", Width, Height);

	NXStream *stream	=	getActiveStream();
	if( !stream ) {
		ALOGE("No ActiveStream!!!");
		return NO_INIT;
	}

	status_t res	=	stream->initBuffer();
	if(res != NO_ERROR) {
		ALOGE("failed to initBuffer.");
		return NO_INIT; 
	}

	InitialSkipCount	=	get_board_preview_skip_frame(SensorId);
	
	ALOGV("readyToRun exit");
	return NO_ERROR;	
}

bool InterlaceRecordThread::threadLoop()
{
	CHECK_AND_EXIT();

	int dqIdx = 0;
	int ret;
	nsecs_t timestamp;
	buffer_handle_t *buf;

	NXStream *stream = getActiveStream();
	if (!stream ) {
		ALOGE("can't getActiveStream()");
		ERROR_EXIT();
	}

	sp<NXStreamThread> previewThread = StreamManager->getStreamThread(STREAM_ID_PREVIEW);
	if (previewThread == NULL || !previewThread->isRunning()) {
		ALOGD("Preview thread is not running... wait.");
		usleep(100 * 1000);
		return true;
	}

	CHECK_AND_EXIT();

	int width, height;
	private_handle_t const *srcHandle = previewThread->getLastBuffer(width, height);
	if (!srcHandle) {
		ALOGE("can't get preview thread laast buffer...wait");
		usleep(500*1000);
		return true;
	}

	CHECK_AND_EXIT();

	if (Width != width || Height != height) {
		ALOGE("preview wxh(%dx%d is diffent from me(%dx%d", width, height, Width, Height);
		ERROR_EXIT();
	}
	
	//ALOGD("dqIdx: %d\n", dqIdx);

	private_handle_t const *dstHandle = stream->getNextBuffer();
	if (!dstHandle) {
		ALOGE("can't get dstHandle");
		ERROR_EXIT();
	}

	ALOGV("src format: 0x%X, dst format: 0x%X", srcHandle->format, dstHandle->format);
	ALOGV("srcHadle %p, dstHandle %p", srcHandle, dstHandle);

//	nxCopySrcData(srcHandle, dstHandle, Width, Height);
	nxMemcpyHandle(dstHandle, srcHandle);

	ret = stream->enqueueBuffer(systemTime(SYSTEM_TIME_MONOTONIC));	
	if (ret != NO_ERROR) {
		ALOGE("failed to enqueue_buffer (idx:%d)", dqIdx);
		ERROR_EXIT();
	}

	ALOGV("end enqueueBuffer");
	
	CHECK_AND_EXIT();

	ret = stream->dequeueBuffer(&buf);	
	if (ret != NO_ERROR || buf == NULL) {
		ALOGE("failed to dequeue_buffer");
		ERROR_EXIT();
	}

	ALOGV("end dequeueBuffer");
	ALOGV("end InterlaceRecordThreadLoop...!!");

	CHECK_AND_EXIT();

	return true;
}

}; //	namespace android
