#ifndef AudioDecoder_h_
#define AudioDecoder_h_

extern "C" 
{
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
}

class Trasnscoder;

class AudioDecoder 
{
    friend class Transcoder;
private:
    AVCodecID m_codecId;
    AVCodecContext * m_aContext;

    AudioDecoder();
    ~AudioDecoder();

    bool isNewCodec(AVCodecContext * aContext);
    bool initDecoder(AVFormatContext * avFormatContext, AVCodecParameters * codecParams);
    bool putPacket(AVPacket * packet);
    AVFrame * getNextFrame();
    void releaseFrame(AVFrame * frame);
};

#endif