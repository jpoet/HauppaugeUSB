#ifndef StreamBuffer_h_
#define StreamBuffer_h_

#include <stdint.h>

extern "C" {
#include <libavformat/avformat.h>
}

class Transcoder;
class StreamWriter;

class StreamBuffer 
{
    friend class Transcoder;
    friend class StreamWriter;

private:
    typedef struct _block {
        struct _block *next;
        unsigned int length;
        unsigned int allocated;
        unsigned int offset;
#pragma pack(push, 16)
        int8_t data[0];
#pragma pack(pop)
    } block_t;

    Transcoder * m_transcoder;

    unsigned int maxAllocated;
    int volatile bytesBuffered;

    volatile block_t * headFree;   // The first free cached block
    volatile block_t * volatile * tailFree; // The last freed cached block

    volatile block_t * head;   // The oldest buffered block. We read from this position
    volatile block_t * volatile * tail; // The newest buffered block, We write to this position

    AVInputFormat * m_iFormat;
    AVIOContext * m_avioContext;
    AVFormatContext * m_iAVFContext;

    bool m_initialized;
    int m_pmtVersion;

    AVPacket m_pkt;

    block_t * getBlock(unsigned int blocksize);

    void freeBlock(block_t * block);

    void putData(void * ptr, size_t length);

    AVPacket * flush();
    AVPacket * getNextPacket(bool flush=false);
    void releasePacket(AVPacket * packet);

    StreamBuffer(Transcoder * transcoder);
    ~StreamBuffer();

public:
    int readPacket(uint8_t * buf, int buf_size);
};

#endif
