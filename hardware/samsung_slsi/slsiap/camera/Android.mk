ifeq ($(BOARD_HAS_CAMERA),true)

LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

ifeq ($(strip $(TARGET_ARCH)),arm)
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
else
LOCAL_MODULE_RELATIVE_PATH := hw
endif

LOCAL_C_INCLUDES += \
	frameworks/native/include \
	system/media/camera/include \
	system/core/include \
	external/jpeg \
	$(LOCAL_PATH) \
	$(LOCAL_PATH)/../libcamerasensor \
	$(LOCAL_PATH)/../include

LOCAL_SRC_FILES := \
	NXCameraSensor.cpp \
	NXSensorThread.cpp \
	NXCommandThread.cpp \
	NXStream.cpp \
	NXStreamThread.cpp \
	NXZoomController.cpp \
	Exif.cpp \
	NXExifProcessor.cpp \
	ScalerZoomController.cpp \
	PreviewThread.cpp \
	CallbackThread.cpp \
	CaptureThread.cpp \
	RecordThread.cpp \
	InterlaceRecordThread.cpp \
	NXStreamManager.cpp \
	NXCameraHWInterface2.cpp

LOCAL_SHARED_LIBRARIES := liblog libutils libcutils libnxutil libion-nexell libv4l2-nexell libion libhardware \
	libcamera_client libcamera_metadata libNX_Jpeg libjpeg

ifeq ($(strip $(TARGET_ARCH)),arm)
LOCAL_SHARED_LIBRARIES += libNX_Jpeghw
LOCAL_CFLAGS += -DUSE_HW_JPEG
endif

LOCAL_STATIC_LIBRARIES := libcamera-$(TARGET_BOOTLOADER_BOARD_NAME) libcamerasensor

# use system timestamp
LOCAL_CFLAGS += -DUSE_SYSTEM_TIMESTAMP

ifeq ($(TARGET_CPU_VARIANT2),s5p4418)
LOCAL_SRC_FILES += W128BAScalerZoomController.cpp
LOCAL_CFLAGS += -DWORKAROUND_128BYTE_ALIGN
LOCAL_CFLAGS += -DARCH_S5P4418
endif

ifeq ($(TARGET_CPU_VARIANT2),s5p6818)
LOCAL_CFLAGS += -DARCH_S5P6818
endif

ANDROID_VERSION_STR := $(subst ., ,$(PLATFORM_VERSION))
ANDROID_VERSION_MAJOR := $(firstword $(ANDROID_VERSION_STR))
ifeq "5" "$(ANDROID_VERSION_MAJOR)"
#@echo This is LOLLIPOP!!!
LOCAL_C_INCLUDES += system/core/libion/include
LOCAL_CFLAGS += -DLOLLIPOP
endif


# use scaler zoom
# LOCAL_CFLAGS += -DUSE_SCALER_ZOOM

LOCAL_MODULE := camera.$(TARGET_BOARD_PLATFORM)

LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
# for test
LOCAL_SRC_FILES := test/verify.cpp
LOCAL_SHARED_LIBRARIES := libcutils libutils libv4l2-nexell libion-nexell libion
LOCAL_C_INCLUDES := frameworks/native/include \
	system/core/include \
	$(LOCAL_PATH)/../include
LOCAL_CFLAGS += -DLOG_TAG=\"camera_verify\"

ANDROID_VERSION_STR := $(subst ., ,$(PLATFORM_VERSION))
ANDROID_VERSION_MAJOR := $(firstword $(ANDROID_VERSION_STR))
ifeq "5" "$(ANDROID_VERSION_MAJOR)"
#@echo This is LOLLIPOP!!!
LOCAL_C_INCLUDES += system/core/libion/include
LOCAL_CFLAGS += -DLOLLIPOP
endif

LOCAL_MODULE := camera_verify
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
# for test
LOCAL_SRC_FILES := test/test-decimator.cpp
LOCAL_SHARED_LIBRARIES := libcutils libutils libv4l2-nexell libion-nexell libion
LOCAL_C_INCLUDES := frameworks/native/include \
	system/core/include \
	$(LOCAL_PATH)/../include
LOCAL_CFLAGS += -DLOG_TAG=\"test-decimator\"

ANDROID_VERSION_STR := $(subst ., ,$(PLATFORM_VERSION))
ANDROID_VERSION_MAJOR := $(firstword $(ANDROID_VERSION_STR))
ifeq "5" "$(ANDROID_VERSION_MAJOR)"
#@echo This is LOLLIPOP!!!
LOCAL_C_INCLUDES += system/core/libion/include
LOCAL_CFLAGS += -DLOLLIPOP
endif

LOCAL_MODULE := test_decimator
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
# for test
LOCAL_SRC_FILES := test/test-decimator-render.cpp
LOCAL_SHARED_LIBRARIES := libcutils libutils libv4l2-nexell libion-nexell libion
LOCAL_C_INCLUDES := frameworks/native/include \
	system/core/include \
	$(LOCAL_PATH)/../include
LOCAL_CFLAGS += -DLOG_TAG=\"test-decimator\"

ANDROID_VERSION_STR := $(subst ., ,$(PLATFORM_VERSION))
ANDROID_VERSION_MAJOR := $(firstword $(ANDROID_VERSION_STR))
ifeq "5" "$(ANDROID_VERSION_MAJOR)"
#@echo This is LOLLIPOP!!!
LOCAL_C_INCLUDES += system/core/libion/include
LOCAL_CFLAGS += -DLOLLIPOP
endif

LOCAL_MODULE := test_decimator_render
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)

endif

#LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := native_window_test/test-native.cpp
LOCAL_SHARED_LIBRARIES := libcutils libbinder libutils libgui libui libv4l2-nexell
LOCAL_C_INCLUDES := frameworks/native/include \
					system/core/include \
					hardware/libhardware/include  \
					$(LOCAL_PATH)/../include

LOCAL_MODULE := camera_native_test
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := native_window_test/test-native-copy.cpp
LOCAL_SHARED_LIBRARIES := libcutils libbinder libutils libgui libui libv4l2-nexell
LOCAL_C_INCLUDES := frameworks/native/include \
					system/core/include \
					hardware/libhardware/include  \
					$(LOCAL_PATH)/../include

LOCAL_MODULE := camera_native_test_copy
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)

#LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := native_window_test/test-native-thread.cpp
LOCAL_SHARED_LIBRARIES := libcutils libhardware_legacy libbinder libutils libgui libui libv4l2-nexell
LOCAL_C_INCLUDES := frameworks/native/include \
					system/core/include \
					hardware/libhardware/include  \
					$(LOCAL_PATH)/../include

LOCAL_MODULE := camera_thread_native_test
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := native_window_test/test-native-thread-deinter.cpp \
				   native_window_test/NXDeinterlacerManager.cpp

LOCAL_SHARED_LIBRARIES := libcutils libhardware_legacy libbinder libutils libgui libui libnxutil libion-nexell libv4l2-nexell libion
LOCAL_C_INCLUDES := frameworks/native/include \
					system/core/include \
					system/core/libion/include \
					hardware/libhardware/include  \
					hardware/samsung_slsi/slsiap/include \
					$(LOCAL_PATH)/../include

LOCAL_MODULE := camera_thread_deinter_native_test
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := native_window_test/test-switch.cpp
LOCAL_SHARED_LIBRARIES := libcutils libv4l2-nexell
LOCAL_C_INCLUDES := frameworks/native/include \
					system/core/include \
					hardware/libhardware/include  \
					$(LOCAL_PATH)/../include

LOCAL_MODULE := camera_test_switch
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := native_window_test/test-tvout-set.cpp
LOCAL_SHARED_LIBRARIES := libcutils libv4l2-nexell
LOCAL_C_INCLUDES := frameworks/native/include \
					system/core/include \
					hardware/libhardware/include  \
					$(LOCAL_PATH)/../include

LOCAL_MODULE := tvout_test_set
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)
