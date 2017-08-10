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

#define CHECK_COMMAND(command) do { \
        int ret = command; \
        if (ret < 0) { \
            ALOGE("func %s line %d error!!!\n", __func__, __LINE__); \
            return ret; \
        } \
    } while (0)
using namespace android;

//#define BUFFER_COUNT    6
#define BUFFER_COUNT    32

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
#if 0
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

    ret = native_window_set_buffers_geometry(window.get(), width, height, HAL_PIXEL_FORMAT_YV12);
    if (ret) {
        ALOGE("failed to native_window_set_buffers_geometry(): ret %d", ret);
        return -1;
    }

    // psw0523 test for rotate
#if 0
    ret = native_window_set_buffers_transform(window.get(), NATIVE_WINDOW_TRANSFORM_ROT_270);
    if (ret) {
        ALOGE("failed to native_window_set_buffers_transform(): ret %d", ret);
        return -1;
    }
#endif

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
    const char *state_file = "/sys/devices/virtual/switch/rearcam/state";
    const char *change_event = "change@/devices/virtual/switch/rearcam";

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

#if 1
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
#endif

#if 0
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
#endif

    pthread_t backward_camera_monitor_thread;
    ret = pthread_create(&backward_camera_monitor_thread, NULL, backward_camera_monitoring_thread, NULL);
    if (ret) {
        ALOGE("failed to start backward camera monitoring thread: %s", strerror(ret));
        return ret;
    }

    IPCThreadState::self()->joinThreadPool();
    return 0;
}
