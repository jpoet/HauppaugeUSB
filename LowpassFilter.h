#ifndef LOWPASSFILTER_H
#define LOWPASSFILTER_H

#include <math.h>

class LowpassFilter
{
  private:
    double a0, a1, a2, b1, b2;
    double z1, z2;

  public:
    float Process(float in)
    {
        double out = in * a0 + z1;
        z1 = in * a1 + z2 - b1 * out;
        z2 = in * a2 - b2 * out;

        return (float)out;
    }

    void Reset() { z1 = z2 = 0; }

    LowpassFilter(double Fc, double Q)
    {
        double K = tan(M_PI * Fc);
        double norm = 1 / (1 + K / Q + K * K);

        a2 = a0 = K * K * norm;
        a1 = 2 * a0;
        b1 = 2 * (K * K - 1) * norm;
        b2 = (1 - K / Q + K * K) * norm;

        z1 = z2 = 0;
    }
};

#endif
