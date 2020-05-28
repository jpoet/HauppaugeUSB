#ifndef ALLPASSFILTER_H
#define ALLPASSFILTER_H

#include <math.h>

class AllpassFilter
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

    AllpassFilter(double Fc, double Q)
    {
        double K = tan(M_PI * Fc);

        double norm = 1 / (1 + K / Q + K * K);
        a0 = K / Q * norm;
        a1 = 0;
        a2 = -a0;
        b1 = 2 * (K * K - 1) * norm;
        b2 = (1 - K / Q + K * K) * norm;

        z1 = z2 = 0;
    }
};

#endif
