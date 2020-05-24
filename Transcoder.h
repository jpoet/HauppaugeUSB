#ifndef Transcoder_h_
#define Transcoder_h_

#include <stdint.h>
#include <string>

#include "AudioBuffer.h"
#include "AudioDecoder.h"
#include "AudioEncoder.h"
#include "Hauppauge/Common/AVOutput.h"
#include "StreamBuffer.h"
#include "StreamWriter.h"

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

    void StreamReset();
    void StreamFlushed();

    void HandleAudioPacket(AVPacket * pkt);

  public:
    Transcoder(const std::string & filename = std::string(),
               DataTransfer::callback_t * cb = nullptr);
    ~Transcoder();

    void AcceptData(void * ptr, size_t length);
    void ProcessData();
    DataTransfer::callback_t * GetCallback() { return &m_cb; }
    operator DataTransfer::callback_t *() { return &m_cb; }
};

#endif
