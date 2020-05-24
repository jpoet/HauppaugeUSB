#ifndef AudioDecoder_h_
#define AudioDecoder_h_

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
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

    bool IsNewCodec(AVCodecContext * aContext);
    bool InitDecoder(AVFormatContext * avFormatContext,
                     AVCodecParameters * codecParams);
    bool PutPacket(AVPacket * packet);
    AVFrame * GetNextFrame();
    void ReleaseFrame(AVFrame * frame);
};

#endif