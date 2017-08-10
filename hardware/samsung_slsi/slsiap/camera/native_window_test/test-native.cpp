#define LOG_TAG "camera-native-test"

#include <stdlib.h>
#include <ui/GraphicBuffer.h>
#include <ui/GraphicBufferMapper.h>
#include <gui/Surface.h>
#include <gui/ISurfaceComposer.h>
#include <gui/SurfaceComposerClient.h>

#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>

#include <cutils/log.h>

#include <linux/media.h>
#include <linux/v4l2-subdev.h>
#include <linux/v4l2-mediabus.h>
#include <linux/videodev2.h>
#include <linux/videodev2_nxp_media.h>

#include <nxp-v4l2.h>
#include <android-nxp-v4l2.h>
#include <gralloc_priv.h>

#define CHECK_COMMAND(command) do { \
        int ret = command; \
        if (ret < 0) { \
            ALOGE("func %s line %d error!!!\n", __func__, __LINE__); \
            return ret; \
        } \
    } while (0)
using namespace android;

#define BUFFER_COUNT    6

static int camera_init(int module, int width, int height, bool is_mipi)
{
    int clipper_id = module == 0 ? nxp_v4l2_clipper0 : nxp_v4l2_clipper1;
    int sensor_id = module == 0 ? nxp_v4l2_sensor0 : nxp_v4l2_sensor1;

    CHECK_COMMAND(v4l2_set_format(clipper_id, width, height, V4L2_PIX_FMT_YUV420));
    CHECK_COMMAND(v4l2_set_crop(clipper_id, 0, 0, width, height));
    CHECK_COMMAND(v4l2_set_format(sensor_id, width, height, V4L2_MBUS_FMT_YUYV8_2X8));
    if (is_mipi)
        CHECK_COMMAND(v4l2_set_format(nxp_v4l2_mipicsi, width, height, V4L2_MBUS_FMT_YUYV8_2X8));
    CHECK_COMMAND(v4l2_reqbuf(clipper_id, BUFFER_COUNT));
    return 0;
}

static int camera_run(int module, int width, int height, bool is_mipi, SurfaceComposerClient *client)
{
    int ret = camera_init(module, width, height, is_mipi);
    if (ret) {
        ALOGE("failed to camera_init(%d, %d, %d)", module, width, height);
        return ret;
    }

    sp<SurfaceControl> surfaceControl = client->createSurface(String8::format("CameraNTest%d", module),
            width, height, HAL_PIXEL_FORMAT_YV12, ISurfaceComposerClient::eFXSurfaceNormal);
    if (surfaceControl == NULL) {
        ALOGE("failed to createSurface");
        return -1;
    }

    sp<Surface> surface = surfaceControl->getSurface();
    SurfaceComposerClient::openGlobalTransaction();
    surfaceControl->show();
    surfaceControl->setLayer(99999);
    SurfaceComposerClient::closeGlobalTransaction();

    sp<ANativeWindow> window(surface);

    ret = native_window_set_buffer_count(window.get(), BUFFER_COUNT + 2);
    if (ret) {
        ALOGE("failed to native_window_set_buffer_count(): ret %d", ret);
        return ret;
    }

    ret = native_window_set_scaling_mode(window.get(), NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW);
    if (ret) {
        ALOGE("failed to native_window_set_scaling_mode(): ret %d", ret);
        return ret;
    }

    ret = native_window_set_usage(window.get(), GRALLOC_USAGE_HW_CAMERA_WRITE);
    if (ret) {
        ALOGE("failed to native_window_set_usage(): ret %d", ret);
        return ret;
    }

    ret = native_window_set_buffers_geometry(window.get(), width, height, HAL_PIXEL_FORMAT_YV12);
    if (ret) {
        ALOGE("failed to native_window_set_buffers_geometry(): ret %d", ret);
        return -1;
    }

    ANativeWindowBuffer *anBuffer[BUFFER_COUNT];
    private_handle_t const *handle[BUFFER_COUNT];

    int clipper_id = module == 0 ? nxp_v4l2_clipper0 : nxp_v4l2_clipper1;

    for (int i = 0; i < BUFFER_COUNT; i++) {
        ret = native_window_dequeue_buffer_and_wait(window.get(), &anBuffer[i]);
        if (ret) {
            ALOGE("failed to native_window_dequeue_buffer_and_wait(): ret %d", ret);
            return ret;
        }
        handle[i] = reinterpret_cast<private_handle_t const *>(anBuffer[i]->handle);
        CHECK_COMMAND(v4l2_qbuf(clipper_id, 1, i, handle[i], -1, NULL));
    }

    CHECK_COMMAND(v4l2_streamon(clipper_id));

    ANativeWindowBuffer *tempBuffer;
    int capture_index;
    int count = 1000;
    while (count--) {
        ret = v4l2_dqbuf(clipper_id, 1, &capture_index, NULL);
        if (ret) {
            ALOGE("v4l2_dqbuf ret %d, capture_index %d", ret, capture_index);
            break;
        }
        window->queueBuffer(window.get(), anBuffer[capture_index], -1);
        ret = native_window_dequeue_buffer_and_wait(window.get(), &tempBuffer);
        if (ret) {
            ALOGE("failed to native_window_dequeue_buffer_and_wait(): ret %d, line %d", ret, __LINE__);
            break;
        }
        ret = v4l2_qbuf(clipper_id, 1, capture_index, handle[capture_index], -1, NULL);
        if (ret) {
            ALOGE("v4l2_qbuf ret %d, capture_index %d", ret, capture_index);
            break;
        }
    }

    CHECK_COMMAND(v4l2_streamoff(clipper_id));
    return 0;
}

int main(int argc, char *argv[])
{
    argc = argc;
    argv++;

    if (false == android_nxp_v4l2_init()) {
        ALOGE("failed to android_nxp_v4l2_init");
        return -1;
    }

    sp<ProcessState> proc(ProcessState::self());
    ProcessState::self()->startThreadPool();

    sp<SurfaceComposerClient> client = new SurfaceComposerClient();
    if (client == NULL) {
        ALOGE("failed to new SurfaceComposerClient()");
        return -1;
    }

#if 0
    ALOGD("start front camera");
    camera_run(0, 720, 480, true, client.get());
#else
    ALOGD("start backward camera");
    camera_run(1, 704, 480, false, client.get());
#endif
    IPCThreadState::self()->joinThreadPool();
    return 0;
}
