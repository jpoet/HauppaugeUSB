#include "Transcoder.h"
#include "Logger.h"

#define ACTIVE_GRAPH_MODE 1

/**
 * The Transcoder class is designed to sit between the Encoder callback and 
 * a consumer. It is capable of calling a supplied callback the same way the
 * Encoder does, as well as writing to a file, or stdout.
 * 
 * The class is written as a transcoding filter graph. As the Encoder
 * feeds data into the Transcoder, it is synchronously transcoded to 
 * a new mpeg transport stream. The video stream is copied in its encoded
 * state, but the audio is transcoded to 5.1 AC-3 audio. Because of its
 * synchronous design, it is not thread safe as a whole. Destroying the
 * Transocder will attempt to flush the filter pipeline, so any thread pushing
 * data into the Transcoder should be stopped before the Transcoder is
 * destroyed.
 * 
 * The individual parts of the Transcoder are all thread safe if the design
 * was switched to a passive, thread-based approach. As long as the Encoder
 * simply pushes data into the StreamBuffer class, a separate thread
 * could be used to call the processData method and pull data through 
 * the filter graph. Even more, the consumer could drive the filter
 * graph itself by periodically calling the processData method.
 * 
 * The ACTIVE_GRAPH_MODE define determines how the Transcoder operates.
 * If set to non-zero, it will operate in the "push" mode, being driven by the
 * Encoder thread itself. If set to 0, then the external consumer
 * must call the processData method periodically.
 */
Transcoder::Transcoder(const std::string& output_filename, DataTransfer::callback_t * cb)
: m_streamBuffer(new StreamBuffer(this))
, m_audioDecoder(nullptr)
, m_streamWriter(new StreamWriter(this, output_filename, cb))
, m_audioBuffer(new AudioBuffer())
, m_audioEncoder(new AudioEncoder())
, m_output_filename(output_filename)
, m_client(cb)
, m_cb(std::bind(&Transcoder::acceptData, this, std::placeholders::_1,
                     std::placeholders::_2))
, m_flushing(false)
, m_curAudioStreamIndex(-1)
{
    av_log_set_level(AV_LOG_QUIET);
}

Transcoder::~Transcoder()
{
    // Flush the stream
    DEBUGLOG << "Flushing stream";
    AVPacket * pkt;
    while ((pkt = m_streamBuffer->flush()) != nullptr)
    {
        if (pkt->stream_index >= 1) 
            handleAudioPacket(pkt);
        else 
            m_streamWriter->writePacket(pkt);

        m_streamBuffer->releasePacket(pkt);
    }

    AVFrame * frame;
    int64_t pts = 0;
    while ((frame = m_audioBuffer->getFrame(true)) != nullptr)
    {
        DEBUGLOG << "Got frame of audio while flushing: " << frame->nb_samples << " samples";
        // encode the raw audio
        m_audioEncoder->putFrame(frame);
        pts = frame->pts;
        m_audioBuffer->releaseFrame(frame);
        // Get packets from encoder and write to stream
        while ((pkt = m_audioEncoder->getNextPacket()) != nullptr)
        {
            DEBUGLOG << "Got packet of encoded audio while flushing: " << pkt->size << " bytes";
            pkt->pts = pkt->dts = pts;
            pts += 2880;
            m_streamWriter->writePacket(pkt);
            m_audioEncoder->releasePacket(pkt);
        }
    }

    DEBUGLOG << "Flushing encoder";
    m_audioEncoder->putFrame(nullptr); // flush encoder
    while ((pkt = m_audioEncoder->getNextPacket()) != nullptr)
    {
        WARNLOG << "Got packet of encoded audio after flush: " << pkt->size << " bytes";
        pkt->pts = pkt->dts = pts;
        pts += 2880;
        m_streamWriter->writePacket(pkt);
        m_audioEncoder->releasePacket(pkt);
    }

    delete m_streamWriter;
    delete m_audioEncoder;
    delete m_audioBuffer;
    delete m_audioDecoder;
    delete m_streamBuffer;
}

void Transcoder::streamReset()
{
    INFOLOG << "Stream has restarted";
    m_flushing = true;
    delete m_audioDecoder; // reset this because it might be changing
    m_audioDecoder = nullptr;
    m_curAudioStreamIndex = -1;
}

void Transcoder::streamFlushed()
{
    INFOLOG << "Stream has flushed";
}

void Transcoder::handleAudioPacket(AVPacket * pkt)
{
    if (m_curAudioStreamIndex < 0)
        m_curAudioStreamIndex = pkt->stream_index;

    if (m_curAudioStreamIndex != pkt->stream_index)
        return; // Ignore the packet if it's not the "current" audio stream

    if (m_audioDecoder == nullptr || m_streamBuffer->m_iAVFContext->streams[pkt->stream_index]->codecpar->codec_id != m_audioDecoder->m_codecId)
    {
        // Init audioDecoder
        delete m_audioDecoder;
        m_audioDecoder = new AudioDecoder();
        DEBUGLOG << "Initing audioDecoder with codec: " << avcodec_descriptor_get(m_streamBuffer->m_iAVFContext->streams[pkt->stream_index]->codecpar->codec_id)->name;
        if (!m_audioDecoder->initDecoder(m_streamBuffer->m_iAVFContext, m_streamBuffer->m_iAVFContext->streams[pkt->stream_index]->codecpar))
        {
            delete m_audioDecoder;
            m_audioDecoder = nullptr;
        }
    }
    if (m_audioDecoder) 
    {
        m_audioDecoder->putPacket(pkt);
        AVFrame * frame;
        while ((frame = m_audioDecoder->getNextFrame()) != nullptr)
        {
            // buffer the decoded audio
            m_audioBuffer->putFrame(frame);
            m_audioDecoder->releaseFrame(frame);
            while ((frame = m_audioBuffer->getFrame()) != nullptr)
            {
                // encode the raw audio
                m_audioEncoder->putFrame(frame);
                int64_t pts = frame->pts;
                m_audioBuffer->releaseFrame(frame);
                AVPacket * pkt2;
                // Get packets from encoder and write to stream
                while ((pkt2=m_audioEncoder->getNextPacket()) != nullptr)
                {
                    pkt2->pts = pkt2->dts = pts;
                    m_streamWriter->writePacket(pkt2);
                    m_audioEncoder->releasePacket(pkt2);
                }
            }
        }
    }
}

void Transcoder::acceptData(void * ptr, size_t length)
{
    DEBUGLOG << "Got " << length << " bytes of data";
    m_streamBuffer->putData(ptr, length);

#if ACTIVE_GRAPH_MODE
    processData();
#endif
}

void Transcoder::processData()
{
    AVPacket * pkt;
    while ((pkt = m_streamBuffer->getNextPacket(m_flushing)) != nullptr)
    {
        if (pkt->stream_index >= 1) 
            handleAudioPacket(pkt);  // Must be of type audio
        else 
            m_streamWriter->writePacket(pkt); // It's video, just write it

        m_streamBuffer->releasePacket(pkt);
    }

    if (m_flushing)
    {
        AVFrame * frame;
        int64_t pts;
        // Flush the audio buffer of all remaining audio data
        // Even if it means we get a bit of silence on the end
        while ((frame = m_audioBuffer->getFrame(true)) != nullptr)
        {
            DEBUGLOG << "Got frame of audio while flushing: " << frame->nb_samples << " samples";
            // encode the raw audio
            m_audioEncoder->putFrame(frame);
            pts = frame->pts;
            m_audioBuffer->releaseFrame(frame);
            // Get packets from encoder and write to stream
            while ((pkt = m_audioEncoder->getNextPacket()) != nullptr)
            {
                DEBUGLOG << "Got packet of encoded audio while flushing: " << pkt->size << " bytes";
                pkt->pts = pkt->dts = pts;
                pts += 2880;
                m_streamWriter->writePacket(pkt);
                m_audioEncoder->releasePacket(pkt);
            }
        }
        m_flushing = false;
    }
}