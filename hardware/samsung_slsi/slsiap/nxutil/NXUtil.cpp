#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/mman.h>

#include <system/graphics.h>

#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <cutils/log.h>

#include <nexell_format.h>

#include <NXUtil.h>

int getScreenAttribute(const char *fbname, int32_t &xres, int32_t &yres, int32_t &refreshRate)
{
    int ret = 0;
    char *path;
    char buf[128] = {0, };
    int fd;
    unsigned int x, y, r;

    asprintf(&path, "/sys/class/graphics/%s/modes", fbname);
    if (!path) {
        ALOGE("%s: failed to asprintf()", __func__);
        return -EINVAL;
    }

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        ALOGE("%s: failed to open %s", __func__, path);
        ret = -EINVAL;
        goto err_open;
    }

    ret = read(fd, buf, sizeof(buf));
    if (ret <= 0) {
         ALOGE("%s: failed to read %s", __func__, path);
         ret = -EINVAL;
         goto err_read;
    }

    ret = sscanf(buf, "U:%ux%up-%u", &x, &y, &r);
    if (ret != 3) {
         ALOGE("%s: failed to sscanf()", __func__);
         ret = -EINVAL;
         goto err_sscanf;
    }

    ALOGI("Using %ux%u %uHz resolution for %s from modes list\n", x, y, r, fbname);

    xres = (int32_t)x;
    yres = (int32_t)y;
    refreshRate = (int32_t)r;

    close(fd);
    free(path);
    return 0;

err_sscanf:
err_read:
    close(fd);
err_open:
    free(path);

    return ret;
}

int copydata(char *srcY, char *srcCb, char *srcCr,
             char *dstY, char *dstCb, char *dstCr,
             uint32_t srcStride, uint32_t dstStride,
             uint32_t width, uint32_t height)
{
    uint32_t i, j;
    char *psrc = srcY;
    char *psrcCb = srcCb;
    char *psrcCr = srcCr;

    char *pdst = dstY;
    char *pdstCb = dstCb;
    char *pdstCr = dstCr;

    ALOGV("srcY %p, srcCb %p, srcCr %p, dstY %p, dstCb %p, dstCr %p, srcStride %d, dstStride %d, width %d, height %d",
            srcY, srcCb, srcCr, dstY, dstCb, dstCr, srcStride, dstStride, width, height);
    // y
    for( i=0; i<height; i++) {
        memcpy(pdst, psrc, width);
        psrc += srcStride;
        pdst += dstStride;
    }

    // Cb
    for( i=0; i<(height>>1); i++) {
        memcpy(pdstCb, psrcCb, (width >> 1));
        psrcCb += (srcStride >> 1);
        pdstCb += (dstStride >> 1);
    }

    // Cr
    for( i=0; i<(height>>1); i++) {
        memcpy(pdstCr, psrcCr, (width >> 1));
        psrcCr += (srcStride >> 1);
        pdstCr += (dstStride >> 1);
    }

    return 0;
}

bool nxMemcpyHandle(private_handle_t const *dstHandle, private_handle_t const *srcHandle)
{
	int width = srcHandle->width;
	int height = srcHandle->height;

    switch (srcHandle->format) {
    case HAL_PIXEL_FORMAT_YV12:
		switch (dstHandle->format) {
		case HAL_PIXEL_FORMAT_YV12:
        	{
#if 1
				char *src = (char *)mmap(NULL, srcHandle->size, PROT_READ | PROT_WRITE, MAP_SHARED, srcHandle->share_fd, 0);
                if (MAP_FAILED == src) {
                    ALOGE("mmap failed for SRC");
                    return false;
                }

                char *dst = (char *)mmap(NULL, dstHandle->size, PROT_READ | PROT_WRITE, MAP_SHARED, dstHandle->share_fd, 0);
                if (MAP_FAILED == dst) {
                    ALOGE("mmap failed for DST");
                    return false;
                 }

                memcpy(dst, src, srcHandle->size);

                munmap(dst, dstHandle->size);
                munmap(src, srcHandle->size);
#else
                char *srcY = (char *)mmap(NULL, srcHandle->size, PROT_READ | PROT_WRITE, MAP_SHARED, srcHandle->share_fd, 0);
                if (MAP_FAILED == srcY) {
                    ALOGE("mmap failed for srcY");
                    return false;
                }
                char *srcCb = srcY + (srcHandle->stride * ALIGN(srcHandle->height, 16));
                char *srcCr = srcCb + (ALIGN(srcHandle->stride >> 1, 16) * ALIGN(srcHandle->height >> 1, 16));
                char *dstY  = (char *)mmap(NULL, dstHandle->size, PROT_READ | PROT_WRITE, MAP_SHARED, dstHandle->share_fd, 0);
                if (MAP_FAILED == dstY) {
                    ALOGE("mmap failed for dstY");
                    munmap(srcY, srcHandle->size);
                    return false;
                }
                char *dstCb = dstY + (dstHandle->stride * ALIGN(dstHandle->height, 16));
                char *dstCr = dstCb + (ALIGN(dstHandle->stride >> 1, 16) * ALIGN(dstHandle->height >> 1, 16));

                copydata(srcY, srcCb, srcCr, 
                         dstY, dstCb, dstCr,
                              width, width,
                              width, height);

                munmap(dstY, dstHandle->size);
                munmap(srcY, srcHandle->size);
#endif
			}
       		return true;
		}
		break;
	}
	return false;
}
