ifeq ($(BOARD_WIFI_VENDOR),realtek)
#LOCAL_PATH := $(call my-dir)
#include $(call all-makefiles-under,$(LOCAL_PATH))
include $(call all-subdir-makefiles)
endif
