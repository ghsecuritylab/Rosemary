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

static int copy_native_window_buffer(GraphicBufferMapper &mapper,
        ANativeWindowBuffer *from,
        ANativeWindowBuffer *to,
        int width,
        int height)
{
    unsigned char *pSrcYVirt, *pSrcCbVirt, *pSrcCrVirt;
    unsigned char *pDstYVirt, *pDstCbVirt, *pDstCrVirt;
    Rect bounds(width, height);

    int y_stride, y_vstride;
    int c_stride, c_vstride;

    y_stride = ALIGN(width, 32);
    y_vstride = ALIGN(height, 16);
    c_stride = ALIGN(y_stride >> 1, 16);
    c_vstride = ALIGN(y_vstride >> 1, 16);

    int ret;
    ret = mapper.lock(from->handle, GRALLOC_USAGE_SW_WRITE_OFTEN, bounds, (void **)&pSrcYVirt);
    if (ret) {
        ALOGE("failed to lock from %p", from);
        return -1;
    }
    ret = mapper.lock(to->handle, GRALLOC_USAGE_SW_WRITE_OFTEN, bounds, (void **)&pDstYVirt);
    if (ret) {
        ALOGE("failed to lock from %p", to);
        return -1;
    }

    pSrcCbVirt = pSrcYVirt + (y_stride * y_vstride);
    pSrcCrVirt = pSrcCbVirt + (c_stride * c_vstride);

    pDstCbVirt = pDstYVirt + (y_stride * y_vstride);
    pDstCrVirt = pDstCbVirt + (c_stride * c_vstride);

    // copy y
    unsigned char *pSrc, *pDst;
    pSrc = pSrcYVirt;
    pDst = pDstYVirt;
    for (int i = 0; i < height; i++) {
        memcpy(pDst, pSrc, width);
        pSrc += y_stride;
        pDst += y_stride;
    }

    // copy cb
    pSrc = pSrcCbVirt;
    pDst = pDstCbVirt;
    for (int i = 0; i < (height/2); i++) {
         memcpy(pDst, pSrc, width/2);
         pSrc += c_stride;
         pDst += c_stride;
    }

    // copy cr
    pSrc = pSrcCrVirt;
    pDst = pDstCrVirt;
    for (int i = 0; i < (height/2); i++) {
         memcpy(pDst, pSrc, width/2);
         pSrc += c_stride;
         pDst += c_stride;
    }

    mapper.unlock(to->handle);
    mapper.unlock(from->handle);
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

    // second surface
#if 1
    sp<SurfaceControl> surfaceControl2 = client->createSurface(String8::format("CameraCopy%d", module),
            width, height, HAL_PIXEL_FORMAT_YV12, ISurfaceComposerClient::eFXSurfaceNormal);
    if (surfaceControl2 == NULL) {
        ALOGE("failed to createSurface");
        return -1;
    }

    surfaceControl2->setPosition(1024-width, 0);

    sp<Surface> surface2 = surfaceControl2->getSurface();
    SurfaceComposerClient::openGlobalTransaction();
    surfaceControl2->show();
    surfaceControl2->setLayer(99999);
    SurfaceComposerClient::closeGlobalTransaction();


    sp<ANativeWindow> window2(surface2);

    ret = native_window_set_buffer_count(window2.get(), BUFFER_COUNT + 2);
    if (ret) {
        ALOGE("failed to native_window_set_buffer_count(): ret %d", ret);
        return ret;
    }

    ret = native_window_set_scaling_mode(window2.get(), NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW);
    if (ret) {
        ALOGE("failed to native_window_set_scaling_mode(): ret %d", ret);
        return ret;
    }

    ret = native_window_set_usage(window2.get(), GRALLOC_USAGE_HW_CAMERA_WRITE);
    if (ret) {
        ALOGE("failed to native_window_set_usage(): ret %d", ret);
        return ret;
    }

    ret = native_window_set_buffers_geometry(window2.get(), width, height, HAL_PIXEL_FORMAT_YV12);
    if (ret) {
        ALOGE("failed to native_window_set_buffers_geometry(): ret %d", ret);
        return -1;
    }
#endif

    CHECK_COMMAND(v4l2_streamon(clipper_id));

    GraphicBufferMapper &mapper = GraphicBufferMapper::get();
    Rect bounds(width, height);

    ANativeWindowBuffer *tempBuffer;
    ANativeWindowBuffer *tempBuffer2;
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
#if 1
        ret = native_window_dequeue_buffer_and_wait(window2.get(), &tempBuffer2);
        copy_native_window_buffer(mapper, anBuffer[capture_index], tempBuffer2, width, height);
        window2->queueBuffer(window2.get(), tempBuffer2, -1);
#endif

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
    camera_run(1, 720, 480, true, client.get());
#endif
    ALOGD("start backward camera");
    camera_run(0, 704, 480, false, client.get());

    IPCThreadState::self()->joinThreadPool();
    return 0;
}
