#ifndef Transcoder_h_
#define Transcoder_h_

#include <stdint.h>
#include <string>

#include "StreamBuffer.h"
#include "AudioDecoder.h"
#include "StreamWriter.h"
#include "AudioBuffer.h"
#include "AudioEncoder.h"
#include "Hauppauge/Common/AVOutput.h"

class Transcoder 
{
    friend class StreamBuffer;
    friend class StreamWriter;
private:
    StreamBuffer * m_streamBuffer;
    AudioDecoder * m_audioDecoder;
    StreamWriter * m_streamWriter;
    AudioBuffer * m_audioBuffer;
    AudioEncoder * m_audioEncoder;
    std::string m_output_filename;
    DataTransfer::callback_t * m_client;
    DataTransfer::callback_t m_cb;
    bool m_flushing;
    int m_curAudioStreamIndex;

    void streamReset();
    void streamFlushed();

    void handleAudioPacket(AVPacket * pkt);

public:
    Transcoder(const std::string& filename = std::string(), DataTransfer::callback_t * cb=nullptr);
    ~Transcoder();

    void acceptData(void * ptr, size_t length);
    void processData();
    DataTransfer::callback_t * getCallback() {
        return &m_cb;
    }
    operator DataTransfer::callback_t * () { return &m_cb; }
};

#endif
