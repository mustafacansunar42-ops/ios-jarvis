#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace automech
{
// Pitch-synchronous overlap-add (TD-PSOLA style) shifter.
//
// One shared input history with pitch epochs (one per detected period,
// snapped to the waveform peak). Each output voice spawns Hann-windowed
// two-period grains taken from the most recent epoch, spaced by
// period / ratio. The grain is read at `formantFactor` speed, so a factor
// of 1 keeps the spectral envelope (formants) in place while the pitch moves.
//
// Several voices share the same input, which is how the harmony voices work.
class PsolaShifter
{
public:
    static constexpr int kMaxVoices = 4;

    void prepare (double sampleRate);
    void reset();

    int getLatencySamples() const noexcept { return latency; }

    // Current period in samples (only used while voiced).
    void setPeriod (float periodSamples, bool isVoiced) noexcept;

    // Call exactly once per sample, before render().
    void pushSample (float x) noexcept;

    // Render one output sample for a voice.
    float render (int voice, float ratio, float formantFactor) noexcept;

private:
    struct Grain
    {
        bool active = false;
        double start = 0.0;
        float rate = 1.0f, gain = 1.0f;
        int length = 0, index = 0;
    };

    struct Voice
    {
        std::array<Grain, 16> grains {};
        double untilNext = 0.0;
    };

    float sampleAt (int64_t pos) const noexcept { return ring[(size_t) (pos & mask)]; }
    float readInterp (double pos) const noexcept;
    void advanceEpochs() noexcept;

    double sr = 48000.0;
    std::vector<float> ring;
    int64_t mask = 0, writeCount = 0, lastEpoch = 0;
    int latency = 0, maxPeriod = 0, defaultPeriod = 0;
    float period = 240.0f;
    bool voiced = false;
    std::array<Voice, kMaxVoices> voices {};
};
} // namespace automech
