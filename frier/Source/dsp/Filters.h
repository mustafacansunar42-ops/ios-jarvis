#pragma once

#include <array>
#include <cmath>
#include <algorithm>

namespace frier::dsp
{
/** Transposed direct-form II biquad with RBJ cookbook designs.
    Holds state for up to two channels. Coefficient changes are allocation-free. */
class Biquad
{
public:
    void reset() noexcept
    {
        s1.fill (0.0f);
        s2.fill (0.0f);
    }

    inline float process (float x, int channel) noexcept
    {
        const auto c = (size_t) channel;
        const float y = b0 * x + s1[c];
        s1[c] = b1 * x - a1 * y + s2[c];
        s2[c] = b2 * x - a2 * y;
        return y;
    }

    void setLowPass (double fs, double freq, double q) noexcept
    {
        const auto [cosw, alpha] = prewarp (fs, freq, q);
        set ((1.0 - cosw) * 0.5, 1.0 - cosw, (1.0 - cosw) * 0.5,
             1.0 + alpha, -2.0 * cosw, 1.0 - alpha);
    }

    void setHighPass (double fs, double freq, double q) noexcept
    {
        const auto [cosw, alpha] = prewarp (fs, freq, q);
        set ((1.0 + cosw) * 0.5, -(1.0 + cosw), (1.0 + cosw) * 0.5,
             1.0 + alpha, -2.0 * cosw, 1.0 - alpha);
    }

    /** Shelf slope S = 1. */
    void setHighShelf (double fs, double freq, double gainDb) noexcept
    {
        const double A     = std::pow (10.0, gainDb / 40.0);
        const double w0    = 2.0 * pi * clampFreq (fs, freq) / fs;
        const double cosw  = std::cos (w0);
        const double alpha = std::sin (w0) * 0.5 * std::sqrt (2.0);
        const double sqA2a = 2.0 * std::sqrt (A) * alpha;

        set (A * ((A + 1.0) + (A - 1.0) * cosw + sqA2a),
             -2.0 * A * ((A - 1.0) + (A + 1.0) * cosw),
             A * ((A + 1.0) + (A - 1.0) * cosw - sqA2a),
             (A + 1.0) - (A - 1.0) * cosw + sqA2a,
             2.0 * ((A - 1.0) - (A + 1.0) * cosw),
             (A + 1.0) - (A - 1.0) * cosw - sqA2a);
    }

private:
    static constexpr double pi = 3.14159265358979323846;

    static double clampFreq (double fs, double f) noexcept { return std::clamp (f, 5.0, 0.49 * fs); }

    static std::pair<double, double> prewarp (double fs, double freq, double q) noexcept
    {
        const double w0 = 2.0 * pi * clampFreq (fs, freq) / fs;
        return { std::cos (w0), std::sin (w0) / (2.0 * q) };
    }

    void set (double nb0, double nb1, double nb2, double na0, double na1, double na2) noexcept
    {
        const double inv = 1.0 / na0;
        b0 = (float) (nb0 * inv);
        b1 = (float) (nb1 * inv);
        b2 = (float) (nb2 * inv);
        a1 = (float) (na1 * inv);
        a2 = (float) (na2 * inv);
    }

    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    std::array<float, 2> s1 {}, s2 {};
};

/** First-order DC blocker (~8 Hz), removes the offset introduced by BIAS and asymmetric models. */
class DcBlocker
{
public:
    void prepare (double fs) noexcept { r = (float) std::exp (-2.0 * 3.14159265358979323846 * 8.0 / fs); }
    void reset() noexcept { x1.fill (0.0f); y1.fill (0.0f); }

    inline float process (float x, int channel) noexcept
    {
        const auto c = (size_t) channel;
        const float y = x - x1[c] + r * y1[c];
        x1[c] = x;
        y1[c] = y;
        return y;
    }

private:
    float r = 0.999f;
    std::array<float, 2> x1 {}, y1 {};
};
} // namespace frier::dsp
