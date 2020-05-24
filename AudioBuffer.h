#ifndef AudioBuffer_h_
#define AudioBuffer_h_

#include <stdint.h>

extern "C" {
#include <libavformat/avformat.h>
}

class Transcoder;
class StreamWriter;

class AudioBuffer
{
    friend class Transcoder;
    friend class StreamWriter;

  private:
    typedef struct _block {
        struct _block * next;
        unsigned int length;
        unsigned int allocated;
        unsigned int offset;
        int64_t pts;
        uint8_t * bufs[8];
#pragma pack(push, 16)
        uint8_t data[0];
#pragma pack(pop)
    } block_t;

    unsigned int m_maxAllocated;
    int volatile m_bytesBuffered;

    volatile block_t * m_headFree;            // The first free cached block
    volatile block_t * volatile * m_tailFree; // The last freed cached block

    volatile block_t * m_head;            // The oldest buffered block.
    volatile block_t * volatile * m_tail; // The newest buffered block.

    void Reset();

    block_t * GetBlock(unsigned int blocksize);

    void FreeBlock(block_t * block);

    void PutFrame(AVFrame * frame);

    AVFrame * GetFrame(bool flush = false);

    void ReleaseFrame(AVFrame * frame);

    AudioBuffer();
    ~AudioBuffer();
};

#endif
