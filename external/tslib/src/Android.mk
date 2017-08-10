LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_CFLAGS += -DDEBUG

LOCAL_C_INCLUDES := \
	external/tslib/ \
	external/tslib/src \
	external/tslib/android

LOCAL_SRC_FILES := \
	ts_attach.c ts_close.c ts_config.c \
	ts_fd.c ts_load_module.c ts_open.c ts_read.c \
	ts_parse_vars.c ts_read_raw_module.c ts_read_raw.c ts_error.c \
	../android/CalibrateTouchScreen.c

LOCAL_MODULE := libts
LOCAL_SHARED_LIBRARIES := libdl libcutils liblog

LOCAL_C_INCLUDES += dalvik/libnativehelper/include/nativehelper

# ghcstop
#LOCAL_LDLIBS += -lpthread

LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)

# ----------- For TS_PRINT ---------------------

include $(CLEAR_VARS)
LOCAL_C_INCLUDES := \
	external/tslib/ \
	external/tslib/src \
	external/tslib/tests/ \
	external/tslib/android

LOCAL_SRC_FILES := ../tests/ts_print.c

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := tsprint
LOCAL_SHARED_LIBRARIES := libts libdl

# ----------- For TS_TEST ------------------------

include $(CLEAR_VARS)
LOCAL_C_INCLUDES := \
	external/tslib/ \
	external/tslib/src \
	external/tslib/tests/ \
	external/tslib/android

LOCAL_SRC_FILES := ../tests/ts_test.c

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := tstest
LOCAL_SHARED_LIBRARIES := libts libdl libcutils

# ----------- For TEST_UTILS ---------------------

include $(CLEAR_VARS)
LOCAL_C_INCLUDES := \
	external/tslib/ \
	external/tslib/src \
	external/tslib/tests/ \
	external/tslib/android

LOCAL_SRC_FILES := ../tests/testutils.c

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := tsutils
LOCAL_SHARED_LIBRARIES := libts libdl

# ----------- For TS_CALIBRATE -------------------

include $(CLEAR_VARS)
LOCAL_C_INCLUDES := \
	external/tslib/ \
	external/tslib/src \
	external/tslib/tests/ \
	external/tslib/android

LOCAL_C_INCLUDES += dalvik/libnativehelper/include/nativehelper

LOCAL_SRC_FILES := ../android/CalibrateTouchScreen.c

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := tscalib
LOCAL_SHARED_LIBRARIES := libts libdl libcutils

#$(call add-prebuilt-files, ../ts.conf)
