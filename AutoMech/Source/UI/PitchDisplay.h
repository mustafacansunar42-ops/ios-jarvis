#pragma once

#include "PixelVisualizer.h"

namespace automech
{
// The black glass screen: detected note, cents, Hz, deviation needle,
// TRACKING / LOCKED status and the pixel visualizer behind it all.
class PitchDisplay : public juce::Component
{
public:
    PitchDisplay();

    void update (const DisplayState& state);
    PixelVisualizer& getVisualizer() noexcept { return visualizer; }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> pixelToggleArea() const;
    juce::Rectangle<float> modeArea() const;

    PixelVisualizer visualizer;
    bool visualizerOn = true;
    DisplayState state;
    float needleCents = 0.0f;
    juce::String noteName = "-", octave, centsText = "+0 \xc2\xa2", hzText = "0.0 Hz";
};
} // namespace automech
