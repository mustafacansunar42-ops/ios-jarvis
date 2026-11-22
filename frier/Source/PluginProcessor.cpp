#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace frier;

namespace
{
constexpr double preEmphasisFreq = 1800.0;
constexpr double tiltPivotFreq   = 900.0;
constexpr double butterworthQ    = 0.70710678;
} // namespace

FrierAudioProcessor::FrierAudioProcessor()
    : AudioProcessor (BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "FRIER", params::createLayout())
{
    using namespace params;
    p.drive        = apvts.getRawParameterValue (id::drive);
    p.tone         = apvts.getRawParameterValue (id::tone);
    p.heat         = apvts.getRawParameterValue (id::heat);
    p.crunch       = apvts.getRawParameterValue (id::crunch);
    p.bias         = apvts.getRawParameterValue (id::bias);
    p.lowCut       = apvts.getRawParameterValue (id::lowCut);
    p.highCut      = apvts.getRawParameterValue (id::highCut);
    p.mix          = apvts.getRawParameterValue (id::mix);
    p.output       = apvts.getRawParameterValue (id::output);
    p.autoGain     = apvts.getRawParameterValue (id::autoGain);
    p.model        = apvts.getRawParameterValue (id::model);
    p.oversampling = apvts.getRawParameterValue (id::oversampling);
    p.bypass       = apvts.getRawParameterValue (id::bypass);

    for (auto& m : meters.input)  m.store (0.0f);
    for (auto& m : meters.output) m.store (0.0f);
    meters.drivenInput.store (0.0f);

    oversampler.setFactorIndex ((int) p.oversampling->load());
    setLatencySamples (oversampler.getLatencySamples());
}

bool FrierAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == out;
}

juce::AudioProcessorParameter* FrierAudioProcessor::getBypassParameter() const
{
    return apvts.getParameter (params::id::bypass);
}

//==============================================================================
void FrierAudioProcessor::prepareToPlay (double sampleRate, int)
{
    sampleRateHz = sampleRate;

    // Everything is processed in fixed chunks of at most chunkSize samples, so
    // the host's block size never matters and nothing is allocated later.
    oversampler.prepare (chunkSize);
    dryBuffer.setSize (2, chunkSize);
    dryDelay.prepare ({ sampleRate, (juce::uint32) chunkSize, 2 });
    dryDelay.reset();

    for (auto* f : { &lowCutFilter, &preEmphasis, &deEmphasis, &tilt, &highCutFilter })
        f->reset();

    dcBlocker.prepare (sampleRate);
    dcBlocker.reset();

    lowCutSmoothed.reset (sampleRate, 0.05);
    highCutSmoothed.reset (sampleRate, 0.05);
    crunchSmoothed.reset (sampleRate, 0.05);
    toneSmoothed.reset (sampleRate, 0.05);
    mixSmoothed.reset (sampleRate, 0.03);
    outputSmoothed.reset (sampleRate, 0.03);
    bypassSmoothed.reset (sampleRate, 0.02);
    autoGainSmoothed.reset (sampleRate, 0.08);

    inputEnergy = wetEnergy = 0.0;
    autoGainTarget = 1.0f;
    autoGainSmoothed.setCurrentAndTargetValue (1.0f);

    applyOversamplingFactor ((int) p.oversampling->load(), true);
    updateTargets (true);
    engine.reset();          // after updateTargets, so no model crossfade runs at start-up
    updateFilters (0);
}

void FrierAudioProcessor::applyOversamplingFactor (int index, bool force) noexcept
{
    if (! force && index == oversampler.getFactorIndex())
        return;

    oversampler.setFactorIndex (index);
    engine.prepare (sampleRateHz * oversampler.getFactor());

    const int latency = oversampler.getLatencySamples();
    dryDelay.setDelay ((float) latency);
    setLatencySamples (latency);
}

void FrierAudioProcessor::updateTargets (bool snap) noexcept
{
    const auto model = static_cast<Model> (juce::jlimit (0, 4, (int) p.model->load()));
    engine.setModel (model);
    engine.setTargets (p.drive->load(), p.heat->load() * 0.01f, p.bias->load() * 0.01f, snap);

    const float lowCut  = p.lowCut->load();
    const float highCut = p.highCut->load();
    const float crunch  = p.crunch->load() * 0.01f;
    const float tone    = p.tone->load();
    const float mix     = p.mix->load() * 0.01f;
    const float outGain = juce::Decibels::decibelsToGain (p.output->load());
    const float bypass  = p.bypass->load() > 0.5f ? 1.0f : 0.0f;

    if (snap)
    {
        lowCutSmoothed.setCurrentAndTargetValue (lowCut);
        highCutSmoothed.setCurrentAndTargetValue (highCut);
        crunchSmoothed.setCurrentAndTargetValue (crunch);
        toneSmoothed.setCurrentAndTargetValue (tone);
        mixSmoothed.setCurrentAndTargetValue (mix);
        outputSmoothed.setCurrentAndTargetValue (outGain);
        bypassSmoothed.setCurrentAndTargetValue (bypass);
    }
    else
    {
        lowCutSmoothed.setTargetValue (lowCut);
        highCutSmoothed.setTargetValue (highCut);
        crunchSmoothed.setTargetValue (crunch);
        toneSmoothed.setTargetValue (tone);
        mixSmoothed.setTargetValue (mix);
        outputSmoothed.setTargetValue (outGain);
        bypassSmoothed.setTargetValue (bypass);
    }
}

void FrierAudioProcessor::updateFilters (int numSamples) noexcept
{
    // Filter coefficients follow their smoothed parameters once per chunk (<= 256 samples).
    const float lowCut  = numSamples > 0 ? lowCutSmoothed.skip (numSamples)  : lowCutSmoothed.getCurrentValue();
    const float highCut = numSamples > 0 ? highCutSmoothed.skip (numSamples) : highCutSmoothed.getCurrentValue();
    const float crunch  = numSamples > 0 ? crunchSmoothed.skip (numSamples)  : crunchSmoothed.getCurrentValue();
    const float tone    = numSamples > 0 ? toneSmoothed.skip (numSamples)    : toneSmoothed.getCurrentValue();

    lowCutFilter.setHighPass (sampleRateHz, lowCut, butterworthQ);
    highCutFilter.setLowPass (sampleRateHz, juce::jmin ((double) highCut, 0.45 * sampleRateHz), butterworthQ);

    // CRUNCH: boost highs before the saturator (they distort more), then take back
    // part of the boost afterwards, which leaves a brighter, grittier result.
    preEmphasis.setHighShelf (sampleRateHz, preEmphasisFreq,  15.0 * crunch);
    deEmphasis.setHighShelf  (sampleRateHz, preEmphasisFreq, -9.0 * crunch);

    // TONE: tilt EQ pivoting around ~900 Hz.
    tilt.setHighShelf (sampleRateHz, tiltPivotFreq, tone);
    tiltGain = juce::Decibels::decibelsToGain (-0.5f * tone);
}

void FrierAudioProcessor::storePeak (std::atomic<float>& slot, float value) noexcept
{
    // The GUI resets the slot with exchange(0), so keep the largest value since then.
    if (value > slot.load (std::memory_order_relaxed))
        slot.store (value, std::memory_order_relaxed);
}

//==============================================================================
void FrierAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalIn  = getTotalNumInputChannels();
    const int totalOut = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, numSamples);

    const int numChannels = juce::jmin (totalIn, totalOut, 2);

    if (numChannels == 0 || numSamples == 0)
        return;

    applyOversamplingFactor ((int) p.oversampling->load(), false);
    updateTargets (false);

    for (int start = 0; start < numSamples; start += chunkSize)
        processChunk (buffer, start, juce::jmin (chunkSize, numSamples - start), numChannels);
}

void FrierAudioProcessor::processChunk (juce::AudioBuffer<float>& buffer, int start, int n, int numChannels) noexcept
{
    std::array<float*, 2> wet {}, dry {};
    for (int ch = 0; ch < numChannels; ++ch)
    {
        wet[(size_t) ch] = buffer.getWritePointer (ch, start);
        dry[(size_t) ch] = dryBuffer.getWritePointer (ch);
    }

    // 1) Dry path: delay-compensated copy of the input; input metering.
    double chunkInputEnergy = 0.0;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        float peak = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            const float x = wet[(size_t) ch][i];
            peak = juce::jmax (peak, std::abs (x));
            chunkInputEnergy += (double) x * x;

            dryDelay.pushSample (ch, x);
            dry[(size_t) ch][i] = dryDelay.popSample (ch);
        }

        storePeak (meters.input[(size_t) ch], peak);
    }

    if (numChannels == 1)
        storePeak (meters.input[1], meters.input[0].load (std::memory_order_relaxed));

    // 2) Pre-saturation filters: low cut, then CRUNCH pre-emphasis.
    updateFilters (n);
    float drivenPeak = 0.0f;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        for (int i = 0; i < n; ++i)
        {
            float x = wet[(size_t) ch][i];
            x = lowCutFilter.process (x, ch);
            x = preEmphasis.process (x, ch);
            drivenPeak = juce::jmax (drivenPeak, std::abs (x));
            wet[(size_t) ch][i] = x;
        }
    }

    storePeak (meters.drivenInput, drivenPeak);

    // 3) Oversampled nonlinear stage.
    {
        juce::dsp::AudioBlock<float> block (buffer);
        auto sub = block.getSubsetChannelBlock (0, (size_t) numChannels)
                        .getSubBlock ((size_t) start, (size_t) n);

        auto upsampled = oversampler.upsample (sub);
        engine.process (upsampled);
        oversampler.downsample (sub);
    }

    // 4) Post filters: de-emphasis, tone tilt, high cut, DC blocker.
    double chunkWetEnergy = 0.0;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        for (int i = 0; i < n; ++i)
        {
            float y = wet[(size_t) ch][i];
            y = deEmphasis.process (y, ch);
            y = tilt.process (y, ch) * tiltGain;
            y = highCutFilter.process (y, ch);
            y = dcBlocker.process (y, ch);
            chunkWetEnergy += (double) y * y;
            wet[(size_t) ch][i] = y;
        }
    }

    // 5) Auto gain: slow RMS follower matching the wet level to the input level.
    if (p.autoGain->load() > 0.5f)
    {
        const double alpha = 1.0 - std::exp (-(double) n / (0.35 * sampleRateHz));
        const double norm = 1.0 / (double) (n * numChannels);
        inputEnergy += alpha * (chunkInputEnergy * norm - inputEnergy);
        wetEnergy   += alpha * (chunkWetEnergy * norm - wetEnergy);

        // Only adapt while there is signal; hold the last gain through silence.
        if (inputEnergy > 1.0e-7 && wetEnergy > 1.0e-10)
            autoGainTarget = (float) juce::jlimit (0.0316, 4.0, std::sqrt (inputEnergy / wetEnergy));
    }
    else
    {
        autoGainTarget = 1.0f;
    }

    autoGainSmoothed.setTargetValue (autoGainTarget);

    // 6) Mix, output gain and bypass crossfade. The dry path is delay-compensated,
    //    so the mix and bypass don't comb-filter.
    std::array<float, 2> outPeak {};
    const float monoScale = 1.0f / (float) numChannels;

    for (int i = 0; i < n; ++i)
    {
        const float ag  = autoGainSmoothed.getNextValue();
        const float mix = mixSmoothed.getNextValue();
        const float out = outputSmoothed.getNextValue();
        const float byp = bypassSmoothed.getNextValue();

        float dryMono = 0.0f, wetMono = 0.0f;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float d = dry[(size_t) ch][i];
            const float w = wet[(size_t) ch][i] * ag;

            float y = out * (d + mix * (w - d));
            y += byp * (d - y);

            wet[(size_t) ch][i] = y;
            outPeak[(size_t) ch] = juce::jmax (outPeak[(size_t) ch], std::abs (y));

            dryMono += d;
            wetMono += y;
        }

        vizDry[(size_t) i] = dryMono * monoScale;
        vizWet[(size_t) i] = wetMono * monoScale;
    }

    storePeak (meters.output[0], outPeak[0]);
    storePeak (meters.output[1], numChannels > 1 ? outPeak[1] : outPeak[0]);

    visualizerFifo.push (vizDry.data(), vizWet.data(), n);
}

//==============================================================================
void FrierAudioProcessor::setCurrentProgram (int index)
{
    const auto& presets = params::factoryPresets;
    index = juce::jlimit (0, (int) presets.size() - 1, index);
    currentProgram.store (index);

    const auto& preset = presets[(size_t) index];

    auto set = [this] (const char* paramId, float value)
    {
        if (auto* param = apvts.getParameter (paramId))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
    };

    using namespace params;
    set (id::model,        (float) preset.model);
    set (id::drive,        preset.drive);
    set (id::tone,         preset.tone);
    set (id::heat,         preset.heat);
    set (id::crunch,       preset.crunch);
    set (id::bias,         preset.bias);
    set (id::lowCut,       preset.lowCut);
    set (id::highCut,      preset.highCut);
    set (id::mix,          preset.mix);
    set (id::output,       preset.output);
    set (id::autoGain,     preset.autoGain ? 1.0f : 0.0f);
    set (id::oversampling, (float) preset.oversampling);
}

const juce::String FrierAudioProcessor::getProgramName (int index)
{
    if (juce::isPositiveAndBelow (index, (int) params::factoryPresets.size()))
        return params::factoryPresets[(size_t) index].name;

    return {};
}

void FrierAudioProcessor::selectABSlot (int slot)
{
    slot = juce::jlimit (0, 1, slot);

    if (slot == abSlot)
        return;

    abStates[(size_t) abSlot] = apvts.copyState();

    // The first time a slot is visited it starts as a copy of the other one.
    if (abStates[(size_t) slot].isValid())
        apvts.replaceState (abStates[(size_t) slot].createCopy());

    abSlot = slot;
}

void FrierAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram.load(), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void FrierAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentProgram.store (juce::jlimit (0, getNumPrograms() - 1, (int) tree.getProperty ("program", 0)));
            apvts.replaceState (tree);
        }
    }
}

//==============================================================================
juce::AudioProcessorEditor* FrierAudioProcessor::createEditor()
{
    return new FrierAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FrierAudioProcessor();
}
