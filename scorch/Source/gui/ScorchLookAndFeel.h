#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace scorch
{
class ScorchLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    ScorchLookAndFeel();

    /** Font helper; tracking widens the letter spacing. */
    static juce::Font font (float height, float tracking = 0.04f, bool bold = false);

    /** Glossy black cap, thin dark-orange ring with a bright value arc, short orange pointer. */
    static void drawKnob (juce::Graphics&, juce::Rectangle<float> bounds, float proportion,
                          bool bipolar, bool highlighted);

    /** Pill-shaped toggle (bypass, auto gain). */
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;

    /** Dark rounded field with light centred text and a small arrow. */
    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    juce::Font getPopupMenuFont() override;
};
} // namespace scorch
