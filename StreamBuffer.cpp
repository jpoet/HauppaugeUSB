#include "StreamBuffer.h"
#include "Transcoder.h"
#include "Logger.h"

extern "C" {
#include <libavutil/timestamp.h>
}

extern "C" int staticReadPacket(void * ctx, uint8_t * buf, int buf_size) {
    return ((StreamBuffer *)ctx)->readPacket(buf, buf_size);
}

#define AVBUFSIZE 128 * 1024

StreamBuffer::StreamBuffer(Transcoder * transcoder)
: m_transcoder(transcoder)
, maxAllocated(0)
, bytesBuffered(0)
, headFree(nullptr)
, tailFree(&headFree)
, head(nullptr)
, tail(&head)
, m_iFormat(nullptr)
, m_avioContext(nullptr)
, m_iAVFContext(nullptr)
, m_initialized(false)
{
    unsigned char * buffer = (unsigned char *)av_malloc(AVBUFSIZE);
    m_iAVFContext = avformat_alloc_context();

    m_avioContext = avio_alloc_context(buffer, AVBUFSIZE, 0, (void *)this, &staticReadPacket, nullptr, nullptr);

    m_iFormat = av_find_input_format("mpegts");

    if (buffer && m_iAVFContext && m_avioContext && m_iFormat)
    {
        m_iAVFContext->pb = m_avioContext;
        m_iAVFContext->probesize = 2147483647;
        m_iAVFContext->max_analyze_duration = 2147483647;
    }
}

StreamBuffer::~StreamBuffer()
{
    if (m_avioContext)
    {
        av_free(m_avioContext->buffer);
        avio_context_free(&m_avioContext);
    }

    if (m_iAVFContext) 
    {
        avformat_free_context(m_iAVFContext);
    }

    while (headFree) 
    {
        block_t * b =  headFree->next;
        delete headFree;
        headFree = b;
    }

    while (head) 
    {
        block_t * b =  head->next;
        delete head;
        head = b;
    }
}

StreamBuffer::block_t * StreamBuffer::getBlock(unsigned int blocksize) 
{
    block_t * block;

    // We can't remove the last one, it has to stay so tailFree stays valid
    while ((block = const_cast<block_t *>(headFree)) != nullptr && headFree->next != nullptr && headFree->allocated < blocksize) 
    {
        headFree = block->next;
        delete block;
    }

    // We can't remove the last one, it has to stay so tailFree stays valid
    if ((block = const_cast<block_t *>(headFree)) != nullptr && headFree->next != nullptr) 
    {
        headFree = block->next;
    }
    else 
    {
        if (blocksize > maxAllocated)
            maxAllocated = blocksize;
        blocksize = maxAllocated;
        block = (block_t *)new char[sizeof(block_t) + blocksize];
        block->allocated = blocksize;
    }
    return block;
}

void StreamBuffer::freeBlock(block_t * block) 
{
    if (block->allocated < maxAllocated) 
    {
        delete block; // Free this one because it's too small
    }
    else 
    {
        block->next = nullptr;
        *tailFree = block;
        tailFree = const_cast<volatile block_t * volatile *>(&block->next);
    }
}

void StreamBuffer::putData(void * ptr, size_t length) 
{
    if (!ptr && !head)
        return; // Skip the very first marker

    // write the data into a buffer
    block_t * b = getBlock(length);
    if (ptr == nullptr) 
    {
        // This is a marker that the stream has stopped and started again
        // For now, we let this go through as a zero-length block
    }
    memcpy(b->data, ptr, length);
    b->length = length;
    b->offset = 0;
    b->next = nullptr;
    *tail = b;
    tail = const_cast<volatile block_t * volatile *>(&b->next);
    __sync_fetch_and_add(&bytesBuffered, length);
}

int StreamBuffer::readPacket(uint8_t * buf, int buf_size)
{
    unsigned int size = buf_size;
    block_t * block;

    // Remove any dead placeholders. This might consume our marker
    while (head && head->offset >= head->length && head->next) 
    {
        block = (block_t *)head;
        head = head->next;
        // Consuming the marker, we should signal the transcoder that the timestamps are resetting
        if (block->length == 0)
            m_transcoder->streamReset();

        freeBlock(block);
    }

    if ((bytesBuffered - size) < AVBUFSIZE)
        m_transcoder->streamFlushed();

    while (size && head && ((head->offset < head->length) || head->next)) 
    {
        size_t l = head->length - head->offset;
        if (l > size)
            l = size;
        memcpy(buf, (void *)(head->data + head->offset), l);
        buf += l;
        size -= l;
        head->offset += l;
        if (head->offset >= head->length && head->next) {
            block = (block_t *)head;
            head = head->next;
            freeBlock(block);
            if (head->length == 0) {
                // We found our stream restart marker, return a short block
                break;
            }
        }
    }

    size = buf_size - size;
    __sync_fetch_and_add(&bytesBuffered, -size);

    if (size != (unsigned int)buf_size && (!head || head->length)) 
        INFOLOG << "readBuffer is returning a short block " << size << " < " << buf_size;

    return size? size : EOF;
}

static void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt, const char *tag)
{
    AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;
    char buf1[64];
    char buf2[64];
    char buf3[64];
    char buf4[64];
    char buf5[64];
    char buf6[64];

    AVCodec * codec = avcodec_find_decoder(fmt_ctx->streams[pkt->stream_index]->codecpar->codec_id);

    DEBUGLOG << tag 
        << ": pts:" << av_ts_make_string(buf1, pkt->pts)
        << " pts_time:" << av_ts_make_time_string(buf2, pkt->pts, time_base)
        << " dts:" << av_ts_make_string(buf3, pkt->dts)
        << " dts_time:" << av_ts_make_time_string(buf4, pkt->dts, time_base)
        << " duration:" << av_ts_make_string(buf5, pkt->duration)
        << " duration_time:" << av_ts_make_time_string(buf6, pkt->duration, time_base)
        << " stream_index:" << pkt->stream_index
        << " codec:" << codec->name;
}

AVPacket * StreamBuffer::flush()
{
    int ret; 

    if (!m_initialized)
        return nullptr;

    av_init_packet(&m_pkt);

    // If no more packets, return null
    if ((ret = av_read_frame(m_iAVFContext, &m_pkt)) < 0) 
        return nullptr;

    // If a pmt change, don't bother, just return null
    if (m_iAVFContext->programs[0]->pmt_version != m_pmtVersion) 
        return nullptr;

    log_packet(m_iAVFContext, &m_pkt, "in");

    return &m_pkt;
}

AVPacket * StreamBuffer::getNextPacket(bool flush)
{
    int ret;

    if (bytesBuffered < (flush? AVBUFSIZE : AVBUFSIZE*2))
        return nullptr;

    av_init_packet(&m_pkt);

    if (!m_initialized)
    {
        if ((ret = avformat_open_input(&m_iAVFContext, nullptr, m_iFormat, nullptr)) < 0) 
        {
            if (ret != -EAGAIN)
                ERRORLOG << "Unable to open input ctx " << ret;

            return nullptr;
        }

        INFOLOG << "stream opened, nb_streams: " <<   m_iAVFContext->nb_streams;
        INFOLOG << "stream 0 " << avcodec_descriptor_get(m_iAVFContext->streams[0]->codecpar->codec_id)->name;
        INFOLOG << "stream 1 " << avcodec_descriptor_get(m_iAVFContext->streams[1]->codecpar->codec_id)->name;
        m_pmtVersion = m_iAVFContext->programs[0]->pmt_version;

        m_initialized = true;
    }

    if ((ret = av_read_frame(m_iAVFContext, &m_pkt)) < 0) 
    {
        if (ret == -EAGAIN) 
            DEBUGLOG << "Demuxer returned EAGAIN";
        else 
            ERRORLOG << "Demuxer returned an error: " << ret;
        return nullptr;
    }

    if (m_iAVFContext->programs[0]->pmt_version != m_pmtVersion) 
    {
        INFOLOG << "PMTVersion changed from " << m_pmtVersion << " to " << m_iAVFContext->programs[0]->pmt_version;
        m_pmtVersion = m_iAVFContext->programs[0]->pmt_version;
    }

    log_packet(m_iAVFContext, &m_pkt, "in");

    return &m_pkt;
}

void StreamBuffer::releasePacket(AVPacket * packet)
{
    av_packet_unref(packet);
}