#ifndef AudioEncoder_h_
#define AudioEncoder_h_

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

class Trasnscoder;

class AudioEncoder
{
    friend class Transcoder;

  private:
    AVCodecID m_codecId;
    AVCodecContext * m_aContext;
    AVPacket * m_pkt;

    AudioEncoder();
    ~AudioEncoder();

    bool initEncoder(AVFormatContext * avFormatContext);
    bool PutFrame(AVFrame * frame);
    AVPacket * GetNextPacket();
    void ReleasePacket(AVPacket * packet);
};

#endif
