#pragma once

#include <cmath>

namespace automech
{
// Minimal RBJ biquad (transposed direct form II). Coefficients are only
// recomputed when a parameter changes, never per sample.
struct Biquad
{
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;

    void reset() noexcept { z1 = z2 = 0.0f; }

    float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void setLowpass (double sr, double f, double q) noexcept  { design (0, sr, f, q); }
    void setHighpass (double sr, double f, double q) noexcept { design (1, sr, f, q); }
    void setNotch (double sr, double f, double q) noexcept    { design (2, sr, f, q); }

private:
    void design (int type, double sr, double f, double q) noexcept
    {
        constexpr double pi = 3.14159265358979323846;
        f = std::fmin (std::fmax (f, 5.0), sr * 0.49);
        const double w0 = 2.0 * pi * f / sr;
        const double cw = std::cos (w0);
        const double alpha = std::sin (w0) / (2.0 * q);
        double nb0, nb1, nb2;

        switch (type)
        {
            case 0:  nb0 = (1.0 - cw) * 0.5; nb1 = 1.0 - cw;    nb2 = nb0; break;
            case 1:  nb0 = (1.0 + cw) * 0.5; nb1 = -(1.0 + cw); nb2 = nb0; break;
            default: nb0 = 1.0;              nb1 = -2.0 * cw;   nb2 = 1.0; break;
        }

        const double a0 = 1.0 + alpha;
        b0 = (float) (nb0 / a0);
        b1 = (float) (nb1 / a0);
        b2 = (float) (nb2 / a0);
        a1 = (float) (-2.0 * cw / a0);
        a2 = (float) ((1.0 - alpha) / a0);
    }
};
} // namespace automech
