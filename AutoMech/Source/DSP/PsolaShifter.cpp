#include "PsolaShifter.h"

#include <algorithm>
#include <cmath>

namespace automech
{
void PsolaShifter::prepare (double sampleRate)
{
    sr = sampleRate;
    maxPeriod = (int) std::ceil (sr / 50.0);          // lowest pitch: 50 Hz
    defaultPeriod = (int) std::lround (sr / 200.0);   // grain size for unvoiced audio
    // Grains read up to 1.25 periods ahead of the analysis point.
    latency = (int) std::ceil (maxPeriod * 1.25) + 4;

    size_t size = 1;
    while (size < (size_t) (sr * 0.5) + (size_t) latency * 4)
        size <<= 1;

    ring.assign (size, 0.0f);
    mask = (int64_t) size - 1;
    reset();
}

void PsolaShifter::reset()
{
    std::fill (ring.begin(), ring.end(), 0.0f);
    writeCount = 0;
    lastEpoch = 0;
    period = (float) defaultPeriod;
    voiced = false;
    for (auto& v : voices)
    {
        v.untilNext = 0.0;
        for (auto& g : v.grains) g.active = false;
    }
}

void PsolaShifter::setPeriod (float p, bool isVoiced) noexcept
{
    voiced = isVoiced;
    period = isVoiced ? std::clamp (p, 16.0f, (float) maxPeriod) : (float) defaultPeriod;
}

void PsolaShifter::pushSample (float x) noexcept
{
    ring[(size_t) (writeCount & mask)] = x;
    ++writeCount;
    advanceEpochs();
}

void PsolaShifter::advanceEpochs() noexcept
{
    const int64_t analysis = writeCount - 1 - latency;
    const int p = std::max (16, (int) std::lround (period));

    if (analysis - lastEpoch > 4 * (int64_t) maxPeriod || lastEpoch > analysis + p)
        lastEpoch = analysis - p;

    while (analysis - lastEpoch >= p)
    {
        const int64_t candidate = lastEpoch + p;
        int64_t best = candidate;

        if (voiced)
        {
            // Snap the epoch to the strongest peak near the expected position.
            const int search = p / 4;
            float bestVal = -1.0e9f;
            for (int64_t i = candidate - search; i <= candidate + search; ++i)
            {
                const float v = sampleAt (i);
                if (v > bestVal) { bestVal = v; best = i; }
            }
        }

        lastEpoch = std::max (best, lastEpoch + p / 2);
    }
}

float PsolaShifter::readInterp (double pos) const noexcept
{
    const auto i = (int64_t) std::floor (pos);
    const auto frac = (float) (pos - (double) i);
    const float a = sampleAt (i), b = sampleAt (i + 1);
    return a + frac * (b - a);
}

float PsolaShifter::render (int voiceIndex, float ratio, float formantFactor) noexcept
{
    constexpr double twoPi = 6.28318530717958647692;
    auto& v = voices[(size_t) voiceIndex];

    v.untilNext -= 1.0;
    if (v.untilNext <= 0.0)
    {
        const float r = voiced ? std::clamp (ratio, 0.25f, 4.0f) : 1.0f;
        const float f = std::clamp (formantFactor, 0.25f, 4.0f);
        const float p = period;

        for (auto& g : v.grains)
        {
            if (g.active) continue;
            g.active = true;
            g.start = (double) lastEpoch - p;
            g.rate = f;
            g.length = std::clamp ((int) std::lround (2.0f * p / f), 8, 8 * maxPeriod);
            g.index = 0;
            // Hann windows of length L at hop H sum to L / (2H); compensate.
            g.gain = std::clamp (f / r, 0.2f, 2.0f);
            break;
        }

        v.untilNext += std::max (1.0, (double) p / r);
    }

    float out = 0.0f;
    for (auto& g : v.grains)
    {
        if (! g.active) continue;

        const double phase = ((double) g.index + 0.5) / g.length;
        const auto w = (float) (0.5 - 0.5 * std::cos (twoPi * phase));
        out += readInterp (g.start + (double) g.index * g.rate) * w * g.gain;

        if (++g.index >= g.length)
            g.active = false;
    }
    return out;
}
} // namespace automech
