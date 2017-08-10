LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional
LOCAL_C_INCLUDES += \
	bootable/recovery \
	hardware/samsung_slsi/slsiap/ota
LOCAL_SRC_FILES := \
	../../../../hardware/samsung_slsi/slsiap/ota/spieeprom.c \
	../../../../hardware/samsung_slsi/slsiap/ota/update.c \
	recovery_updater.c

LOCAL_MODULE := librecovery_updater_lepus

include $(BUILD_STATIC_LIBRARY)
