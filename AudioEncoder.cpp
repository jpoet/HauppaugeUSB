#include "AudioEncoder.h"
#include "Logger.h"

AudioEncoder::AudioEncoder()
: m_codecId(AV_CODEC_ID_NONE)
, m_aContext(nullptr)
{
    // Find the AC3 encoder
    AVCodec * ac3encoder = avcodec_find_encoder(AV_CODEC_ID_AC3);
    if (!ac3encoder) 
    {
        ERRORLOG << "Unable to find encoder AC3";
        return;
    }

    // Alloc an encoder context
    m_aContext = avcodec_alloc_context3(ac3encoder);

    AVRational tb;
    tb.num = 0;
    tb.den = 1;
    m_aContext->channel_layout = AV_CH_LAYOUT_5POINT1;
    m_aContext->channels = 6;
    m_aContext->time_base = tb;
    m_aContext->sample_rate = 48000;
    m_aContext->sample_fmt = AV_SAMPLE_FMT_FLTP;

    // Open the encoder
    if (avcodec_open2(m_aContext, ac3encoder, nullptr) < 0) 
    {
        ERRORLOG << "Unable to open AC3 EncoderContext";
        avcodec_free_context(&m_aContext);
        return;
    }
}

AudioEncoder::~AudioEncoder()
{
    avcodec_free_context(&m_aContext);
}

bool AudioEncoder::putFrame(AVFrame * frame) 
{
    int ret = avcodec_send_frame(m_aContext, frame);
    if (ret < 0) 
    {
        ERRORLOG << "Unable to encode audio frame: " << ret;
        return false;
    }

    return true;
}

AVPacket * AudioEncoder::getNextPacket()
{
    int ret;

    av_init_packet(&m_pkt);
    ret = avcodec_receive_packet(m_aContext, &m_pkt);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) 
    {
        DEBUGLOG << "Nothing to do right now";
        return nullptr;
    }
    else if (ret < 0) 
    {
        ERRORLOG << "Unable to receive frame from encoder: " << ret;
        return nullptr;
    }

    m_pkt.stream_index = 1;

    return &m_pkt;
}

void AudioEncoder::releasePacket(AVPacket * pkt)
{
    av_packet_unref(pkt);
}
