#ifndef StreamWriter_h_
#define StreamWriter_h_

extern "C" {
    #include <libavformat/avformat.h>
}

#include "Hauppauge/Common/AVOutput.h"

#include <stdint.h>
#include <string>

class Transcoder;

class StreamWriter
{
    friend class Transcoder;
private:
    Transcoder * m_transcoder;
    std::string m_filename;
    DataTransfer::callback_t * m_cb;
    AVFormatContext * m_oAVFContext;
    AVIOContext * m_avioContext;
    AVOutputFormat * m_oFormat;
    bool m_initialized;
    int m_fd;

    StreamWriter(Transcoder * transcoder, const std::string& filename = std::string(), DataTransfer::callback_t * cb=nullptr);
    ~StreamWriter();

    bool writePacket(AVPacket * packet);

public:    
    int writeBuffer(uint8_t * buf, int buf_size);
};

#endif