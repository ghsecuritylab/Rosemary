#define LOG_TAG "tvout-brightness"

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
    // brightness : 0 ~ 127
    int brightness_val = 0;
    // contrast : -128 ~ 0
    int contrast_val = 0;
    
    int ret = 0;

    if (android_nxp_v4l2_init() == false) {
        ALOGE("failed to android_nxp_v4l2_init");
        return -1;
    }

    if (argc != 3) {
        printf("usage: %s [Brightness Value(0 ~ 127)] [Contrast Value(-128 ~ 0)\n", argv[0]);
        return -1;
    }

    brightness_val = atoi(argv[1]);
    contrast_val = atoi(argv[2]);

    if (brightness_val < 0 || brightness_val > 127) {
        printf("brightness value error!!(avalable value : 0 ~ 127)\n");
        return -1;
    }

    if (contrast_val > 0 || contrast_val < -128) {
        printf("contrast value error!!(avalable value : 0 ~ -128)\n");
        return -1;
    }

    printf("\n------------ Set Brightness ---------------\n");
    v4l2_set_ctrl(nxp_v4l2_tvout, V4L2_CID_BRIGHTNESS, brightness_val);
    printf("-------- Brightness Value : %d ------------\n", brightness_val);

    usleep(2000000);

    printf("\n------------ Set Contrast ---------------\n");
    v4l2_set_ctrl(nxp_v4l2_tvout, V4L2_CID_CONTRAST, contrast_val);
    printf("-------- Contrast Value : %d ------------\n", contrast_val);

    return 0;
}

