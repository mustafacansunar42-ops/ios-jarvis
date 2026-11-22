#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace frier
{
/** Rotary control bound directly to an APVTS parameter.

    - vertical drag (relative, so it never jumps)
    - Shift + drag / Shift + wheel for fine adjustment
    - mouse wheel
    - double-click resets to the default
    - label above, live value below (highlighted while hovered or dragged) */
class Knob final : public juce::Component
{
public:
    enum class Size { Large, Small };

    Knob (juce::RangedAudioParameter& parameter, juce::String label, Size size);

    /** Preferred height for the given size: label + knob + value. */
    static int preferredHeight (Size size) noexcept;
    static int knobDiameter (Size size) noexcept;

    void paint (juce::Graphics&) override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }

private:
    juce::Rectangle<float> knobArea() const;
    void setNormalised (float normalised, bool asGesture);

    juce::RangedAudioParameter& param;
    juce::ParameterAttachment attachment;
    juce::String label;
    Size size;
    bool bipolar = false;

    float normalisedValue = 0.0f;
    float dragNormalised = 0.0f;
    float lastDragY = 0.0f;
    bool dragging = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Knob)
};
} // namespace frier
