#ifndef _NX_DEINTERLACER_MANAGER_H
#define _NX_DEINTERLACER_MANAGER_H

#include <system/window.h>
#include <NXAllocator.h>
#include <NXQueue.h>

#include <nxp-deinterlacer.h>

namespace android {

class NXDeinterlacerManager
{
public:
    NXDeinterlacerManager(int srcWidth, int srcHeight);
    virtual ~NXDeinterlacerManager();

    virtual bool qSrcBuf(int index, struct nxp_vid_buffer *buf);
    virtual bool dqSrcBuf(int *pIndex, struct nxp_vid_buffer **pBuf);
    virtual bool qDstBuf(struct nxp_vid_buffer *buf);
    virtual bool dqDstBuf(struct nxp_vid_buffer **pBuf);
    virtual bool run(void);
    virtual int  getRunCount(void) {
        return mRunCount;
    }
    virtual void setSrcType(int type) {
        mSrcType = type;
    }
    virtual void setStartField(int field) {
        mCurrentField = field;
    }

    enum {
         SRC_TYPE_FRAME = 0,
         SRC_TYPE_FIELD
    };

    enum {
        FIELD_EVEN = 0,
        FIELD_ODD
    };

private:
    struct SrcBufferType {
        int index;
        struct nxp_vid_buffer *buf;
    };

    int mSrcWidth;
    int mSrcHeight;

    class NXQueue<SrcBufferType> mSrcBufferQueue;
    class NXQueue<struct nxp_vid_buffer *> mDstBufferQueue;

    int mHandle;
    int mRunCount;
    int mIonHandle;

    frame_data_info mFrameInfo;
    int mCurrentField;

    int mSrcType;

private:
    void makeFrameInfo(int index);
    // psw0523 add for sw scaling
    void makeFrameBySW(int index);
    void makeFrameByScaler(int index);
};

};
#endif
