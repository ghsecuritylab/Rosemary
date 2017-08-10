#define LOG_TAG "camera-switch"

#include <stdlib.h>

#include <cutils/log.h>

#include <linux/media.h>
#include <linux/v4l2-subdev.h>
#include <linux/v4l2-mediabus.h>
#include <linux/videodev2.h>
#include <linux/videodev2_nxp_media.h>

#include <nxp-v4l2.h>
#include <android-nxp-v4l2.h>
#include <gralloc_priv.h>

#define V4L2_CID_MUX        (V4L2_CTRL_CLASS_USER | 0x1001)
#define V4L2_CID_STATUS     (V4L2_CTRL_CLASS_USER | 0x1002)

// status
// bit 0 : channel 0 status
// bit 1 : channel 1 status
int main(int argc, char *argv[])
{
    argc = argc;
    argv++;

    if (false == android_nxp_v4l2_init()) {
        ALOGE("failed to android_nxp_v4l2_init");
        return -1;
    }

    int count = 100;
    int mux_tw9900 = 0;
    int mux_tw9992 = 0;
    int status = 0;
    while (count--) {
        usleep(1000000);

        // tw9900
        v4l2_set_ctrl(nxp_v4l2_sensor0, V4L2_CID_MUX, mux_tw9900);
        mux_tw9900 ^= 1;
        usleep(200000);
        v4l2_get_ctrl(nxp_v4l2_sensor0, V4L2_CID_STATUS, &status);
        ALOGD("Sensor0 Status: %d", status);

        // tw9992
        v4l2_set_ctrl(nxp_v4l2_sensor1, V4L2_CID_MUX, mux_tw9992);
        mux_tw9992 ^= 1;
        usleep(500000);
        v4l2_get_ctrl(nxp_v4l2_sensor1, V4L2_CID_STATUS, &status);
        ALOGD("Sensor1 Status: %d", status);
    }

    return 0;
}
