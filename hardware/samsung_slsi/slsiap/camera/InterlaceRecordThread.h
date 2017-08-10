#ifndef _INTERLACERECORD_THREAD_H
#define _INTERLACERECORD_THREAD_H

#include <hardware/camera2.h>
#include <camera/Camera.h>
#include <camera/CameraParameters.h>
#include <utils/Thread.h>
#include <gralloc_priv.h>
#include <nx_camera_board.h>

#include "NXCommandThread.h"
#include "NXStream.h"
#include "NXStreamThread.h"
#include "NXStreamManager.h"

namespace android	{
	
class InterlaceRecordThread:	public NXStreamThread
{
public:
	InterlaceRecordThread(nxp_v4l2_id id, 
		int width,
		int height,
		sp<NXZoomController> &zoomController,
		sp<NXStreamManager>	&streamManager);
	virtual ~InterlaceRecordThread();

	virtual status_t readyToRun();
	virtual void onCommand(int32_t streamId, camera_metadata_t *metadata);

protected:
	virtual void init(nxp_v4l2_id id);

private:
	virtual bool threadLoop();

private:
	bool 		UseZoom;
	uint32_t	PlaneNum;
	uint32_t	Format;
};

};	//	namespace

#endif
