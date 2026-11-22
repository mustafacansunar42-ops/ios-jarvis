#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace scorch
{
/** Rotary control bound directly to an APVTS parameter.

    - vertical drag (relative, never jumps); Shift for fine adjustment
    - mouse wheel (Shift for fine)
    - double-click resets to the default
    - small lowercase label under the knob
    - the value pops up in a small tooltip bubble above the knob while dragging or hovering */
class Knob final : public juce::Component
{
public:
    Knob (juce::RangedAudioParameter& parameter, juce::String label);

    static constexpr int knobSize = 64;      // ring + cap
    static constexpr int bubbleHeight = 20;
    static constexpr int labelHeight = 16;
    static constexpr int preferredWidth = 104;
    static constexpr int preferredHeight = bubbleHeight + 4 + knobSize + 2 + labelHeight;

    void paint (juce::Graphics&) override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }
    bool hitTest (int x, int y) override;

private:
    juce::Rectangle<float> knobArea() const;
    void setNormalised (float normalised, bool asGesture);

    juce::RangedAudioParameter& param;
    juce::ParameterAttachment attachment;
    juce::String label;
    bool bipolar = false;

    float normalisedValue = 0.0f;
    float dragNormalised = 0.0f;
    float lastDragY = 0.0f;
    bool dragging = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Knob)
};
} // namespace scorch
