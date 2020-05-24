#include "StreamBuffer.h"
#include "Logger.h"
#include "Transcoder.h"

extern "C" {
#include <libavutil/timestamp.h>
}

extern "C" int staticReadPacket(void * ctx, uint8_t * buf, int buf_size)
{
    return ((StreamBuffer *)ctx)->ReadPacket(buf, buf_size);
}

#define AVBUFSIZE 128 * 1024

StreamBuffer::StreamBuffer(Transcoder * transcoder)
: m_transcoder(transcoder)
, m_maxAllocated(0)
, m_bytesBuffered(0)
, m_headFree(nullptr)
, m_tailFree(&m_headFree)
, m_head(nullptr)
, m_tail(&m_head)
, m_iFormat(nullptr)
, m_avioContext(nullptr)
, m_iAVFContext(nullptr)
, m_initialized(false)
{
    unsigned char * buffer = (unsigned char *)av_malloc(AVBUFSIZE);
    m_iAVFContext = avformat_alloc_context();

    m_avioContext = avio_alloc_context(buffer, AVBUFSIZE, 0, (void *)this,
                                       &staticReadPacket, nullptr, nullptr);

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

    while (m_headFree)
    {
        block_t * b = m_headFree->next;
        delete m_headFree;
        m_headFree = b;
    }

    while (m_head)
    {
        block_t * b = m_head->next;
        delete m_head;
        m_head = b;
    }
}

StreamBuffer::block_t * StreamBuffer::GetBlock(unsigned int blocksize)
{
    block_t * block;

    // We can't remove the last one, it has to stay so tailFree stays valid
    while ((block = const_cast<block_t *>(m_headFree)) != nullptr
           && m_headFree->next != nullptr && m_headFree->allocated < blocksize)
    {
        m_headFree = block->next;
        delete block;
    }

    // We can't remove the last one, it has to stay so tailFree stays valid
    if ((block = const_cast<block_t *>(m_headFree)) != nullptr
        && m_headFree->next != nullptr)
    {
        m_headFree = block->next;
    }
    else
    {
        if (blocksize > m_maxAllocated)
            m_maxAllocated = blocksize;
        blocksize = m_maxAllocated;
        block = (block_t *)new char[sizeof(block_t) + blocksize];
        block->allocated = blocksize;
    }
    return block;
}

void StreamBuffer::FreeBlock(block_t * block)
{
    if (block->allocated < m_maxAllocated)
    {
        delete block; // Free this one because it's too small
    }
    else
    {
        block->next = nullptr;
        *m_tailFree = block;
        m_tailFree = const_cast<volatile block_t * volatile *>(&block->next);
    }
}

void StreamBuffer::PutData(void * ptr, size_t length)
{
    if (!ptr && !m_head)
        return; // Skip the very first marker

    // write the data into a buffer
    block_t * b = GetBlock(length);
    if (ptr == nullptr)
    {
        // This is a marker that the stream has stopped and started again
        // For now, we let this go through as a zero-length block
    }
    memcpy(b->data, ptr, length);
    b->length = length;
    b->offset = 0;
    b->next = nullptr;
    *m_tail = b;
    m_tail = const_cast<volatile block_t * volatile *>(&b->next);
    __sync_fetch_and_add(&m_bytesBuffered, length);
}

int StreamBuffer::ReadPacket(uint8_t * buf, int buf_size)
{
    unsigned int size = buf_size;
    block_t * block;

    // Remove any dead placeholders. This might consume our marker
    while (m_head && m_head->offset >= m_head->length && m_head->next)
    {
        block = (block_t *)m_head;
        m_head = m_head->next;
        // Consuming the marker, we should signal the transcoder that the
        // timestamps are resetting
        if (block->length == 0)
            m_transcoder->StreamReset();

        FreeBlock(block);
    }

    if ((m_bytesBuffered - size) < AVBUFSIZE)
        m_transcoder->StreamFlushed();

    while (size && m_head
           && ((m_head->offset < m_head->length) || m_head->next))
    {
        size_t l = m_head->length - m_head->offset;
        if (l > size)
            l = size;
        memcpy(buf, (void *)(m_head->data + m_head->offset), l);
        buf += l;
        size -= l;
        m_head->offset += l;
        if (m_head->offset >= m_head->length && m_head->next)
        {
            block = (block_t *)m_head;
            m_head = m_head->next;
            FreeBlock(block);
            if (m_head->length == 0)
            {
                // We found our stream restart marker, return a short block
                break;
            }
        }
    }

    size = buf_size - size;
    __sync_fetch_and_add(&m_bytesBuffered, -size);

    if (size != (unsigned int)buf_size && (!m_head || m_head->length))
        INFOLOG << "readBuffer is returning a short block " << size << " < "
                << buf_size;

    return size ? size : EOF;
}

static void log_packet(const AVFormatContext * fmt_ctx, const AVPacket * pkt,
                       const char * tag)
{
    AVRational * time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;
    char buf1[64];
    char buf2[64];
    char buf3[64];
    char buf4[64];
    char buf5[64];
    char buf6[64];

    AVCodec * codec = avcodec_find_decoder(
        fmt_ctx->streams[pkt->stream_index]->codecpar->codec_id);

    DEBUGLOG << tag << ": pts:" << av_ts_make_string(buf1, pkt->pts)
             << " pts_time:"
             << av_ts_make_time_string(buf2, pkt->pts, time_base)
             << " dts:" << av_ts_make_string(buf3, pkt->dts) << " dts_time:"
             << av_ts_make_time_string(buf4, pkt->dts, time_base)
             << " duration:" << av_ts_make_string(buf5, pkt->duration)
             << " duration_time:"
             << av_ts_make_time_string(buf6, pkt->duration, time_base)
             << " stream_index:" << pkt->stream_index
             << " codec:" << codec->name;
}

AVPacket * StreamBuffer::Flush()
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

AVPacket * StreamBuffer::GetNextPacket(bool flush)
{
    int ret;

    if (m_bytesBuffered < (flush ? AVBUFSIZE : AVBUFSIZE * 2))
        return nullptr;

    av_init_packet(&m_pkt);

    if (!m_initialized)
    {
        if ((ret = avformat_open_input(&m_iAVFContext, nullptr, m_iFormat,
                                       nullptr))
            < 0)
        {
            if (ret != -EAGAIN)
                ERRORLOG << "Unable to open input ctx " << ret;

            return nullptr;
        }

        INFOLOG << "stream opened, nb_streams: " << m_iAVFContext->nb_streams;
        INFOLOG << "stream 0 "
                << avcodec_descriptor_get(
                       m_iAVFContext->streams[0]->codecpar->codec_id)
                       ->name;
        INFOLOG << "stream 1 "
                << avcodec_descriptor_get(
                       m_iAVFContext->streams[1]->codecpar->codec_id)
                       ->name;
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
        INFOLOG << "PMTVersion changed from " << m_pmtVersion << " to "
                << m_iAVFContext->programs[0]->pmt_version;
        m_pmtVersion = m_iAVFContext->programs[0]->pmt_version;
    }

    log_packet(m_iAVFContext, &m_pkt, "in");

    return &m_pkt;
}

void StreamBuffer::ReleasePacket(AVPacket * packet)
{
    av_packet_unref(packet);
}