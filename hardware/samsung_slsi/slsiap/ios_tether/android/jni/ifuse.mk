#ifuse

LIB_VERSION:=ifuse
LIB_FUSE_VERSION:=libfuse-android/libfuse
LIB_PLIST_VERSION:=libplist
LIB_IMOBILEDEVICE_VERSION:=libimobiledevice

LOCAL_PATH:= $(call my-dir)
LIB_ROOT_REL:= ../../$(LIB_VERSION)
LIB_ROOT_ABS:= $(LOCAL_PATH)/../../$(LIB_VERSION)

include $(CLEAR_VARS)

LOCAL_CFLAGS := \
 -DHAVE_LIBIMOBILEDEVICE_1_1_5 \
 -DHAVE_LIBIMOBILEDEVICE_1_1 \
 -DPACKAGE_NAME=\"ifuse\" \
 -DPACKAGE_VERSION=\"1.1.3\" \
 -DANDROID \
 -D_FILE_OFFSET_BITS=64 \
 -DFUSE_USE_VERSION=26 \
 -fno-strict-aliasing -D__ANDROID__

LOCAL_SRC_FILES := \
 $(LIB_ROOT_REL)/src/ifuse.c

LOCAL_C_INCLUDES += \
 $(LIB_ROOT_ABS) \
 $(LIB_ROOT_ABS)/../../usbmuxd/out/toolchain/sysroot/usr/include \
 $(LIB_ROOT_ABS)/../../usbmuxd/out/toolchain/include/c++/4.8 \
 $(LIB_ROOT_ABS)/src \
 $(LIB_ROOT_ABS)/../$(LIB_FUSE_VERSION)/include \
 $(LIB_ROOT_ABS)/../$(LIB_PLIST_VERSION)/include \
 $(LIB_ROOT_ABS)/../$(LIB_IMOBILEDEVICE_VERSION)/common \
 $(LIB_ROOT_ABS)/../$(LIB_IMOBILEDEVICE_VERSION)/src \
 $(LIB_ROOT_ABS)/../$(LIB_IMOBILEDEVICE_VERSION)/include 

LOCAL_SHARED_LIBRARIES := libfuse libimobiledevice libplist

LOCAL_MODULE := ifuse

include $(BUILD_EXECUTABLE)

