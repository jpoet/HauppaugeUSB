#include "AudioBuffer.h"
#include "Logger.h"

AudioBuffer::AudioBuffer()
: maxAllocated(0)
, bytesBuffered(0)
, headFree(nullptr)
, tailFree(&headFree)
, head(nullptr)
, tail(&head)
{
}

AudioBuffer::~AudioBuffer()
{
    while (headFree) 
    {
        block_t * b = headFree->next;
        delete headFree;
        headFree = b;
    }

    while (head) 
    {
        block_t * b = head->next;
        delete head;
        head = b;
    }
}

void AudioBuffer::reset()
{
    bytesBuffered = 0;

    block_t * block;
    while ((block = const_cast<block_t *>(head)) != nullptr && head->next) 
    {
        head = head->next;
        head->offset = head->length;
        freeBlock(block);
    }
}

AudioBuffer::block_t * AudioBuffer::getBlock(unsigned int blocksize)
{
    block_t * block;

    blocksize = (blocksize + 31) & ~32;
    unsigned int allocated = (blocksize * 8); // Align to 128-byte page boundary

    // We can't remove the last one, it has to stay so tailFree stays valid
    while ((block = const_cast<block_t *>(headFree)) != nullptr && headFree->next && headFree->allocated < blocksize) 
    {
        headFree = block->next;
        delete block;
    }

    // We can't remove the last one, it has to stay so tailFree stays valid
    if ((block = const_cast<block_t *>(headFree)) != nullptr && headFree->next) 
    {
        headFree = block->next;
    }
    else {
        if (allocated > maxAllocated)
            maxAllocated = allocated;
        allocated = maxAllocated;
        block = (block_t *)new char[sizeof(block_t) + allocated];
        block->allocated = allocated;
    }

    for (int i=0; i<8; i++)
        block->bufs[i] = block->data + i * blocksize;

    return block;
}

void AudioBuffer::freeBlock(block_t * block) 
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

void AudioBuffer::putFrame(AVFrame * frame)
{
    int bs = frame->nb_samples * sizeof(float);
    block_t * block = nullptr;
    switch (frame->format) 
    {
        case AV_SAMPLE_FMT_FLTP:
            block = getBlock(frame->linesize[0]);
            for (int i=0; i<frame->channels; i++)
                memcpy(block->bufs[i], frame->data[i], bs);
            for (int i=frame->channels;i<6; i++)
                memset(block->bufs[i], 0, bs);
            break;
        case AV_SAMPLE_FMT_S16: // Another popular format
            block = getBlock(bs);
            switch (frame->channel_layout) {
                case AV_CH_LAYOUT_STEREO:
                case AV_CH_LAYOUT_5POINT1:
                case AV_CH_LAYOUT_7POINT1:
                    for (int i=0; i<frame->nb_samples; i++) 
                    {
                        for (int j=0; j<frame->channels; j++) 
                        {
                            *((float *)(&block->data[j]) + i) = *((int16_t *)(&frame->data[0]) + (i * frame->channels) + j) / 32768.0;
                        }
                    }
                    break;
                default:
                    ERRORLOG << "Unhandled source format channel layout: " << frame->channel_layout;
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
        *tail = block;
        tail = const_cast<volatile block_t * volatile *>(&block->next);
        __sync_fetch_and_add(&bytesBuffered, bs);
    }
}

AVFrame * AudioBuffer::getFrame(bool flush)
{
    block_t * b;

    while (head && head->offset >= head->length && head->next) 
    {
        b = (block_t *)head;
        head = head->next;
        freeBlock(b);
    }

    int bs = 1536 * sizeof(float);
    if (bytesBuffered >= bs || (flush && bytesBuffered)) 
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
        frame->pts = head->pts + (int64_t)head->offset / sizeof(float) * 90000 / 48000;
        av_frame_get_buffer(frame, 32);
        int size = bs;
        int c = 0;
        while (size && head && (head->offset < head->length || head->next)) 
        {
            b = (block_t *)head;
            int l = b->length - b->offset;
            if (l > size)
                l = size;
            for (int i=0; i<6; i++) 
                memcpy(frame->data[i] + c, (const void *)(b->bufs[i] + b->offset), l);
            b->offset += l;
            size -= l;
            c += l;
            if (b->offset >= b->length && b->next) {
                head = head->next;
                freeBlock(b);
            }
        }
        __sync_fetch_and_add(&bytesBuffered, -c);

        if (c < bs)
        {
            for (int i=0; i<6; i++) 
                memset(frame->data[i] + c, 0, bs - c);
        }

        return frame;
    }
    return nullptr;
}

void AudioBuffer::releaseFrame(AVFrame * frame)
{
    if (frame)
        av_frame_free(&frame);
}