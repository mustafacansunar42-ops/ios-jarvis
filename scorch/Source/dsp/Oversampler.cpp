#include "Oversampler.h"

namespace scorch
{
Oversampler::Oversampler (int numChannels)
{
    using OS = juce::dsp::Oversampling<float>;

    for (size_t i = 0; i < stages.size(); ++i)
        stages[i] = std::make_unique<OS> ((size_t) numChannels,
                                          i,                              // log2 of the factor
                                          OS::filterHalfBandFIREquiripple,
                                          true,                           // max quality
                                          true);                          // integer latency
}

void Oversampler::prepare (int maxBlockSize)
{
    for (auto& stage : stages)
    {
        stage->initProcessing ((size_t) maxBlockSize);
        stage->reset();
    }
}

void Oversampler::reset() noexcept
{
    for (auto& stage : stages)
        stage->reset();
}

void Oversampler::setFactorIndex (int index) noexcept
{
    index = juce::jlimit (0, numFactors - 1, index);

    if (index != factorIndex)
    {
        factorIndex = index;
        stages[(size_t) factorIndex]->reset();
    }
}

int Oversampler::getLatencySamples() const noexcept
{
    return juce::roundToInt (stages[(size_t) factorIndex]->getLatencyInSamples());
}

juce::dsp::AudioBlock<float> Oversampler::upsample (const juce::dsp::AudioBlock<float>& block) noexcept
{
    return stages[(size_t) factorIndex]->processSamplesUp (juce::dsp::AudioBlock<const float> (block));
}

void Oversampler::downsample (juce::dsp::AudioBlock<float>& block) noexcept
{
    stages[(size_t) factorIndex]->processSamplesDown (block);
}
} // namespace scorch
