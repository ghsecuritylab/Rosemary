#define LOG_TAG "NXAllocator"

#include <sys/mman.h>
#include <linux/videodev2.h>
#include <utils/Log.h>

#include <nexell_format.h>
#include "gralloc_priv.h"
#include "NXAllocator.h"


static bool allocPlane(int ionFD, size_t size, int &outFD, unsigned long &outVirt, unsigned long &outPhys, bool isSystem = false)
{
    int ret;
    int fd;
    char *virt;
    unsigned long phys;

    int heapMask;
    if (isSystem)
        heapMask = ION_HEAP_SYSTEM_MASK;
    else
        heapMask = ION_HEAP_NXP_CONTIG_MASK;

    ret = ion_alloc_fd(ionFD, size, 0, heapMask, 0, &fd);
    if (ret < 0) {
        ALOGE("failed to ion_alloc_fd()");
        return false;
    }
    virt = (char *)mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (MAP_FAILED == virt) {
        ALOGE("failed to mmap()");
        close(fd);
        return false;
    }
    ret = ion_get_phys(ionFD, fd, &phys);
    if (ret < 0) {
        ALOGE("failed to ion_get_phys()");
        munmap(virt, size);
        close(fd);
        return false;
    }

    outFD = fd;
    outVirt = (unsigned long)virt;
    outPhys = phys;
    return true;
}

bool allocateBuffer(struct nxp_vid_buffer *buf, int bufSize, int width, int height, uint32_t format)
{
    int planeNum;
    size_t ySize, cSize;

    switch (format) {
    case PIXFORMAT_YUV422_PACKED:
        planeNum = 1;
        ySize = YUV_STRIDE(width * 2) * YUV_VSTRIDE(height);
        cSize = 0;
        break;
    case PIXFORMAT_YUV420_PLANAR:
        planeNum = 3;
        ySize = YUV_YSTRIDE(width) * YUV_VSTRIDE(height);
        cSize = YUV_STRIDE(width/2) * YUV_VSTRIDE(height/2);
        break;
    case PIXFORMAT_YUV422_PLANAR:
        planeNum = 3;
        ySize = YUV_STRIDE(width) * YUV_VSTRIDE(height);
        cSize = YUV_STRIDE(width) * YUV_VSTRIDE(height/2);
        break;
    case PIXFORMAT_YUV444_PLANAR:
        planeNum = 3;
        ySize = cSize = YUV_STRIDE(width) * YUV_VSTRIDE(height);
        break;
    case PIXFORMAT_RGB32:
        planeNum = 1;
        ySize = width * height * 4;
        cSize = 0;
        break;
    default:
        ALOGE("%s: invalid format 0x%x", __func__, format);
        return false;
    }

    int ionFD = ion_open();
    if (ionFD < 0) {
        ALOGE("failed to ion_open()");
        return false;
    }

    int i;
    int fd;
    unsigned long phys, virt;
    bool ret;

    for (i = 0; i < bufSize; i++) {
        /* alloc Y */
        ret = allocPlane(ionFD, ySize, fd, virt, phys);
        if (!ret)
            goto out;
        buf[i].plane_num    = planeNum;
        buf[i].sizes[0]     = ySize;
        buf[i].fds[0]       = fd;
        buf[i].virt[0]      = reinterpret_cast<char *>(virt);
        buf[i].phys[0]      = phys;
        buf[i].stride[0]    = YUV_YSTRIDE(width);

        if (cSize > 0) {
            /* alloc CB */
            ret = allocPlane(ionFD, cSize, fd, virt, phys);
            if (!ret)
                goto out;
            buf[i].sizes[1]     = cSize;
            buf[i].fds[1]       = fd;
            buf[i].virt[1]      = reinterpret_cast<char *>(virt);
            buf[i].phys[1]      = phys;
            buf[i].stride[1]    = YUV_STRIDE(width/2);

            /* alloc CR */
            ret = allocPlane(ionFD, cSize, fd, virt, phys);
            if (!ret)
                goto out;
            buf[i].sizes[2]     = cSize;
            buf[i].fds[2]       = fd;
            buf[i].virt[2]      = reinterpret_cast<char *>(virt);
            buf[i].phys[2]      = phys;
            buf[i].stride[2]    = YUV_STRIDE(width/2);
        }
    }

    //dumpBuffer(buf, bufSize);
out:
    close(ionFD);
    return ret;
}

void freeBuffer(struct nxp_vid_buffer *buf, int bufSize)
{
    for (int i = 0; i < bufSize; i++) {
        for (int j = 0; j < buf[i].plane_num; j++) {
            if (buf[i].sizes[j] > 0) {
                munmap(buf[i].virt[j], buf[i].sizes[j]);
                close(buf[i].fds[j]);
            }
        }
    }
}

void dumpBuffer(struct nxp_vid_buffer *buf, int bufSize)
{
    for (int i = 0; i < bufSize; i++) {
        ALOGD("====> buffer %d", i);
        for (int j = 0; j < buf[i].plane_num; j++) {
            ALOGD("\tfd: %d,\tsize %d,\tphys 0x%lx,\tvirt %p,\tstride %lu",
                    buf[i].fds[j],
                    buf[i].sizes[j],
                    buf[i].phys[j],
                    buf[i].virt[j],
                    buf[i].stride[j]);
        }
    }
}
