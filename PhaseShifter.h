#ifndef PHASESHIFTER_H
#define PHASESHIFTER_H

#include <math.h>
#include <string.h>

class PhaseShifter
{
  private:
    float * m_filt;
    float * m_hist;
    int m_filtLen;
    int m_filtLen2; // keep this because we need filtLength/2 pretty often

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
    PhaseShifter(int filterLength)
    : m_filtLen(filterLength)
    , m_filtLen2(filterLength / 2)
    {
        m_filt = AlignedNew(m_filtLen);

        for (int i = 1; i <= m_filtLen; i++)
            m_filt[m_filtLen - i] = 1 / ((i - m_filtLen2) - 0.5) / M_PI;

        m_hist = AlignedNew(m_filtLen2);
        memset(m_hist, 0, sizeof(float) * m_filtLen2);
    }

    ~PhaseShifter()
    {
        AlignedDelete(m_filt);
        AlignedDelete(m_hist);
    }

    int GetFilterLength() { return m_filtLen; }

    void Reset() { memset(m_hist, 0, sizeof(float) * m_filtLen2); }

    // Need to be provided lfilt/2 samples beyond our window as a peek
    // ahead for the filter
    int Process(float * in, float * out, int npt)
    {
        int i, l;
        double yt;
        float * p = out;

        int hc = m_filtLen2;
        int ic = m_filtLen2;
        for (l = 0; l < m_filtLen2; l++)
        {
            // take hc samples from history
            // take ic from input
            float * f = m_filt;
            float * h = &m_hist[l];
            float * s = in;
            yt = *h++ * *f++;
            for (i = 1; i < hc; i++)
                yt += *h++ * *f++;
            for (i = 0; i < ic; i++)
                yt += *s++ * *f++;
            *p++ = yt;
            ic++;
            hc--;
        }

        // Second case is when our window is completely in the input vector
        for (l = 0; l < npt - m_filtLen; l++)
        {
            float * s = &in[l];
            float * f = m_filt;
            yt = *s++ * *f++;
            for (i = 1; i < m_filtLen; i++)
                yt += *s++ * *f++;
            *p++ = yt;
        }

        // save off last lifilt/2 of data (not the extra part)
        memcpy(m_hist, in + npt - m_filtLen, m_filtLen2 * sizeof(float));

        return p - out;
    }
};

#endif
