#ifndef AudioBuffer_h_
#define AudioBuffer_h_

#include <stdint.h>

extern "C" 
{
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
        struct _block *next;
        unsigned int length;
        unsigned int allocated;
        unsigned int offset;
        int64_t pts;
        uint8_t * bufs[8];
#pragma pack(push, 16)
        uint8_t data[0];
#pragma pack(pop)
    } block_t;

    unsigned int maxAllocated;
    int volatile bytesBuffered;

    volatile block_t * headFree;   // The first free cached block
    volatile block_t * volatile * tailFree; // The last freed cached block

    volatile block_t * head;   // The oldest buffered block. We read from this position
    volatile block_t * volatile * tail; // The newest buffered block, We write to this position

    void reset();

    block_t * getBlock(unsigned int blocksize);

    void freeBlock(block_t * block);

    void putFrame(AVFrame * frame);

    AVFrame * getFrame(bool flush=false);

    void releaseFrame(AVFrame * frame);

    AudioBuffer();
    ~AudioBuffer();
};

#endif
