LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
#LOCAL_PRELINK_MODULE := false
#LOCAL_MODULE_PATH := $(TARGET_OUT_EXECUTABLES)
LOCAL_SRC_FILES := rtw_fwloader.c
LOCAL_MODULE := rtw_fwloader
LOCAL_MODULE_TAGS := optional
LOCAL_SHARED_LIBRARIES := liblog libcutils
include $(BUILD_EXECUTABLE)
