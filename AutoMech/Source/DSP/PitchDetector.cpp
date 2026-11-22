#include "PitchDetector.h"

#include <algorithm>
#include <cmath>

namespace automech
{
void PitchDetector::prepare (double sampleRate)
{
    decimation = std::max (1, (int) std::lround (sampleRate / 12000.0));
    rate = sampleRate / decimation;

    // 4th-order Butterworth anti-alias filter ahead of the decimator.
    aa1.setLowpass (sampleRate, rate * 0.42, 0.5412);
    aa2.setLowpass (sampleRate, rate * 0.42, 1.3066);

    maxTauCap = (int) std::ceil (rate / 35.0) + 2;
    ringSize = kWindow + maxTauCap + 8;
    ring.assign ((size_t) ringSize, 0.0f);
    frame.assign ((size_t) ringSize, 0.0f);
    diff.assign ((size_t) maxTauCap + 2, 0.0f);
    cmnd.assign ((size_t) maxTauCap + 2, 1.0f);

    setRange (80.0f, 700.0f);
    reset();
}

void PitchDetector::reset()
{
    aa1.reset();
    aa2.reset();
    std::fill (ring.begin(), ring.end(), 0.0f);
    writeIdx = hopCounter = decimCounter = 0;
    frequency = confidence = 0.0f;
    voiced = false;
}

void PitchDetector::setRange (float minHz, float maxHz) noexcept
{
    minTau = std::max (2, (int) std::floor (rate / maxHz));
    maxTau = std::min (maxTauCap - 1, (int) std::ceil (rate / minHz));
}

void PitchDetector::setSensitivity (float s) noexcept
{
    s = std::clamp (s, 0.0f, 1.0f);
    threshold = 0.08f + 0.32f * s;
    gate = std::pow (10.0f, (-38.0f - 30.0f * s) / 20.0f);
}

void PitchDetector::push (float x) noexcept
{
    x = aa2.process (aa1.process (x));

    if (++decimCounter < decimation)
        return;
    decimCounter = 0;

    ring[(size_t) writeIdx] = x;
    writeIdx = (writeIdx + 1) % ringSize;

    if (++hopCounter >= kHop)
    {
        hopCounter = 0;
        analyse();
    }
}

void PitchDetector::analyse() noexcept
{
    const int n = kWindow + maxTau;
    int idx = (writeIdx - n + ringSize) % ringSize;
    double energy = 0.0;

    for (int i = 0; i < n; ++i)
    {
        const float v = ring[(size_t) idx];
        frame[(size_t) i] = v;
        energy += (double) v * v;
        if (++idx == ringSize) idx = 0;
    }

    if (std::sqrt (energy / n) < gate)
    {
        voiced = false;
        confidence = 0.0f;
        return;
    }

    // Difference function + cumulative mean normalisation.
    double running = 0.0;
    cmnd[0] = 1.0f;
    for (int tau = 1; tau <= maxTau; ++tau)
    {
        double sum = 0.0;
        const float* a = frame.data();
        const float* b = frame.data() + tau;
        for (int j = 0; j < kWindow; ++j)
        {
            const float d = a[j] - b[j];
            sum += (double) d * d;
        }
        diff[(size_t) tau] = (float) sum;
        running += sum;
        cmnd[(size_t) tau] = running > 0.0 ? (float) (sum * tau / running) : 1.0f;
    }

    int tau = -1;
    for (int t = minTau; t < maxTau; ++t)
    {
        if (cmnd[(size_t) t] < threshold)
        {
            while (t + 1 < maxTau && cmnd[(size_t) t + 1] < cmnd[(size_t) t])
                ++t;
            tau = t;
            break;
        }
    }

    if (tau < 0)
    {
        voiced = false;
        confidence = 0.0f;
        return;
    }

    // Parabolic interpolation for sub-sample accuracy.
    float shift = 0.0f;
    if (tau > 1 && tau < maxTau)
    {
        const float s0 = cmnd[(size_t) tau - 1], s1 = cmnd[(size_t) tau], s2 = cmnd[(size_t) tau + 1];
        const float denom = s0 + s2 - 2.0f * s1;
        if (std::fabs (denom) > 1.0e-9f)
            shift = std::clamp (0.5f * (s0 - s2) / denom, -0.5f, 0.5f);
    }

    const float f = (float) (rate / (tau + shift));
    confidence = std::clamp (1.0f - cmnd[(size_t) tau], 0.0f, 1.0f);

    // Median-of-3 to reject single-frame octave jumps.
    if (! voiced)
        history[0] = history[1] = history[2] = f;
    history[0] = history[1];
    history[1] = history[2];
    history[2] = f;

    const float a = history[0], b = history[1], c = history[2];
    frequency = std::max (std::min (a, b), std::min (std::max (a, b), c));
    voiced = true;
}
} // namespace automech
