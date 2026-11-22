#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "Identity.h"
#include "Parameters.h"
#include "dsp/Filters.h"
#include "dsp/Oversampler.h"
#include "dsp/SaturationEngine.h"
#include "dsp/VisualizerFifo.h"

class ScorchAudioProcessor final : public juce::AudioProcessor
{
public:
    ScorchAudioProcessor();
    ~ScorchAudioProcessor() override = default;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return scorch::identity::pluginName; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    juce::AudioProcessorParameter* getBypassParameter() const override;

    //==============================================================================
    int getNumPrograms() override { return (int) scorch::params::factoryPresets.size(); }
    int getCurrentProgram() override { return currentProgram.load(); }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // A/B comparison (message thread)
    int getABSlot() const noexcept { return abSlot; }
    void selectABSlot (int slot);

    //==============================================================================
    // Data for the GUI
    struct Meters
    {
        std::array<std::atomic<float>, 2> input, output;
        std::atomic<float> drivenInput;   // peak level entering the saturator (before DRIVE)
    };

    Meters& getMeters() noexcept                        { return meters; }
    scorch::VisualizerFifo& getVisualizerFifo() noexcept { return visualizerFifo; }

    juce::AudioProcessorValueTreeState apvts;

private:
    static constexpr int chunkSize = 256;

    void processChunk (juce::AudioBuffer<float>& buffer, int start, int numSamples, int numChannels) noexcept;
    void updateTargets (bool snap) noexcept;
    void applyOversamplingFactor (int index, bool force) noexcept;
    void updateFilters (int numSamples) noexcept;
    static void storePeak (std::atomic<float>& slot, float value) noexcept;

    struct ParameterPointers
    {
        std::atomic<float>* drive = nullptr;
        std::atomic<float>* tone = nullptr;
        std::atomic<float>* heat = nullptr;
        std::atomic<float>* crunch = nullptr;
        std::atomic<float>* bias = nullptr;
        std::atomic<float>* lowCut = nullptr;
        std::atomic<float>* highCut = nullptr;
        std::atomic<float>* mix = nullptr;
        std::atomic<float>* output = nullptr;
        std::atomic<float>* autoGain = nullptr;
        std::atomic<float>* model = nullptr;
        std::atomic<float>* oversampling = nullptr;
        std::atomic<float>* bypass = nullptr;
    } p;

    // DSP
    scorch::Oversampler oversampler { 2 };
    scorch::SaturationEngine engine;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryDelay { 4096 };

    scorch::dsp::Biquad lowCutFilter, preEmphasis, deEmphasis, tilt, highCutFilter;
    scorch::dsp::DcBlocker dcBlocker;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> lowCutSmoothed { 20.0f },
                                                                          highCutSmoothed { 20000.0f },
                                                                          autoGainSmoothed { 1.0f };
    juce::SmoothedValue<float> crunchSmoothed, toneSmoothed, mixSmoothed { 1.0f },
                               outputSmoothed { 1.0f }, bypassSmoothed;

    float tiltGain = 1.0f;
    double inputEnergy = 0.0, wetEnergy = 0.0;
    float autoGainTarget = 1.0f;

    juce::AudioBuffer<float> dryBuffer;
    std::array<float, chunkSize> vizDry {}, vizWet {};

    double sampleRateHz = 44100.0;

    // GUI data
    Meters meters;
    scorch::VisualizerFifo visualizerFifo;

    // Presets / A-B
    std::atomic<int> currentProgram { 0 };
    std::array<juce::ValueTree, 2> abStates;
    int abSlot = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScorchAudioProcessor)
};
