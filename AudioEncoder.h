#ifndef AudioEncoder_h_
#define AudioEncoder_h_

extern "C" 
{
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
}

class Trasnscoder;

class AudioEncoder 
{
    friend class Transcoder;
private:
    AVCodecID m_codecId;
    AVCodecContext * m_aContext;
    AVPacket m_pkt;

    AudioEncoder();
    ~AudioEncoder();

    bool initEncoder(AVFormatContext * avFormatContext);
    bool putFrame(AVFrame * frame);
    AVPacket * getNextPacket();
    void releasePacket(AVPacket * packet);
};

#endif