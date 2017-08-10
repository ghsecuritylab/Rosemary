#define LOG_TAG "camera-brightness"

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

int main(int argc, char *argv[])
{
    int brightness;

    if (argc < 2) {
        ALOGE("usage: test-brightness brightness-value\n");
        return 0;
    }

    brightness = atoi(argv[1]);

    if (false == android_nxp_v4l2_init()) {
        ALOGE("failed to android_nxp_v4l2_init");
        return -1;
    }

    ALOGD("set brightness: %d\n", brightness);
    v4l2_set_ctrl(nxp_v4l2_sensor0, V4L2_CID_BRIGHTNESS, brightness);
    v4l2_set_ctrl(nxp_v4l2_sensor1, V4L2_CID_BRIGHTNESS, brightness);

    return 0;
}
