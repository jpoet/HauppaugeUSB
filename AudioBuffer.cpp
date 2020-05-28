#include "AudioBuffer.h"
#include "Logger.h"

#define MINUS3DB (1.0 / 1.414213)

AudioBuffer::AudioBuffer(bool upmix2to51)
: m_maxAllocated(0)
, m_bytesBuffered(0)
, m_headFree(nullptr)
, m_tailFree(&m_headFree)
, m_head(nullptr)
, m_tail(&m_head)
, m_delay(48000, 12)
, m_filterCenter(4000.0 / 48000.0, .5)
, m_filterLfe(200.0 / 48000.0, .5)
, m_filterSurround(7000.0 / 48000.0, .5)
, m_filterLeft(10000.0 / 48000.0, 0.1)
, m_filterRight(10000.0 / 48000.0, 0.1)
, m_phaseShifter(PHASEHIFTER_FILTER_LENGTH)
, m_upmix2to51(upmix2to51)
{
    m_tempbuf = new uint8_t[(1536 + PHASEHIFTER_FILTER_LENGTH) * sizeof(float)];
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

    delete m_tempbuf;
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

    m_filterLeft.Reset();
    m_filterRight.Reset();
    m_filterCenter.Reset();
    m_filterLfe.Reset();
    m_filterSurround.Reset();
    m_phaseShifter.Reset();
    m_delay.Reset();
}

AudioBuffer::block_t * AudioBuffer::GetBlock(unsigned int blocksize)
{
    block_t * block;

    blocksize = (blocksize + 31) & ~31;
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

    block->applyPhase = false;

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
        if (frame->channels == 2 && m_upmix2to51)
        {
            // upmix stereo using "Dolby Surround"
            block->applyPhase = true; // apply phase shift during read
            for (int i = 0; i < frame->nb_samples; i++)
            {
                float l = *((float *)(frame->data[0]) + i);
                float r = *((float *)(frame->data[1]) + i);
                *((float *)(block->bufs[0]) + i) = m_filterLeft.Process(l);
                *((float *)(block->bufs[1]) + i) = m_filterRight.Process(r);
                *((float *)(block->bufs[2]) + i) =
                    m_filterCenter.Process(l + r) * MINUS3DB;
                *((float *)(block->bufs[3]) + i) = m_filterLfe.Process(l + r);
                *((float *)(block->bufs[4]) + i) = *((float *)(block->bufs[5])
                                                     + i) =
                    m_delay.Process(m_filterSurround.Process(l - r) * MINUS3DB);
            }
        }
        else
        {
            for (int i = 0; i < frame->channels; i++)
                memcpy(block->bufs[i], frame->data[i], bs);
            for (int i = frame->channels; i < 6; i++)
                memset(block->bufs[i], 0, bs);
        }
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
    int peekAmountBytes = 0;
    if (m_head && m_head->applyPhase)
        peekAmountBytes = m_phaseShifter.GetFilterLength() / 2
                          * sizeof(float); // Have to provide filt/2 extra data
    if (m_bytesBuffered >= (bs + peekAmountBytes) || (flush && m_bytesBuffered))
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
            if (peekAmountBytes)
            {
                // Copy the first 4 channels
                for (int i = 0; i < 4; i++)
                    memcpy(frame->data[i] + c,
                           (const void *)(b->bufs[i] + b->offset), l);
                // For the surround, copy into a separate buffer
                memcpy(m_tempbuf + c, (const void *)(b->bufs[4] + b->offset),
                       l);
            }
            else
            {
                for (int i = 0; i < 6; i++)
                    memcpy(frame->data[i] + c,
                           (const void *)(b->bufs[i] + b->offset), l);
            }
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
        else if (peekAmountBytes)
        {
            // peek ahead for the phase shift
            int peekSize = peekAmountBytes;
            b = (block_t *)m_head;
            unsigned int boffset = b ? b->offset : 0;
            while (peekSize && b && (boffset < b->length || b->next))
            {
                int l = b->length - boffset;
                if (l > peekSize)
                    l = peekSize;
                memcpy(m_tempbuf + c, (const void *)(b->bufs[4] + boffset), l);
                boffset += l;
                peekSize -= l;
                c += l;
                if (boffset >= b->length)
                {
                    b = b->next;
                    boffset = 0;
                }
            }
            if (c < (bs + peekAmountBytes))
                memset(m_tempbuf + c, 0, (bs + peekAmountBytes) - c);
            // memcpy(frame->data[4], m_tempbuf, 1536 * sizeof(float));
            c = m_phaseShifter.Process((float *)m_tempbuf,
                                       (float *)frame->data[4],
                                       (bs + peekAmountBytes) / sizeof(float));
            memcpy(frame->data[5], frame->data[4], 1536 * sizeof(float));
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