#ifndef PHASESHIFTER_H
#define PHASESHIFTER_H

#include <math.h>
#include <string.h>
#include <xmmintrin.h>

class PhaseShifter
{
  private:
    float * m_filt;
    float * m_hist;
    int m_filtLen;
    int m_filtLen2; // keep this because we need filtLength/2 pretty often
    int m_bufsize;

    float * AlignedNew(int floats)
    {
        float * f =
            new float[floats + 128 / sizeof(float)]; // allocate extra so we can
                                                     // align it
        char * t = (char *)(((long)f + sizeof(void *) + 63) & ~63);
        *((float **)(t - sizeof(void *))) = f;
        return (float *)t;
    }

    void AlignedDelete(float * f)
    {
        delete *((float **)((char *)f - sizeof(void *)));
    }

  public:
    PhaseShifter(int filterLength, int bufsize)
    : m_filtLen(filterLength)
    , m_filtLen2(filterLength / 2)
    , m_bufsize(bufsize)
    {
        m_filt = AlignedNew(m_filtLen);

        for (int i = 1; i <= m_filtLen; i++)
            m_filt[m_filtLen - i] = 1 / ((i - m_filtLen2) - 0.5) / M_PI;

        m_hist = AlignedNew(m_filtLen2 + bufsize + m_filtLen2);
        memset(m_hist, 0, sizeof(float) * m_filtLen2);
    }

    ~PhaseShifter()
    {
        AlignedDelete(m_filt);
        AlignedDelete(m_hist);
    }

    int GetFilterLength() { return m_filtLen; }

    void Reset() { memset(m_hist, 0, sizeof(float) * m_filtLen2); }

    void * GetBuffer() { return m_hist + m_filtLen2; }
    int GetBufferSize() { return m_bufsize; }
    int GetPeekSize() { return m_filtLen2; }

    // Need to be provided lfilt/2 samples beyond our window as a peek
    // ahead for the filter
    int Process(float * out)
    {
        int i, l;
        double yt;
        float * p = out;

        for (l = 0; l < m_bufsize; l++)
        {
            float * s = &m_hist[l];
            float * f = m_filt;
            yt = *s++ * *f++;
            for (i = 1; i < m_filtLen; i++)
                yt += *s++ * *f++;
            *p++ = yt;
        }

        // Save off the last portion to use as history for the next window
        memcpy(m_hist, m_hist + m_bufsize, m_filtLen2 * sizeof(float));

        return p - out;
    }
};

#endif
