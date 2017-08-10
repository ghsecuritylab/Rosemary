#ifndef GRALLOC_PRIV_H
#define GRALLOC_PRIV_H

#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#include <linux/fb.h>

#include <hardware/gralloc.h>
#include <cutils/native_handle.h>

#define NUM_FB_BUFFERS   3

//#define HAL_PIXEL_FORMAT_YV12_444 0x32315660 // HAL_PIXEL_FORMAT_YV12 + 1

#define GRALLOC_ARM_DMA_BUF_MODULE  1

#ifndef ALIGN
#define ALIGN(x, a) (((x) + (a) - 1) & ~((a) - 1))
#endif

#define YUV_STRIDE_ALIGN_FACTOR     64
#define YUV_VSTRIDE_ALIGN_FACTOR    16

#define YUV_STRIDE(w)    ALIGN(w, YUV_STRIDE_ALIGN_FACTOR)
#define YUV_YSTRIDE(w)   (ALIGN(w/2, YUV_STRIDE_ALIGN_FACTOR) * 2)
#define YUV_VSTRIDE(h)   ALIGN(h, YUV_VSTRIDE_ALIGN_FACTOR)

typedef enum
{
	MALI_YUV_NO_INFO,
	MALI_YUV_BT601_NARROW,
	MALI_YUV_BT601_WIDE,
	MALI_YUV_BT709_NARROW,
	MALI_YUV_BT709_WIDE,
} mali_gralloc_yuv_info;

struct private_handle_t;

struct private_module_t {
    gralloc_module_t base;

    int fb_fd;
    struct private_handle_t *framebuffer[NUM_FB_BUFFERS];
    pthread_mutex_t lock;
    int ion_client; // /dev/ion fd

    int xres;
    int yres;
    float xdpi;
    float ydpi;
    float fps;
};

#ifdef __cplusplus
struct private_handle_t : public native_handle {
#else
struct private_handle_t {
    struct native_handle nativeHandle;
#endif

#ifdef __cplusplus
    enum {
         PRIV_FLAGS_FRAMEBUFFER = 0x00000001,
         PRIV_FLAGS_USES_UMP    = 0x00000002,
         PRIV_FLAGS_USES_ION    = 0x00000004,
    };
#endif

    // file descriptors
    int share_fd;
    int share_fd1;
    int share_fd2;

    // ints
    int magic;
    int flags;
    int usage;
    int size;
    int format;
    int width;
    int height;
    int stride;
    void* base;
    mali_gralloc_yuv_info yuv_info;

#ifdef __cplusplus
    static const int sNumFds  = 3;
    static const int sNumInts = 10;
    static const int sMagic   = 0x3141592;

    private_handle_t(
            int flags,
            int usage,
            int size,
            int format,
            int width,
            int height,
            int stride,
            void* base,
            mali_gralloc_yuv_info yuv_info,
            int fd,
            int fd1 = -1,
            int fd2 = -1) :
        share_fd(fd),
        share_fd1(fd1),
        share_fd2(fd2),
        magic(sMagic),
        flags(flags),
        usage(usage),
        size(size),
        format(format),
        width(width),
        height(height),
        stride(stride),
        base(base),
        yuv_info(yuv_info)
    {
        version = sizeof(native_handle);
        numInts = sNumInts;
        numFds  = sNumFds;
        if (fd1 < 0) {
            numInts++;
            numFds--;
        }
        if (fd2 < 0) {
            numInts++;
            numFds--;
        }
    }

    ~private_handle_t() {
         magic = 0;
    }

    static int validate(const native_handle *h) {
        const private_handle_t *hnd = (const private_handle_t *)h;
        if (!h ||
             h->version != sizeof(native_handle) ||
             hnd->numInts + hnd->numFds != sNumInts + sNumFds ||
             hnd->magic != sMagic) {
             //ALOGE("invalid gralloc handle (at %p)", reinterpret_cast<void *>(const_cast<native_handle *>(h)));
             return -EINVAL;
        }
        return 0;
    }

    static private_handle_t *dynamicCast(const native_handle *in) {
         if (0 == validate(in))
             return (private_handle_t *)in;
         return NULL;
    }
#endif
};

#endif
