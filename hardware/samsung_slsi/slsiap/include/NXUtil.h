#ifndef _NX_UTIL_H
#define _NX_UTIL_H

#include <gralloc_priv.h>

int getScreenAttribute(const char *fbname, int32_t &xres, int32_t &yres, int32_t &refreshRate);

bool nxMemcpyHandle(private_handle_t const *srcHandle, private_handle_t const *dstHandle);

int copydata(char *srcY, char *srcCb, char *srcCr,
			 char *dstY, char *dstCb, char *dstCr,
             uint32_t srcStride, uint32_t dstStride,
             uint32_t width, uint32_t height);

#endif
