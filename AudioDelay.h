#ifndef AUDIO_DELAY_H
#define AUDIO_DELAY_H

#include <string.h>

class AudioDelay
{
  private:
    int m_samples;
    int m_index;
    float * m_buf;

  public:
    AudioDelay(int Fc, int delayInMillis)
    : m_samples((int)((long)delayInMillis * Fc / 1000))
    , m_index(0)
    {
        m_buf = new float[m_samples];
        memset(m_buf, 0, m_samples * sizeof(float));
    }

    ~AudioDelay() { delete m_buf; }

    void Reset()
    {
        memset(m_buf, 0, m_samples * sizeof(float));
        m_index = 0;
    }

    float Process(float in)
    {
        float out = m_buf[m_index];
        m_buf[m_index] = in;
        if (++m_index == m_samples)
            m_index = 0;
        return out;
    }
};

#endif