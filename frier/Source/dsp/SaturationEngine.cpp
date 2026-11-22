#include "SaturationEngine.h"

namespace frier
{
namespace
{
float onePoleCoeff (double fs, double hz) noexcept
{
    return (float) (1.0 - std::exp (-juce::MathConstants<double>::twoPi * hz / fs));
}

float timeCoeff (double fs, double seconds) noexcept
{
    return (float) (1.0 - std::exp (-1.0 / (seconds * fs)));
}
} // namespace

void SaturationEngine::prepare (double oversampledRate) noexcept
{
    rate = oversampledRate;

    driveGain.reset (rate, 0.03);
    heat.reset (rate, 0.05);
    bias.reset (rate, 0.05);

    crossfadeLength = juce::jmax (1, (int) (rate * 0.012));
    crossfadeRemaining = juce::jmin (crossfadeRemaining, crossfadeLength);

    envAttack   = timeCoeff (rate, 0.0005);
    envRelease  = timeCoeff (rate, 0.030);
    gateAttack  = timeCoeff (rate, 0.002);
    gateRelease = timeCoeff (rate, 0.120);
}

void SaturationEngine::reset() noexcept
{
    states.fill ({});
    crossfadeRemaining = 0;
    previous = current;
}

void SaturationEngine::setModel (Model newModel) noexcept
{
    if (newModel == current)
        return;

    previous = current;
    current = newModel;
    crossfadeRemaining = crossfadeLength;
}

void SaturationEngine::setTargets (float driveDb, float heat01, float biasBipolar, bool snap) noexcept
{
    const float g = juce::Decibels::decibelsToGain (driveDb);

    if (snap)
    {
        driveGain.setCurrentAndTargetValue (g);
        heat.setCurrentAndTargetValue (heat01);
        bias.setCurrentAndTargetValue (biasBipolar);
    }
    else
    {
        driveGain.setTargetValue (g);
        heat.setTargetValue (heat01);
        bias.setTargetValue (biasBipolar);
    }
}

//==============================================================================
float SaturationEngine::shape (Model model, float x, float h, float b) noexcept
{
    x += b * 0.5f;

    switch (model)
    {
        case Model::Tube:
        {
            // Asymmetric soft clip: the positive half compresses earlier than the
            // negative half, which creates even harmonics. HEAT pushes both the
            // asymmetry and an explicit 2nd-harmonic term.
            const float kPos = 1.0f + 0.8f * h;
            constexpr float kNeg = 0.7f;
            float y = x >= 0.0f ? std::tanh (x * kPos) / kPos
                                : std::tanh (x * kNeg) / kNeg;
            y += 0.18f * h * y * y;
            return y;
        }

        case Model::Tape:
        {
            // Smooth algebraic saturation with a gentle knee.
            const float u = x * (1.0f + 0.5f * h);
            return u / std::sqrt (1.0f + u * u);
        }

        case Model::Transistor:
        {
            // Symmetric clipper (odd harmonics) whose knee gets harder with HEAT.
            const float a = std::abs (x);
            if (a > 40.0f)
                return x > 0.0f ? 1.0f : -1.0f;

            const float p = 2.0f + 8.0f * h;
            return x / std::pow (1.0f + std::pow (a, p), 1.0f / p);
        }

        case Model::Fuzz:
        {
            // Partial rectification (octave-up content) into a heavy asymmetric clip.
            const float u = x * (1.0f + 2.0f * h);
            const float r = 0.15f + 0.35f * h;   // 0.5 = full half-wave rectification
            const float v = (1.0f - r) * u + r * std::abs (u);
            return v >= 0.0f ? std::tanh (2.0f * v)
                             : 0.45f * std::tanh (2.0f * v / 0.45f);
        }

        case Model::Fry:
        {
            // Triangle wavefolder followed by bit reduction (16 bits down to 4).
            const float u = x * (1.0f + 3.0f * h);
            const float t = (u + 1.0f) * 0.25f;
            const float folded = 1.0f - 4.0f * std::abs (t - std::floor (t) - 0.5f);
            const float levels = std::exp2 (15.0f - 12.0f * h);
            return std::round (folded * levels) / levels;
        }
    }

    return x;
}

float SaturationEngine::processSample (Model model, float x, ChannelState& s, float h, float b) noexcept
{
    switch (model)
    {
        case Model::Tape:
        {
            // Hysteresis-style memory: the input is pulled toward the previous
            // output, so rising and falling signals follow different paths.
            const float k = 0.05f + 0.2f * h;
            const float y = shape (Model::Tape, x + k * (s.tapeMemory - x), h, b);
            s.tapeMemory = y;

            // Gentle high-frequency roll-off, as on tape playback.
            s.tapeLowpass += tapeLowpassCoeff * (y - s.tapeLowpass);
            return s.tapeLowpass;
        }

        case Model::Fuzz:
        {
            const float a = std::abs (x);
            s.fuzzEnvelope += (a > s.fuzzEnvelope ? envAttack : envRelease) * (a - s.fuzzEnvelope);

            // Gate-like decay: as the input fades, the fuzz sputters and closes.
            const float threshold = 0.02f + 0.06f * h;
            const float ratio = s.fuzzEnvelope / threshold;
            const float target = ratio >= 1.0f ? 1.0f : ratio * ratio * ratio;
            s.fuzzGate += (target > s.fuzzGate ? gateAttack : gateRelease) * (target - s.fuzzGate);

            return shape (Model::Fuzz, x, h, b) * s.fuzzGate;
        }

        case Model::Tube:
        case Model::Transistor:
        case Model::Fry:
            break;
    }

    return shape (model, x, h, b);
}

void SaturationEngine::process (juce::dsp::AudioBlock<float>& block) noexcept
{
    const auto numChannels = juce::jmin ((int) block.getNumChannels(), maxChannels);
    const auto numSamples  = (int) block.getNumSamples();

    const double cutoff = juce::jmin (18000.0 - 8000.0 * (double) heat.getTargetValue(), 0.45 * rate);
    tapeLowpassCoeff = onePoleCoeff (rate, cutoff);

    std::array<float*, maxChannels> data {};
    for (int ch = 0; ch < numChannels; ++ch)
        data[(size_t) ch] = block.getChannelPointer ((size_t) ch);

    for (int i = 0; i < numSamples; ++i)
    {
        const float g = driveGain.getNextValue();
        const float h = heat.getNextValue();
        const float b = bias.getNextValue();

        const bool fading = crossfadeRemaining > 0;
        float fade = 1.0f;

        if (fading)
        {
            fade = 1.0f - (float) crossfadeRemaining / (float) crossfadeLength;
            --crossfadeRemaining;
        }

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto& state = states[(size_t) ch];
            const float x = data[(size_t) ch][i] * g;
            float y = processSample (current, x, state, h, b);

            if (fading)
                y = fade * y + (1.0f - fade) * processSample (previous, x, state, h, b);

            data[(size_t) ch][i] = y;
        }
    }
}
} // namespace frier
