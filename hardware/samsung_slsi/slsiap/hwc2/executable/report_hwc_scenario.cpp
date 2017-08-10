#include <cutils/log.h>
#include <cutils/properties.h>
#include "../service/NXHWCService.h"

#ifdef LOLLIPOP
#define HWC_PROPERTY_RESOLUTION_KEY    "persist.hwc.resolution"
#else
#define HWC_PROPERTY_SCENARIO_KEY      "hwc.scenario"
#define HWC_PROPERTY_RESOLUTION_KEY    "hwc.resolution"
#define HWC_PROPERTY_SCALE_KEY         "hwc.scale"
#define HWC_PROPERTY_SCREEN_DOWNSIZING "hwc.screendownsizing"
#endif

using namespace android;

static int get_hwc_property(const char *key, uint32_t *val, const char *defaultVal)
{
    int len;
    char buf[PROPERTY_VALUE_MAX];
    len = property_get(key, buf, defaultVal);
    if (len <= 0)
        *val = 0;
    else
        *val = atoi(buf);

    return len;
}

int main(__attribute__((__unused__)) int argc, __attribute__((__unused__)) char *argv[])
{
    sp<INXHWCService> hwcService = getNXHWCService();
    uint32_t val;
    get_hwc_property(HWC_PROPERTY_RESOLUTION_KEY, &val, (const char *)"18");
    hwcService->hwcResolutionChanged(val);
#ifndef LOLLIPOP
    get_hwc_property(HWC_PROPERTY_SCENARIO_KEY, &val, (const char *)"0");
    hwcService->hwcScenarioChanged(val);
    get_hwc_property(HWC_PROPERTY_SCALE_KEY, &val, (const char *)"3");
    hwcService->hwcRescScaleFactorChanged(val);
    get_hwc_property(HWC_PROPERTY_SCREEN_DOWNSIZING, &val, (const char *)"0");
    hwcService->hwcScreenDownSizingChanged(val);
#endif
    return 0;
}
