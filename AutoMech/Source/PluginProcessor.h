#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>

#include "DSP/Biquad.h"
#include "DSP/PitchDetector.h"
#include "DSP/PsolaShifter.h"
#include "DSP/ScaleQuantizer.h"
#include "Parameters.h"

#include <array>
#include <atomic>

class AutoMechProcessor : public juce::AudioProcessor
{
public:
    AutoMechProcessor();
    ~AutoMechProcessor() override = default;

    //============================================================ AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "AutoMech"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override;

    //============================================================ Parameters
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    // Absolute pitch-class mask for the current key / scale / custom notes.
    uint16_t currentScaleMask() const noexcept;
    // Set a parameter from the message thread (host gets notified).
    void setParam (const juce::String& id, float plainValue);

    //============================================================ GUI telemetry (lock-free)
    struct Telemetry
    {
        std::atomic<float> detectedHz { 0.0f };
        std::atomic<float> detectedMidi { 0.0f };
        std::atomic<float> outputMidi { 0.0f };
        std::atomic<float> targetNote { -1.0f };
        std::atomic<float> confidence { 0.0f };
        std::atomic<bool> voiced { false };
        std::atomic<bool> locked { false };
        std::atomic<int> heldMidiNote { -1 };
    };
    Telemetry telemetry;
    double getCurrentSampleRate() const noexcept { return currentSampleRate; }

    // Output samples for the pixel visualizer (single reader: the GUI thread).
    int readVisualiserSamples (float* dest, int maxSamples);

    //============================================================ Learn (auto key detect)
    void startLearn();
    bool isLearning() const noexcept { return learning.load(); }
    bool popLearnResult (int& key, int& scale);

    //============================================================ Presets / A-B
    static const juce::StringArray& factoryPresetNames();
    void applyFactoryPreset (int index);
    bool saveUserPreset (const juce::File& file);
    bool loadUserPreset (const juce::File& file);
    void toggleAB();
    int getABSlot() const noexcept { return abSlot; }

private:
    void updateControl() noexcept;
    void updateFilters() noexcept;
    void handleMidi (const juce::MidiBuffer& midi) noexcept;
    void pushVisualiser (const float* data, int n) noexcept;
    void finishLearn() noexcept;
    float param (const char* id) const noexcept;

    // Cached raw parameter pointers (read on the audio thread).
    struct Raw
    {
        std::atomic<float>* key; std::atomic<float>* scale; std::atomic<float>* reference;
        std::atomic<float>* correction; std::atomic<float>* speed; std::atomic<float>* noteTrans;
        std::atomic<float>* flex; std::atomic<float>* human; std::atomic<float>* formant;
        std::atomic<float>* vibrato; std::atomic<float>* mix; std::atomic<float>* bypass;
        std::atomic<float>* cleanOn; std::atomic<float>* cleanup; std::atomic<float>* lowCut;
        std::atomic<float>* hum; std::atomic<float>* tighten; std::atomic<float>* onset;
        std::atomic<float>* sensitivity; std::atomic<float>* voiceType; std::atomic<float>* formantPreserve;
        std::atomic<float>* vibRate; std::atomic<float>* vibDelay; std::atomic<float>* vibShape;
        std::atomic<float>* mechMode; std::atomic<float>* mechFreq; std::atomic<float>* mechMix;
        std::atomic<float>* drive; std::atomic<float>* width; std::atomic<float>* midiFollow;
        std::array<std::atomic<float>*, 3> harmInterval, harmLevel, harmPan;
        std::array<std::atomic<float>*, 12> noteEnabled;
    } raw {};

    // DSP
    automech::PitchDetector detector;
    automech::PsolaShifter shifter;
    automech::Biquad lowCutFilter;
    std::array<automech::Biquad, 3> humFilters;
    double currentSampleRate = 48000.0;
    int latency = 0;

    std::array<std::vector<float>, 2> dryRing;
    int dryMask = 0, dryWrite = 0;
    std::vector<float> combBuffer;
    int combWrite = 0;
    double ringPhase = 0.0;

    float lastLowCut = -1.0f;
    int lastHum = -1, lastVoiceType = -1;
    float gateEnv = 0.0f, gateGain = 1.0f;

    // Control-rate state
    static constexpr int kControlInterval = 32;
    int controlCounter = 0;
    uint16_t blockMask = 0x0FFF;
    bool wasVoiced = false;
    int unvoicedTicks = 0;
    float detMidi = 57.0f, detSmooth = 57.0f;
    float currentNote = -1.0f, glide = 0.0f, glideFrom = 0.0f, glideProgress = 1.0f;
    float noteTimeMs = 0.0f, vibPhase = 0.0f;
    float smoothedOffset = 0.0f, outMidi = 57.0f;
    float mainRatio = 1.0f;
    std::array<float, 3> harmRatio { 1.0f, 1.0f, 1.0f };
    std::array<bool, 3> harmActive { false, false, false };

    juce::SmoothedValue<float> mixSmooth, bypassSmooth, formantSmooth, driveSmooth, widthSmooth, mechMixSmooth;
    std::array<juce::SmoothedValue<float>, 3> harmGain;

    // MIDI
    std::array<int, 16> heldNotes {};
    int numHeld = 0;

    // Learn
    std::atomic<bool> learning { false }, learnReady { false };
    std::atomic<int> learnKey { 0 }, learnScale { 0 };
    std::array<float, 12> learnHistogram {};
    int learnSamplesLeft = 0;

    // Visualiser FIFO
    juce::AbstractFifo visFifo { 1 << 15 };
    std::vector<float> visBuffer;

    // Programs / A-B
    int currentProgram = 0;
    int abSlot = 0;
    std::array<juce::ValueTree, 2> abStates;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoMechProcessor)
};
