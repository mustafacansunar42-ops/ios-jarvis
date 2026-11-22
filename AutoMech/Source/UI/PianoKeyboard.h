#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace automech
{
// One and a half octaves (C to G). Scale notes glow; the detected note is
// highlighted; clicking a key toggles that pitch class for a custom scale.
class PianoKeyboard : public juce::Component
{
public:
    void setState (uint16_t mask, int detectedPitchClass);
    std::function<void (int pitchClass)> onToggle;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    static constexpr int kNumSemitones = 20;   // C .. G
    static bool isBlack (int semitone) noexcept;
    juce::Rectangle<float> keyBounds (int semitone) const;
    int keyAt (juce::Point<float> p) const;
    bool enabled (int semitone) const noexcept { return (mask >> (unsigned) (semitone % 12)) & 1u; }

    uint16_t mask = 0x0FFF;
    int detected = -1;
};
} // namespace automech
