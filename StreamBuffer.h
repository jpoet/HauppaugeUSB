#ifndef STREAMBUFFER_H
#define STREAMBUFFER_H

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
        struct _block * next;
        unsigned int length;
        unsigned int allocated;
        unsigned int offset;
#pragma pack(push, 16)
        int8_t data[0];
#pragma pack(pop)
    } block_t;

    Transcoder * m_transcoder;

    unsigned int m_maxAllocated;
    int volatile m_bytesBuffered;

    volatile block_t * m_headFree;            // The first free cached block
    volatile block_t * volatile * m_tailFree; // The last freed cached block

    volatile block_t * m_head;            // The oldest buffered block.
    volatile block_t * volatile * m_tail; // The newest buffered block.

    const AVInputFormat * m_iFormat;
    AVIOContext * m_avioContext;
    AVFormatContext * m_iAVFContext;

    bool m_initialized;
    int m_pmtVersion;

    AVPacket *m_pkt;

    bool m_isErrored;

    block_t * GetBlock(unsigned int blocksize);

    void FreeBlock(block_t * block);

    void Reset();
    bool isErrored();

    void PutData(void * ptr, size_t length);

    AVPacket * Flush();
    AVPacket * GetNextPacket(bool flush = false);
    void ReleasePacket(AVPacket * packet);

    StreamBuffer(Transcoder * transcoder);
    ~StreamBuffer();

  public:
    int ReadPacket(uint8_t * buf, int buf_size);
};

#endif
