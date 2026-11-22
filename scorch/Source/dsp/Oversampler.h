#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <memory>

namespace scorch
{
/** Holds one pre-built juce::dsp::Oversampling instance per factor (1x/2x/4x/8x),
    so switching factor on the audio thread never allocates.
    Uses linear-phase FIR half-band filters with integer latency, so the dry path
    can be delay-compensated exactly. */
class Oversampler
{
public:
    static constexpr int numFactors = 4;

    /** Builds the filter stages. Allocates: call from the constructor. */
    explicit Oversampler (int numChannels);

    /** Allocates the processing buffers. Call from prepareToPlay. */
    void prepare (int maxBlockSize);
    void reset() noexcept;

    /** 0 = 1x, 1 = 2x, 2 = 4x, 3 = 8x. Real-time safe. */
    void setFactorIndex (int index) noexcept;
    int  getFactorIndex() const noexcept { return factorIndex; }
    int  getFactor() const noexcept      { return 1 << factorIndex; }

    /** Latency of the active stage, in base-rate samples. */
    int getLatencySamples() const noexcept;

    juce::dsp::AudioBlock<float> upsample (const juce::dsp::AudioBlock<float>& block) noexcept;
    void downsample (juce::dsp::AudioBlock<float>& block) noexcept;

private:
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, numFactors> stages;
    int factorIndex = 2;
};
} // namespace scorch
