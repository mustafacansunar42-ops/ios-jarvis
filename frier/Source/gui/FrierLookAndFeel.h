#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace frier
{
class FrierLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    FrierLookAndFeel();

    /** Wide-tracked caps font used throughout (matches the reference's spaced lettering). */
    static juce::Font font (float height, float tracking = 0.12f, bool bold = false);

    /** Draws a knob in the reference style: dark domed body, thin light rim, slim pointer. */
    static void drawKnob (juce::Graphics&, juce::Rectangle<float> bounds, float proportion,
                          bool bipolar, bool highlighted);

    /** The small square indicator used next to section titles and on toggles. */
    static void drawLed (juce::Graphics&, juce::Rectangle<float> area, bool on, juce::Colour onColour);

    // ComboBox properties understood by drawComboBox
    static constexpr const char* flatComboProperty = "frierFlat";

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    juce::Font getPopupMenuFont() override;
};
} // namespace frier
