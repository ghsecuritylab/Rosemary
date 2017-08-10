LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_C_INCLUDES += \
	system/core/include \
	frameworks/native/include \
	$(LOCAL_PATH) \
	$(LOCAL_PATH)/../include

LOCAL_SRC_FILES := \
	NXAllocator.cpp \
	NXScaler.cpp \
	NXCpu.cpp \
	NXUtil.cpp \
	csc.cpp \
	NXCsc.cpp

ifeq ($(TARGET_ARCH),arm64)
LOCAL_CFLAGS := -DARM64=1
else
LOCAL_CFLAGS := -DARM64=0
endif

LOCAL_SHARED_LIBRARIES := liblog libutils libcutils libion-nexell libion

ANDROID_VERSION_STR := $(subst ., ,$(PLATFORM_VERSION))
ANDROID_VERSION_MAJOR := $(firstword $(ANDROID_VERSION_STR))
ifeq "5" "$(ANDROID_VERSION_MAJOR)"
#@echo This is LOLLIPOP!!!
LOCAL_C_INCLUDES += system/core/libion/include
LOCAL_CFLAGS += -DLOLLIPOP
LOCAL_SRC_FILES_arm += \
	csc_ARGB8888_to_NV12_NEON.s \
	csc_ARGB8888_to_NV21_NEON.s
endif

ifeq "4" "$(ANDROID_VERSION_MAJOR)"
LOCAL_SRC_FILES += \
	csc_ARGB8888_to_NV12_NEON.s \
	csc_ARGB8888_to_NV21_NEON.s
endif

LOCAL_MODULE := libnxutil

LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)
