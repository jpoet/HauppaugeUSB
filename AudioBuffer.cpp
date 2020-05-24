#include "AudioBuffer.h"
#include "Logger.h"

AudioBuffer::AudioBuffer()
: m_maxAllocated(0)
, m_bytesBuffered(0)
, m_headFree(nullptr)
, m_tailFree(&m_headFree)
, m_head(nullptr)
, m_tail(&m_head)
{
}

AudioBuffer::~AudioBuffer()
{
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

void AudioBuffer::Reset()
{
    m_bytesBuffered = 0;

    block_t * block;
    while ((block = const_cast<block_t *>(m_head)) != nullptr && m_head->next)
    {
        m_head = m_head->next;
        m_head->offset = m_head->length;
        FreeBlock(block);
    }
}

AudioBuffer::block_t * AudioBuffer::GetBlock(unsigned int blocksize)
{
    block_t * block;

    blocksize = (blocksize + 31) & ~32;
    unsigned int allocated = (blocksize * 8); // Align to 128-byte page boundary

    // We can't remove the last one, it has to stay so tailFree stays valid
    while ((block = const_cast<block_t *>(m_headFree)) != nullptr
           && m_headFree->next && m_headFree->allocated < blocksize)
    {
        m_headFree = block->next;
        delete block;
    }

    // We can't remove the last one, it has to stay so tailFree stays valid
    if ((block = const_cast<block_t *>(m_headFree)) != nullptr
        && m_headFree->next)
    {
        m_headFree = block->next;
    }
    else
    {
        if (allocated > m_maxAllocated)
            m_maxAllocated = allocated;
        allocated = m_maxAllocated;
        block = (block_t *)new char[sizeof(block_t) + allocated];
        block->allocated = allocated;
    }

    for (int i = 0; i < 8; i++)
        block->bufs[i] = block->data + i * blocksize;

    return block;
}

void AudioBuffer::FreeBlock(block_t * block)
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

void AudioBuffer::PutFrame(AVFrame * frame)
{
    int bs = frame->nb_samples * sizeof(float);
    block_t * block = nullptr;
    switch (frame->format)
    {
    case AV_SAMPLE_FMT_FLTP:
        block = GetBlock(frame->linesize[0]);
        for (int i = 0; i < frame->channels; i++)
            memcpy(block->bufs[i], frame->data[i], bs);
        for (int i = frame->channels; i < 6; i++)
            memset(block->bufs[i], 0, bs);
        break;
    case AV_SAMPLE_FMT_S16: // Another popular format
        block = GetBlock(bs);
        switch (frame->channel_layout)
        {
        case AV_CH_LAYOUT_STEREO:
        case AV_CH_LAYOUT_5POINT1:
        case AV_CH_LAYOUT_7POINT1:
            for (int i = 0; i < frame->nb_samples; i++)
            {
                for (int j = 0; j < frame->channels; j++)
                {
                    *((float *)(&block->data[j]) + i) =
                        *((int16_t *)(&frame->data[0]) + (i * frame->channels)
                          + j)
                        / 32768.0;
                }
            }
            break;
        default:
            ERRORLOG << "Unhandled source format channel layout: "
                     << frame->channel_layout;
            break;
        }
        break;
    default:
        ERRORLOG << "Unhandled source format: " << frame->format;
        break;
    }

    if (block)
    {
        block->pts = frame->pts;
        block->length = bs;
        block->offset = 0;
        block->next = nullptr;
        *m_tail = block;
        m_tail = const_cast<volatile block_t * volatile *>(&block->next);
        __sync_fetch_and_add(&m_bytesBuffered, bs);
    }
}

AVFrame * AudioBuffer::GetFrame(bool flush)
{
    block_t * b;

    while (m_head && m_head->offset >= m_head->length && m_head->next)
    {
        b = (block_t *)m_head;
        m_head = m_head->next;
        FreeBlock(b);
    }

    int bs = 1536 * sizeof(float);
    if (m_bytesBuffered >= bs || (flush && m_bytesBuffered))
    {
        AVFrame * frame = av_frame_alloc();
        if (!frame)
        {
            ERRORLOG << "Unable to allocate frame!";
            return nullptr;
        }
        frame->channel_layout = AV_CH_LAYOUT_5POINT1;
        frame->channels = 6;
        frame->nb_samples = 1536;
        frame->format = AV_SAMPLE_FMT_FLTP;
        frame->pts = m_head->pts
                     + (int64_t)m_head->offset / sizeof(float) * 90000 / 48000;
        av_frame_get_buffer(frame, 32);
        int size = bs;
        int c = 0;
        while (size && m_head
               && (m_head->offset < m_head->length || m_head->next))
        {
            b = (block_t *)m_head;
            int l = b->length - b->offset;
            if (l > size)
                l = size;
            for (int i = 0; i < 6; i++)
                memcpy(frame->data[i] + c,
                       (const void *)(b->bufs[i] + b->offset), l);
            b->offset += l;
            size -= l;
            c += l;
            if (b->offset >= b->length && b->next)
            {
                m_head = m_head->next;
                FreeBlock(b);
            }
        }
        __sync_fetch_and_add(&m_bytesBuffered, -c);

        if (c < bs)
        {
            for (int i = 0; i < 6; i++)
                memset(frame->data[i] + c, 0, bs - c);
        }

        return frame;
    }
    return nullptr;
}

void AudioBuffer::ReleaseFrame(AVFrame * frame)
{
    if (frame)
        av_frame_free(&frame);
}