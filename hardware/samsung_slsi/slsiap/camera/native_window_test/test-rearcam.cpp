#define LOG_TAG "camera-rearcam-test"

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
#include <NXAllocator.h>

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

    CHECK_COMMAND(v4l2_set_format(clipper_id, width, height, V4L2_PIX_FMT_YUV420M));
    CHECK_COMMAND(v4l2_set_crop(clipper_id, 0, 0, width, height));
    CHECK_COMMAND(v4l2_set_format(sensor_id, width, height, V4L2_MBUS_FMT_YUYV8_2X8));
    if (is_mipi)
        CHECK_COMMAND(v4l2_set_format(nxp_v4l2_mipicsi, width, height, V4L2_MBUS_FMT_YUYV8_2X8));
    CHECK_COMMAND(v4l2_reqbuf(clipper_id, BUFFER_COUNT));
    return 0;
}

static int copy_camerabuffer_to_nativebuffer(GraphicBufferMapper &mapper,
        int width,
        int height,
        struct nxp_vid_buffer *buf,
        ANativeWindowBuffer *to)
{
    unsigned char *pSrcY = (unsigned char *)buf->virt[0];
    unsigned char *pSrcCb = (unsigned char *)buf->virt[1];
    unsigned char *pSrcCr = (unsigned char *)buf->virt[2];

    int srcYStride = buf->stride[0];
    int srcCStride = buf->stride[1];

    unsigned char *pDstY, *pDstCb, *pDstCr;
    int dstYStride = ALIGN(width, 32);
    int dstYVStride = ALIGN(height, 16);
    int dstCStride = ALIGN(dstYStride >> 1, 16);
    int dstCVStride = ALIGN(dstYVStride >> 1, 16);

    int ret;
    Rect bounds(width, height);

    ret = mapper.lock(to->handle, GRALLOC_USAGE_SW_WRITE_OFTEN, bounds, (void **)&pDstY);
    if (ret) {
        ALOGE("failed to lock to %p", to);
        return -1;
    }

    pDstCb = pDstY + (dstYStride * dstYVStride);
    pDstCr = pDstCb + (dstCStride * dstCVStride);

    unsigned char *pSrc, *pDst;
    pSrc = pSrcY;
    pDst = pDstY;
    for (int i = 0; i < height; i++) {
        memcpy(pDst, pSrc, width);
        pSrc += srcYStride;
        pDst += dstYStride;
    }

    pSrc = pSrcCb;
    pDst = pDstCb;
    for (int i = 0; i < (height/2); i++) {
         memcpy(pDst, pSrc, width/2);
         pSrc += srcCStride;
         pDst += dstCStride;
    }

    pSrc = pSrcCr;
    pDst = pDstCr;
    for (int i = 0; i < (height/2); i++) {
         memcpy(pDst, pSrc, width/2);
         pSrc += srcCStride;
         pDst += dstCStride;
    }

    mapper.unlock(to->handle);
    return 0;
}

static int camera_run(int module, int width, int height, bool is_mipi, SurfaceComposerClient *client)
{
    int ret = camera_init(module, width, height, is_mipi);
    if (ret) {
        ALOGE("failed to camera_init(%d, %d, %d)", module, width, height);
        return ret;
    }

    int clipper_id = module == 0 ? nxp_v4l2_clipper0 : nxp_v4l2_clipper1;

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
    surfaceControl->setPosition(0, 0);
    surfaceControl->setSize(1024, 600);

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

    struct nxp_vid_buffer camera_buffer[BUFFER_COUNT];
    if (false == allocateBuffer(camera_buffer, BUFFER_COUNT, width, height, PIXFORMAT_YUV420_PLANAR)) {
        ALOGE("failed to allocateBuffer for camera");
        return -1;
    }

    struct nxp_vid_buffer *buf;
    for (int i = 0; i < BUFFER_COUNT; i++) {
        buf = &camera_buffer[i];
        CHECK_COMMAND(v4l2_qbuf(clipper_id, buf->plane_num, i, buf, -1, NULL));
    }

    int plane_num = buf->plane_num;

    CHECK_COMMAND(v4l2_streamon(clipper_id));

    GraphicBufferMapper &mapper = GraphicBufferMapper::get();
    Rect bounds(width, height);

    ANativeWindowBuffer *tempBuffer;
    //int count = 300;
    int count = 10000;
    int capture_index;

    while (count--) {
        ret = v4l2_dqbuf(clipper_id, plane_num, &capture_index, NULL);
        if (ret) {
            ALOGE("v4l2_dqbuf ret %d, capture_index %d", ret, capture_index);
            break;
        }

        buf = &camera_buffer[capture_index];
        ret = native_window_dequeue_buffer_and_wait(window.get(), &tempBuffer);
        if (ret) {
            ALOGE("failed to native_window_dequeue_buffer_and_wait(): ret %d, line %d", ret, __LINE__);
            break;
        }

        copy_camerabuffer_to_nativebuffer(mapper, width, height, buf, tempBuffer);

        window->queueBuffer(window.get(), tempBuffer, -1);

        ret = v4l2_qbuf(clipper_id, plane_num, capture_index, buf, -1, NULL);
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

    camera_run(0, 704, 480, false, client.get());

    return 0;
}
