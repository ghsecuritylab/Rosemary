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


LOCAL_PATH := $(call my-dir)

# HAL module implemenation stored in
# hw/<OVERLAY_HARDWARE_MODULE_ID>.<ro.product.board>.so
include $(CLEAR_VARS)

ifeq ($(strip $(TARGET_ARCH)),arm)
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
else
LOCAL_MODULE_RELATIVE_PATH := hw
endif

LOCAL_SHARED_LIBRARIES := liblog libcutils libion libutils libion-nexell libnxutil

LOCAL_C_INCLUDES := system/core/include $(LOCAL_PATH)/../include

LOCAL_SRC_FILES := 	\
	gralloc.cpp 	\
	framebuffer_device.cpp \
	mapper.cpp
	
LOCAL_MODULE := gralloc.${TARGET_BOARD_PLATFORM}
LOCAL_CFLAGS := -DLOG_TAG=\"gralloc\"
LOCAL_MODULE_TAGS := optional

ANDROID_VERSION_STR := $(subst ., ,$(PLATFORM_VERSION))
ANDROID_VERSION_MAJOR := $(firstword $(ANDROID_VERSION_STR))
ifeq "5" "$(ANDROID_VERSION_MAJOR)"
#@echo This is LOLLIPOP!!!
LOCAL_C_INCLUDES += system/core/libion/include
LOCAL_CFLAGS += -DLOLLIPOP
endif

kernel_patch_level := $(shell cat kernel/Makefile | grep "PATCHLEVEL =" | cut -f 3 -d ' ')
#$(warning kernel_patch_level: $(kernel_patch_level))
ifeq ($(strip $(kernel_patch_level)),18)
LOCAL_CFLAGS += -DARM64
endif

include $(BUILD_SHARED_LIBRARY)
