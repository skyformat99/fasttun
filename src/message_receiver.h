#ifndef __MESSAGERECEIVER_H__
#define __MESSAGERECEIVER_H__

#include "fasttun_base.h"

NAMESPACE_BEG(msg)

template <class T, size_t MaxLen, class SizeType = size_t>
class MessageReceiver
{
    typedef void (T::*RecvFunc)(const void *, SizeType, void *);
    typedef void (T::*RecvErrFunc)(void *);
  public:
    MessageReceiver(T *host, RecvFunc recvFunc, RecvErrFunc recvErrFunc)
            :mRcvdState(MsgRcvState_NoData)
            ,mRcvdMsgLen(0)
            ,mHost(host)
            ,mRecvFunc(recvFunc)
            ,mRecvErrFunc(recvErrFunc)
    {
        assert(mHost && mRecvFunc && mRecvErrFunc);
        memset(&mRcvdLenBuf, 0, sizeof(mRcvdLenBuf));
        memset(&mCurMsg, 0, sizeof(mCurMsg));
    }
    
    virtual ~MessageReceiver()
    {
        if (mCurMsg.ptr)
            free(mCurMsg.ptr);  
    }

    void input(const void *data, SizeType datalen, void *user)
    {
        const char *ptr = (const char *)data;
        for (;;)        
        {
            SizeType leftlen = parseMessage(ptr, datalen);
            if (MsgRcvState_Error == mRcvdState)
            {
                (mHost->*mRecvErrFunc)(user);
                break;
            }
            else if (MsgRcvState_RcvComplete == mRcvdState)
            {
                (mHost->*mRecvFunc)(mCurMsg.ptr, mCurMsg.len, user);
                clearCurMsg();
            }

            if (leftlen > 0)
            {
                ptr += (datalen-leftlen);
                datalen = leftlen;
            }
            else
            {
                break;
            }
        }
    }

    void clear()
    {
        clearCurMsg();
    }

  private:
    SizeType parseMessage(const void *data, SizeType datalen)
    {
        const char *ptr = (const char *)data;
    
        // 先收取消息长度
        if (MsgRcvState_NoData == mRcvdState)
        {
            assert(sizeof(SizeType) >= mRcvdLenBuf.curlen);
            SizeType copylen = sizeof(SizeType)-mRcvdLenBuf.curlen;
            copylen = min(copylen, datalen);

            if (copylen > 0)
            {
                memcpy(mRcvdLenBuf.buf+mRcvdLenBuf.curlen, ptr, copylen);
                mRcvdLenBuf.curlen += copylen;
                ptr += copylen;
                datalen -= copylen;
            }
            if (mRcvdLenBuf.curlen == sizeof(SizeType)) // 消息长度已获取
            {
                MemoryStream stream;
                stream.append(mRcvdLenBuf.buf, sizeof(SizeType));
                stream>>mCurMsg.len;
                if (mCurMsg.len > MaxLen)
                {
                    mRcvdState = MsgRcvState_Error;
                    mCurMsg.len = 0;
                    return datalen;
                }
            
                mRcvdState = MsgRcvState_RcvdHead;
                mRcvdMsgLen = 0;
                mCurMsg.ptr = (char *)malloc(mCurMsg.len);
                assert(mCurMsg.ptr != NULL && "mCurMsg.ptr != NULL");
            }
        }

        // 再收取消息内容
        if (MsgRcvState_RcvdHead == mRcvdState)
        {
            assert(mCurMsg.len >= mRcvdMsgLen);
            SizeType copylen = mCurMsg.len-mRcvdMsgLen;     
            copylen = min(copylen, datalen);

            if (copylen > 0)
            {
                memcpy(mCurMsg.ptr+mRcvdMsgLen, ptr, copylen);
                mRcvdMsgLen += copylen;
                ptr += copylen;
                datalen -= copylen;
            }
            if (mRcvdMsgLen == mCurMsg.len) // 消息体已获取
            {
                mRcvdState = MsgRcvState_RcvComplete;
            }
        }

        return datalen;
    }

    void clearCurMsg()
    {
        mRcvdState = MsgRcvState_NoData;
        
        memset(&mRcvdLenBuf, 0, sizeof(mRcvdLenBuf));
        
        if (mCurMsg.ptr)
            free(mCurMsg.ptr);
        mRcvdMsgLen = 0;
        memset(&mCurMsg, 0, sizeof(mCurMsg));
    }

  private:
    enum EMsgRcvState
    {
        MsgRcvState_NoData,
        MsgRcvState_Error,
        MsgRcvState_RcvdHead,
        MsgRcvState_RcvComplete,
    };

    struct MessageLenBuf
    {
        SizeType curlen;
        char buf[sizeof(SizeType)];
    };
    
    struct Message
    {
        SizeType len;
        char *ptr;
    };

    // message parse state
    EMsgRcvState mRcvdState;

    // message header
    MessageLenBuf mRcvdLenBuf;

    // message body
    SizeType mRcvdMsgLen;
    Message mCurMsg;

    // message dealer
    T *mHost;
    RecvFunc mRecvFunc;
    RecvErrFunc mRecvErrFunc;
};

NAMESPACE_END // namespace msg

#endif // __MESSAGERECEIVER_H__
