# Copyright (C) 2008 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

ifeq ($(SLSIAP_HWC_VERSION),2)

LOCAL_PATH := $(call my-dir)

# hwc service
include $(CLEAR_VARS)
LOCAL_PRELINK_MODULE := false
ifeq ($(strip $(TARGET_ARCH)),arm)
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)
endif
LOCAL_SHARED_LIBRARIES := libutils libcutils libbinder
LOCAL_C_INCLUDES += frameworks/base/include system/core/include
LOCAL_SRC_FILES = service/NXHWCService.cpp
LOCAL_MODULE := libnxhwcservice
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)

# for hwc property reporting
include $(CLEAR_VARS)
LOCAL_PRELINK_MODULE := false
ifeq ($(strip $(TARGET_ARCH)),arm)
LOCAL_MODULE_PATH := $(TARGET_OUT_EXECUTABLES)
endif
LOCAL_SHARED_LIBRARIES := liblog libutils libnxhwcservice libcutils libbinder
LOCAL_CFLAGS += -DLOG_TAG=\"HWC_SCENARIO_REPORTER\"
ANDROID_VERSION_STR := $(subst ., ,$(PLATFORM_VERSION))
ANDROID_VERSION_MAJOR := $(firstword $(ANDROID_VERSION_STR))
ifeq "5" "$(ANDROID_VERSION_MAJOR)"
LOCAL_CFLAGS += -DLOLLIPOP
endif
LOCAL_C_INCLUDES += frameworks/native/include system/core/include
LOCAL_SRC_FILES := executable/report_hwc_scenario.cpp
LOCAL_MODULE := report_hwc_scenario
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)

# hwc
include $(CLEAR_VARS)
LOCAL_PRELINK_MODULE := false
ifeq ($(strip $(TARGET_ARCH)),arm)
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
else
LOCAL_MODULE_RELATIVE_PATH := hw
endif
LOCAL_SHARED_LIBRARIES := liblog libsync libEGL libcutils libhardware libhardware_legacy libnxhwcservice libutils libbinder libion-nexell libv4l2-nexell libion libnxutil
LOCAL_STATIC_LIBRARIES := libcec
LOCAL_CFLAGS += -DLOG_TAG=\"hwcomposer\"

LOCAL_C_INCLUDES += \
	frameworks/native/include \
	system/core/include \
	hardware/libhardware/include \
	$(LOCAL_PATH)/include \
	$(LOCAL_PATH)/../include

LOCAL_C_INCLUDES += linux/platform/$(TARGET_CPU_VARIANT2)/library/src/libcec

LOCAL_SRC_FILES := hwc.cpp \
	renderer/HWCCommonRenderer.cpp \
	renderer/LCDRGBRenderer.cpp \
	renderer/HDMIMirrorRenderer.cpp \
	impl/RescConfigure.cpp \
	impl/LCDCommonImpl.cpp \
	impl/LCDUseOnlyGLImpl.cpp \
	impl/LCDUseGLAndVideoImpl.cpp \
	impl/HDMICommonImpl.cpp \
	impl/HDMIUseOnlyMirrorImpl.cpp \
	impl/HDMIUseGLAndVideoImpl.cpp \
	impl/HDMIUseOnlyGLImpl.cpp \
	impl/HDMIUseMirrorAndVideoImpl.cpp \
	impl/HDMIUseRescCommonImpl.cpp \
	HWCreator.cpp

ANDROID_VERSION_STR := $(subst ., ,$(PLATFORM_VERSION))
ANDROID_VERSION_MAJOR := $(firstword $(ANDROID_VERSION_STR))
ifeq "5" "$(ANDROID_VERSION_MAJOR)"
#@echo This is LOLLIPOP!!!
LOCAL_C_INCLUDES += system/core/libion/include
LOCAL_CFLAGS += -DLOLLIPOP
endif

LOCAL_MODULE := hwcomposer.$(TARGET_BOARD_PLATFORM)
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

endif
