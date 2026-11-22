#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>

namespace frier
{
enum class Model : int
{
    Tube = 0,
    Tape,
    Transistor,
    Fuzz,
    Fry
};

/** The nonlinear stage. Runs at the oversampled rate.

    Every model is built around a stateless waveshaper (shape()), which the GUI
    also uses to draw the transfer curve. TAPE and FUZZ add per-channel state
    on top of it (hysteresis-style memory, HF roll-off, gate). */
class SaturationEngine
{
public:
    static constexpr int maxChannels = 2;

    /** Non-allocating: safe to call from the audio thread when the oversampling factor changes. */
    void prepare (double oversampledRate) noexcept;
    void reset() noexcept;

    void setModel (Model newModel) noexcept;

    /** driveDb 0..48, heat 0..1, bias -1..1. snap = jump without smoothing. */
    void setTargets (float driveDb, float heat01, float biasBipolar, bool snap) noexcept;

    /** Processes an oversampled block in place. */
    void process (juce::dsp::AudioBlock<float>& block) noexcept;

    /** Stateless transfer function, shared with the visualizer. x is the driven input. */
    static float shape (Model model, float x, float heat, float bias) noexcept;

private:
    struct ChannelState
    {
        float tapeMemory = 0.0f;
        float tapeLowpass = 0.0f;
        float fuzzEnvelope = 0.0f;
        float fuzzGate = 0.0f;
    };

    float processSample (Model model, float x, ChannelState& state, float heat, float bias) noexcept;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> driveGain { 1.0f };
    juce::SmoothedValue<float> heat { 0.4f }, bias { 0.0f };

    std::array<ChannelState, maxChannels> states {};

    Model current = Model::Tube, previous = Model::Tube;
    int crossfadeRemaining = 0, crossfadeLength = 1;

    double rate = 176400.0;
    float tapeLowpassCoeff = 1.0f;
    float envAttack = 1.0f, envRelease = 1.0f, gateAttack = 1.0f, gateRelease = 1.0f;
};
} // namespace frier
