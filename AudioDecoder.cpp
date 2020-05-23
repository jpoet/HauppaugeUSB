#include "AudioDecoder.h"
#include "Logger.h"

AudioDecoder::AudioDecoder()
: m_codecId(AV_CODEC_ID_NONE)
, m_aContext(nullptr)
{}

AudioDecoder::~AudioDecoder()
{
    //delete m_aContext; // Don't delete this, it will get closed when its parent AVFormat is freed
}

bool AudioDecoder::isNewCodec(AVCodecContext * aContext)
{
    return (aContext->codec_id != m_codecId);
}

bool AudioDecoder::initDecoder(AVFormatContext * avFormatContext, AVCodecParameters * codecParams)
{
    int ret;

    AVCodec * codec = avcodec_find_decoder(codecParams->codec_id);
    m_aContext = avcodec_alloc_context3(codec);

    avcodec_parameters_to_context(m_aContext, codecParams);

    ret = avcodec_open2(m_aContext, codec, nullptr);
    if (ret < 0) 
    {
        ERRORLOG << "Failed to open codec for stream: " << ret;
        return false;
    }
    INFOLOG << "Opened codec " << codec->long_name;

    m_codecId = codecParams->codec_id;

    return true;
}

bool AudioDecoder::putPacket(AVPacket * packet) 
{
    int ret;
    ret = avcodec_send_packet(m_aContext, packet);
    if (ret < 0) 
    {
        ERRORLOG << "Decoding failed: " << ret;
        return false;
    }

    return true;
}

AVFrame * AudioDecoder::getNextFrame()
{
    AVFrame * frame =  av_frame_alloc();
    int ret = avcodec_receive_frame(m_aContext, frame);
    if (ret < 0) 
    {
        if (ret != -EAGAIN)
            ERRORLOG << "Decoding failed";
        return nullptr;
    }

    INFOLOG << "Decoded frame for code " << m_aContext->codec->long_name;
    return frame;
}

void AudioDecoder::releaseFrame(AVFrame * frame)
{
    av_frame_free(&frame);
}
