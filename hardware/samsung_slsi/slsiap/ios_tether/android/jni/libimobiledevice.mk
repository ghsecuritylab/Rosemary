#libimobiledevice

LIB_USBMUXD_VERSION:=usbmuxd
LIB_PLIST_VERSION:=libplist
LIB_LIBUSBMUXD_VERSION:=libusbmuxd
LIB_OPENSSL_VERSION:=external/openssl/

LIB_VERSION:=libimobiledevice

LOCAL_PATH:= $(call my-dir)
LIB_ROOT_REL:= ../../$(LIB_VERSION)
LIB_ROOT_ABS:= $(LOCAL_PATH)/../../$(LIB_VERSION)

include $(CLEAR_VARS)

LOCAL_CFLAGS := -Wall -lpthread -DHAVE_OPENSSL -DHAVE_VASPRINTF -DHAVE_ASPRINTF -Wunused-function -D__ANDROID__

LOCAL_SRC_FILES := \
 $(LIB_ROOT_REL)/src/afc.c \
 $(LIB_ROOT_REL)/src/device_link_service.c \
 $(LIB_ROOT_REL)/src/diagnostics_relay.c \
 $(LIB_ROOT_REL)/src/file_relay.c \
 $(LIB_ROOT_REL)/src/heartbeat.c \
 $(LIB_ROOT_REL)/src/house_arrest.c \
 $(LIB_ROOT_REL)/src/idevice.c \
 $(LIB_ROOT_REL)/src/installation_proxy.c \
 $(LIB_ROOT_REL)/src/lockdown.c \
 $(LIB_ROOT_REL)/src/misagent.c \
 $(LIB_ROOT_REL)/src/mobile_image_mounter.c \
 $(LIB_ROOT_REL)/src/mobilebackup.c \
 $(LIB_ROOT_REL)/src/mobilebackup2.c \
 $(LIB_ROOT_REL)/src/mobilesync.c \
 $(LIB_ROOT_REL)/src/notification_proxy.c \
 $(LIB_ROOT_REL)/src/property_list_service.c \
 $(LIB_ROOT_REL)/src/restore.c \
 $(LIB_ROOT_REL)/src/sbservices.c \
 $(LIB_ROOT_REL)/src/screenshotr.c \
 $(LIB_ROOT_REL)/src/service.c \
 $(LIB_ROOT_REL)/src/syslog_relay.c \
 $(LIB_ROOT_REL)/src/webinspector.c \
 $(LIB_ROOT_REL)/common/debug.c \
 $(LIB_ROOT_REL)/common/userpref.c \
 $(LIB_ROOT_REL)/common/socket.c \
 $(LIB_ROOT_REL)/common/thread.c \
 $(LIB_ROOT_REL)/common/utils.c \
 $(LIB_ROOT_REL)/common/log.c

LOCAL_C_INCLUDES += \
 $(LIB_ROOT_ABS) \
 $(LIB_ROOT_ABS)/../../usbmuxd/out/toolchain/sysroot/usr/include \
 $(LIB_ROOT_ABS)/../../usbmuxd/out/toolchain/include/c++/4.8 \
 $(LIB_ROOT_ABS)/src \
 $(LIB_ROOT_ABS)/common \
 $(LIB_ROOT_ABS)/include \
 $(LIB_ROOT_ABS)/../$(LIB_PLIST_VERSION)/include \
 $(LIB_ROOT_ABS)/../$(LIB_LIBUSBMUXD_VERSION)/include \
 $(LIB_ROOT_ABS)/../$(LIB_LIBUSBMUXD_VERSION)/src \
 $(LIB_ROOT_ABS)/../$(LIB_USBMUXD_VERSION)/src \
 $(LIB_OPENSSL_VERSION) \
 $(LIB_OPENSSL_VERSION)/include \
 $(LIB_OPENSSL_VERSION)/ssl \
 $(LIB_OPENSSL_VERSION)/apps \
 $(LIB_OPENSSL_VERSION)/crypto

LOCAL_SHARED_LIBRARIES := libusbmuxd libplist libssl libcrypto

#LOCAL_LDLIBS := -llog

LOCAL_MODULE := libimobiledevice

include $(BUILD_SHARED_LIBRARY)

ifeq ($(NOT_BUILD),true)
#ideviceinfo

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
 $(LIB_ROOT_REL)/tools/ideviceinfo.c

LOCAL_C_INCLUDES += \
 $(LIB_ROOT_ABS) \
 $(LIB_ROOT_ABS)/../../usbmuxd/out/toolchain/sysroot/usr/include \
 $(LIB_ROOT_ABS)/../../usbmuxd/out/toolchain/include/c++/4.8 \
 $(LIB_ROOT_ABS)/src \
 $(LIB_ROOT_ABS)/include \
 $(LIB_ROOT_ABS)/../$(LIB_PLIST_VERSION)/include \

LOCAL_SHARED_LIBRARIES := libimobiledevice libplist

LOCAL_MODULE := ideviceinfo

include $(BUILD_EXECUTABLE)


#ideviceid

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
 $(LIB_ROOT_REL)/tools/idevice_id.c

LOCAL_C_INCLUDES += \
 $(LIB_ROOT_ABS) \
 $(LIB_ROOT_ABS)/../../usbmuxd/out/toolchain/sysroot/usr/include \
 $(LIB_ROOT_ABS)/../../usbmuxd/out/toolchain/include/c++/4.8 \
 $(LIB_ROOT_ABS)/src \
 $(LIB_ROOT_ABS)/include \
 $(LIB_ROOT_ABS)/../$(LIB_PLIST_VERSION)/include \

LOCAL_SHARED_LIBRARIES := libimobiledevice libplist

LOCAL_MODULE := ideviceid

include $(BUILD_EXECUTABLE)


#idevicename

include $(CLEAR_VARS)

LOCAL_CFLAGS := -D__ANDROID__

LOCAL_SRC_FILES := \
 $(LIB_ROOT_REL)/tools/idevicename.c

LOCAL_C_INCLUDES += \
 $(LIB_ROOT_ABS) \
 $(LIB_ROOT_ABS)/../../usbmuxd/out/toolchain/sysroot/usr/include \
 $(LIB_ROOT_ABS)/../../usbmuxd/out/toolchain/include/c++/4.8 \
 $(LIB_ROOT_ABS)/src \
 $(LIB_ROOT_ABS)/include \
 $(LIB_ROOT_ABS)/../$(LIB_PLIST_VERSION)/include \

LOCAL_SHARED_LIBRARIES := libimobiledevice libplist

LOCAL_MODULE := idevicename

include $(BUILD_EXECUTABLE)


#idevicepair

include $(CLEAR_VARS)

LOCAL_CFLAGS := -DHAVE_OPENSSL -D__ANDROID__
 
LOCAL_SRC_FILES := \
 $(LIB_ROOT_REL)/tools/idevicepair.c

LOCAL_C_INCLUDES += \
 $(LIB_ROOT_ABS) \
 $(LIB_ROOT_ABS)/../../usbmuxd/out/toolchain/sysroot/usr/include \
 $(LIB_ROOT_ABS)/../../usbmuxd/out/toolchain/include/c++/4.8 \
 $(LIB_ROOT_ABS)/src \
 $(LIB_ROOT_ABS)/include \
 $(LIB_ROOT_ABS)/../$(LIB_PLIST_VERSION)/include \

LOCAL_SHARED_LIBRARIES := libimobiledevice libplist

LOCAL_MODULE := idevicepair

include $(BUILD_EXECUTABLE)


#idevicedate

include $(CLEAR_VARS)

LOCAL_CFLAGS := \
 -DHAVE_OPENSSL -D__ANDROID__
 
LOCAL_SRC_FILES := \
 $(LIB_ROOT_REL)/tools/idevicedate.c

LOCAL_C_INCLUDES += \
 $(LIB_ROOT_ABS) \
 $(LIB_ROOT_ABS)/../../usbmuxd/out/toolchain/sysroot/usr/include \
 $(LIB_ROOT_ABS)/../../usbmuxd/out/toolchain/include/c++/4.8 \
 $(LIB_ROOT_ABS)/src \
 $(LIB_ROOT_ABS)/include \
 $(LIB_ROOT_ABS)/../$(LIB_PLIST_VERSION)/include \

LOCAL_SHARED_LIBRARIES := libimobiledevice libplist

LOCAL_MODULE := idevicedate

include $(BUILD_EXECUTABLE)


#idevicediagnostics

include $(CLEAR_VARS)

LOCAL_CFLAGS := \
 -DHAVE_OPENSSL -D__ANDROID__
 
LOCAL_SRC_FILES := \
 $(LIB_ROOT_REL)/tools/idevicediagnostics.c

LOCAL_C_INCLUDES += \
 $(LIB_ROOT_ABS) \
 $(LIB_ROOT_ABS)/../../usbmuxd/out/toolchain/sysroot/usr/include \
 $(LIB_ROOT_ABS)/../../usbmuxd/out/toolchain/include/c++/4.8 \
 $(LIB_ROOT_ABS)/src \
 $(LIB_ROOT_ABS)/include \
 $(LIB_ROOT_ABS)/../$(LIB_PLIST_VERSION)/include \

LOCAL_SHARED_LIBRARIES := libimobiledevice libplist

LOCAL_MODULE := idevicediagnostics

include $(BUILD_EXECUTABLE)


#idevicescreenshot

include $(CLEAR_VARS)

LOCAL_CFLAGS := \
 -DHAVE_OPENSSL -D__ANDROID__
 
LOCAL_SRC_FILES := \
 $(LIB_ROOT_REL)/tools/idevicescreenshot.c

LOCAL_C_INCLUDES += \
 $(LIB_ROOT_ABS) \
 $(LIB_ROOT_ABS)/../../usbmuxd/out/toolchain/sysroot/usr/include \
 $(LIB_ROOT_ABS)/../../usbmuxd/out/toolchain/include/c++/4.8 \
 $(LIB_ROOT_ABS)/src \
 $(LIB_ROOT_ABS)/include \
 $(LIB_ROOT_ABS)/../$(LIB_PLIST_VERSION)/include \

LOCAL_SHARED_LIBRARIES := libimobiledevice libplist

LOCAL_MODULE := idevicescreenshot

include $(BUILD_EXECUTABLE)


#idevicesyslog

include $(CLEAR_VARS)

LOCAL_CFLAGS := \
 -DHAVE_OPENSSL -D__ANDROID__
 
LOCAL_SRC_FILES := \
 $(LIB_ROOT_REL)/tools/idevicesyslog.c

LOCAL_C_INCLUDES += \
 $(LIB_ROOT_ABS) \
 $(LIB_ROOT_ABS)/../../usbmuxd/out/toolchain/sysroot/usr/include \
 $(LIB_ROOT_ABS)/../../usbmuxd/out/toolchain/include/c++/4.8 \
 $(LIB_ROOT_ABS)/src \
 $(LIB_ROOT_ABS)/include \
 $(LIB_ROOT_ABS)/../$(LIB_PLIST_VERSION)/include \

LOCAL_SHARED_LIBRARIES := libimobiledevice libplist

LOCAL_MODULE := idevicesyslog

include $(BUILD_EXECUTABLE)


#idevicecrashreport

include $(CLEAR_VARS)

LOCAL_CFLAGS := -D__ANDROID__

LOCAL_SRC_FILES := \
 $(LIB_ROOT_REL)/tools/idevicecrashreport.c

LOCAL_C_INCLUDES += \
 $(LIB_ROOT_ABS) \
 $(LIB_ROOT_ABS)/../../usbmuxd/out/toolchain/sysroot/usr/include \
 $(LIB_ROOT_ABS)/../../usbmuxd/out/toolchain/include/c++/4.8 \
 $(LIB_ROOT_ABS)/src \
 $(LIB_ROOT_ABS)/include \
 $(LIB_ROOT_ABS)/../$(LIB_PLIST_VERSION)/include \

LOCAL_SHARED_LIBRARIES := libimobiledevice libplist

LOCAL_MODULE := idevicecrashreport

include $(BUILD_EXECUTABLE)
endif
