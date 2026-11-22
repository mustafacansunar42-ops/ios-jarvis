#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <atomic>

namespace scorch
{
/** Horizontal segmented stereo peak meter (L above R) with peak hold.
    Reads peaks the audio thread publishes through atomics; ballistics run on the GUI thread. */
class LevelMeter final : public juce::Component, private juce::Timer
{
public:
    LevelMeter (std::array<std::atomic<float>, 2>& sources, juce::String title);

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    static float dbToProportion (float db) noexcept;

    std::array<std::atomic<float>, 2>& sources;
    juce::String title;

    std::array<float, 2> levelDb { -100.0f, -100.0f };
    std::array<float, 2> holdDb { -100.0f, -100.0f };
    std::array<int, 2> holdFrames {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LevelMeter)
};
} // namespace scorch
