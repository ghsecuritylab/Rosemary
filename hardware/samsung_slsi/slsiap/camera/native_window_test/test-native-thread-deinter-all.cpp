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

#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <hardware_legacy/uevent.h>

#include <NXAllocator.h>

#include "NXDeinterlacerManager.h"

#define CHECK_COMMAND(command) do { \
        int ret = command; \
        if (ret < 0) { \
            ALOGE("func %s line %d error!!!\n", __func__, __LINE__); \
            return ret; \
        } \
    } while (0)
using namespace android;

#define BUFFER_COUNT    6
//#define BUFFER_COUNT    32

static int copy_deinter_to_native(GraphicBufferMapper &mapper,
        struct nxp_vid_buffer *from,
        ANativeWindowBuffer *to,
        int width,
        int height)
{
    unsigned char *pSrcYVirt, *pSrcCbVirt, *pSrcCrVirt;
    unsigned char *pDstYVirt, *pDstCbVirt, *pDstCrVirt;
    Rect bounds(width, height);

    int y_stride = ALIGN(width, 32);
    int y_vstride = ALIGN(height, 16);
    int c_stride = ALIGN(y_stride >> 1, 16);
    int c_vstride = ALIGN(y_vstride >> 1, 16);

    int ret = mapper.lock(to->handle, GRALLOC_USAGE_SW_WRITE_OFTEN, bounds, (void **)&pDstYVirt);
    if (ret) {
        ALOGE("failed to lock to %p", to);
        return -1;
    }
    pDstCbVirt = pDstYVirt + (y_stride * y_vstride);
    pDstCrVirt = pDstCbVirt + (c_stride * c_vstride);

    pSrcYVirt = (unsigned char *)from->virt[0];
    pSrcCbVirt = (unsigned char *)from->virt[1];
    pSrcCrVirt = (unsigned char *)from->virt[2];

    // copy y
    unsigned char *pSrc, *pDst;
    pSrc = pSrcYVirt;
    pDst = pDstYVirt;
    for (int i = 0; i < height; i++) {
        memcpy(pDst, pSrc, width);
        pSrc += 1024;
        pDst += y_stride;
    }

    // copy cb
    pSrc = pSrcCbVirt;
    pDst = pDstCbVirt;
    for (int i = 0; i < (height/2); i++) {
         memcpy(pDst, pSrc, width/2);
         pSrc += 512;
         pDst += c_stride;
    }

    // copy cr
    pSrc = pSrcCrVirt;
    pDst = pDstCrVirt;
    for (int i = 0; i < (height/2); i++) {
         memcpy(pDst, pSrc, width/2);
         pSrc += 512;
         pDst += c_stride;
    }

    mapper.unlock(to->handle);
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

static int save_source_to_file(struct nxp_vid_buffer *psrc, char *filename)
{
    //int fd = open(filename, O_WRONLY);
    int fd = creat(filename, 0777);
    if (fd >= 0) {
        char *pY, *pCb, *pCr;

        pY = psrc->virt[0];
        pCb = psrc->virt[1];
        pCr = psrc->virt[2];

        int i;
        for (i = 0; i < 480; i++) {
            write(fd, pY, 704);
            pY += YUV_YSTRIDE(704);
        }

        for (i = 0; i < 240; i++) {
            write(fd, pCb, 704/2);
            pCb += YUV_STRIDE(704/2);
        }

        for (i = 0; i < 240; i++) {
            write(fd, pCr, 704/2);
            pCr += YUV_STRIDE(704/2);
        }

        close(fd);
    }

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

    // HACK!!!
    CHECK_COMMAND(v4l2_streamoff(clipper_id));

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
    // psw0523 HACK!!!
#if 1
    if (module == 0) {
        surfaceControl->setPosition(0, 0);
    } else {
        surfaceControl->setPosition(1024-width, 0);
    }
#else
    surfaceControl->setPosition(0, 0);
    surfaceControl->setSize(1024, 600);
#endif
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

    if (module == 0) {
        ret = native_window_set_buffers_geometry(window.get(), width, height, HAL_PIXEL_FORMAT_YV12);
        if (ret) {
            ALOGE("failed to native_window_set_buffers_geometry(): ret %d", ret);
            return -1;
        }

        NXDeinterlacerManager *manager = new NXDeinterlacerManager(width, height/2);
        manager->setSrcType(NXDeinterlacerManager::SRC_TYPE_FIELD);
        manager->setStartField(NXDeinterlacerManager::FIELD_ODD);

        struct nxp_vid_buffer *buf;

        struct nxp_vid_buffer deinter_buffer[BUFFER_COUNT];
        if (false == allocateBuffer(deinter_buffer, BUFFER_COUNT, 1024, 480, PIXFORMAT_YUV420_PLANAR)) {
            ALOGE("failed to allocateBuffer for deinterlacer");
            return -1;
        }

        for (int i = 0; i < BUFFER_COUNT; i++) {
            buf = &deinter_buffer[i];
            manager->qDstBuf(buf);
        }

        struct nxp_vid_buffer camera_buffer[BUFFER_COUNT];
        if (false == allocateBuffer(camera_buffer, BUFFER_COUNT, width, height, PIXFORMAT_YUV420_PLANAR)) {
            ALOGE("failed to allocateBuffer for camera");
            return -1;
        }

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
        int count = 2000;
        int capture_index;

        struct nxp_vid_buffer *pDeinterBuf;
        //while (count--) {
        while (1) {
            ret = v4l2_dqbuf(clipper_id, plane_num, &capture_index, NULL);
            if (ret) {
                ALOGE("v4l2_dqbuf ret %d, capture_index %d", ret, capture_index);
                break;
            }

            buf = &camera_buffer[capture_index];
            manager->qSrcBuf(capture_index, buf);
            if (manager->run()) {
                for (int i = 0; i < manager->getRunCount(); i++) {
                    manager->dqDstBuf(&pDeinterBuf);
                    ret = native_window_dequeue_buffer_and_wait(window.get(), &tempBuffer);
                    if (ret) {
                        ALOGE("failed to native_window_dequeue_buffer_and_wait(): ret %d, line %d", ret, __LINE__);
                        break;
                    }

                    copy_deinter_to_native(mapper, pDeinterBuf, tempBuffer, width, height);
                    window->queueBuffer(window.get(), tempBuffer, -1);
                    manager->qDstBuf(pDeinterBuf);
                }
            }

            if (manager->dqSrcBuf(&capture_index, &buf)) {
                ret = v4l2_qbuf(clipper_id, plane_num, capture_index, buf, -1, NULL);
                if (ret) {
                    ALOGE("v4l2_qbuf ret %d, capture_index %d", ret, capture_index);
                    break;
                }
            }
        }
    } else {
        class NXDeinterlacerManager *manager = new NXDeinterlacerManager(width, height/2);
        GraphicBufferMapper &mapper = GraphicBufferMapper::get();


        // module 1 : deinterlace camera input to native window
        int deinter_width = width;
        int deinter_height = height;

        ret = native_window_set_buffers_geometry(window.get(), deinter_width, deinter_height, HAL_PIXEL_FORMAT_YV12);
        if (ret) {
            ALOGE("failed to native_window_set_buffers_geometry(): ret %d", ret);
            return -1;
        }

        ANativeWindowBuffer *anBuffer[BUFFER_COUNT];
        private_handle_t const *handle[BUFFER_COUNT];

        struct nxp_vid_buffer *buf;
        struct nxp_vid_buffer deinter_buffer[BUFFER_COUNT];
        if (false == allocateBuffer(deinter_buffer, BUFFER_COUNT, 1024, 480, PIXFORMAT_YUV420_PLANAR)) {
            ALOGE("failed to allocateBuffer for deinterlacer");
            return -1;
        }

        for (int i = 0; i < BUFFER_COUNT; i++) {
            buf = &deinter_buffer[i];
            manager->qDstBuf(buf);
        }

        struct nxp_vid_buffer camera_buffer[BUFFER_COUNT];
        if (false == allocateBuffer(camera_buffer, BUFFER_COUNT, width, height, PIXFORMAT_YUV420_PLANAR)) {
            ALOGE("failed to allocateBuffer for camera");
            return -1;
        }

        for (int i = 0; i < BUFFER_COUNT; i++) {
            buf = &camera_buffer[i];
            CHECK_COMMAND(v4l2_qbuf(clipper_id, buf->plane_num, i, buf, -1, NULL));
        }

        int plane_num = buf->plane_num;

        CHECK_COMMAND(v4l2_streamon(clipper_id));

        ANativeWindowBuffer *tempBuffer;
        int capture_index;
        int count = 2000;
        //int count = 200;

        struct nxp_vid_buffer *pDeinterBuf;
        //while (count--) {
        while (1) {
            ret = v4l2_dqbuf(clipper_id, plane_num, &capture_index, NULL);
            if (ret) {
                ALOGE("v4l2_dqbuf ret %d, capture_index %d", ret, capture_index);
                break;
            }
            ALOGD("camera dq end: %d", capture_index);

            buf = &camera_buffer[capture_index];
            manager->qSrcBuf(capture_index, buf);

            if (manager->run()) {
                for (int i = 0; i < manager->getRunCount(); i++) {
                    manager->dqDstBuf(&pDeinterBuf);
                    ret = native_window_dequeue_buffer_and_wait(window.get(), &tempBuffer);
                    if (ret) {
                        ALOGE("failed to native_window_dequeue_buffer_and_wait(): ret %d, line %d", ret, __LINE__);
                        break;
                    }

                    copy_deinter_to_native(mapper, pDeinterBuf, tempBuffer, width, height);
                    window->queueBuffer(window.get(), tempBuffer, -1);
                    manager->qDstBuf(pDeinterBuf);
                }
            }

            if (manager->dqSrcBuf(&capture_index, &buf)) {
                ret = v4l2_qbuf(clipper_id, plane_num, capture_index, buf, -1, NULL);
                if (ret) {
                    ALOGE("v4l2_qbuf ret %d, capture_index %d", ret, capture_index);
                    break;
                }
                //ALOGD("capture index %d", capture_index);
            }
        }

#if 0
        //static int capture_count = 0;
        char save_file_name[256] = {0, };
        //char command[128] = {0, };
        //sprintf(command, "mkdir /mnt/shell/emulated/0/capture-%d", capture_count);
        //system(command);
        //sync();
        for (int i = 0; i < BUFFER_COUNT; i++) {
            //sprintf(save_file_name, "/mnt/shell/emulated/0/capture-%d/%s-%d", capture_count, "tw9992", i);
            sprintf(save_file_name, "/mnt/shell/emulated/0/capture/%s-%d", "tw9992", i);
            buf = &camera_buffer[i];
            save_source_to_file(buf, save_file_name);
        }
        //capture_count++;
        sync();
#endif
    }


    CHECK_COMMAND(v4l2_streamoff(clipper_id));
    return 0;
}

static void handle_change_event(const char *buf, int len)
{
    const char *s = buf;
    s += strlen(s) + 1;
    int on = 0;

    while (*s) {
        if (!strncmp(s, "SWITCH_STATE=", strlen("SWITCH_STATE=")))
            on = atoi(s + strlen("SWITCH_STATE=")) == 1;

        s += strlen(s) + 1;
        if (s - buf >= len)
            break;
    }

    ALOGD("on: %d", on);
}

static void *backward_camera_monitoring_thread(void *data)
{
    const char *state_file = "/sys/devices/virtual/switch/tw9900/state";
    const char *change_event = "change@/devices/virtual/switch/tw9900";

    int state_fd = open(state_file, O_RDONLY);
    if (state_fd < 0) {
         ALOGE("failed to open %s", state_file);
         return NULL;
    }
    char val;
    if (read(state_fd, &val, 1) == 1 && val == '1') {
        ALOGD("current backward camera state is on");
    } else {
        ALOGD("current backward camera state is off");
    }

    char uevent_desc[4096];
    memset(uevent_desc, 0, sizeof(uevent_desc));
    uevent_init();

    struct pollfd pollfd;
    pollfd.fd = uevent_get_fd();
    pollfd.events = POLLIN;

    while(true) {
        int err = poll(&pollfd, 1, -1);
        if (err > 0) {
            if (pollfd.revents & POLLIN) {
                int len = uevent_next_event(uevent_desc, sizeof(uevent_desc) - 2);
                bool change = !strcmp(uevent_desc, change_event);
                if (change)
                    handle_change_event(uevent_desc, len);
            }
        } else if (err == -1) {
             if (errno == EINTR) break;
        }
    }

    return NULL;
}

struct thread_data {
    int module;
    int width;
    int height;
    bool is_mipi;
    SurfaceComposerClient *client;
};

static void *camera_thread(void *data)
{
    struct thread_data *p = (struct thread_data *)data;
    ALOGD("camera thread: module %d, %dX%d, is_mipi %d, client %p", p->module, p->width, p->height, p->is_mipi, p->client);
    camera_run(p->module, p->width, p->height, p->is_mipi, p->client);
    ALOGD("exit %d", p->module);
    return NULL;
}

static struct thread_data s_thread_data0;
static struct thread_data s_thread_data1;
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

    int ret;

    s_thread_data1.module = 1;
    s_thread_data1.width = 720;
    s_thread_data1.height = 480;
    s_thread_data1.is_mipi = true;
    s_thread_data1.client = client.get();
    pthread_t camera1_thread;
    ret = pthread_create(&camera1_thread, NULL, camera_thread, &s_thread_data1);
    if (ret) {
        ALOGE("failed to start camera1 thread: %s", strerror(ret));
        return ret;
    }

    s_thread_data0.module = 0;
    s_thread_data0.width = 704;
    s_thread_data0.height = 480;
    s_thread_data0.is_mipi = false;
    s_thread_data0.client = client.get();
    pthread_t camera0_thread;
    ret = pthread_create(&camera0_thread, NULL, camera_thread, &s_thread_data0);
    if (ret) {
        ALOGE("failed to start camera0 thread: %s", strerror(ret));
        return ret;
    }

    pthread_t backward_camera_monitor_thread;
    ret = pthread_create(&backward_camera_monitor_thread, NULL, backward_camera_monitoring_thread, NULL);
    if (ret) {
        ALOGE("failed to start backward camera monitoring thread: %s", strerror(ret));
        return ret;
    }

    IPCThreadState::self()->joinThreadPool();
    return 0;
}
