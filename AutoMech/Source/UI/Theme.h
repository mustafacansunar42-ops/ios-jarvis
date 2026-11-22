#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace automech::theme
{
inline const juce::Colour bgTop        { 0xffC4171D };
inline const juce::Colour bgBottom     { 0xffA50F15 };
inline const juce::Colour panel        { 0xff8F0C12 };
inline const juce::Colour panelEdge    { 0xffD8434A };
inline const juce::Colour knobBody     { 0xffD4252B };
inline const juce::Colour knobHighlight{ 0xffF4686D };
inline const juce::Colour knobRim      { 0xff7A0A0F };
inline const juce::Colour text         { 0xffFFFFFF };
inline const juce::Colour label        { 0xffFFD9D9 };
inline const juce::Colour display      { 0xff0B0506 };
inline const juce::Colour whiteKey     { 0xffF6F1EE };
inline const juce::Colour blackKey     { 0xff6A0A10 };
inline const juce::Colour control      { 0xff8A0B11 };
inline const juce::Colour controlDark  { 0xff6C070C };
inline const juce::Colour accentRed    { 0xffB3121A };

inline juce::Font font (float height, bool bold = false, float kerning = 0.0f)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain))
        .withExtraKerningFactor (kerning);
}

// Dark-red recessed panel with a soft inner shadow and a thin lighter edge.
inline void drawRecessedPanel (juce::Graphics& g, juce::Rectangle<float> r, float radius = 10.0f)
{
    g.setColour (panel);
    g.fillRoundedRectangle (r, radius);

    juce::Graphics::ScopedSaveState s (g);
    juce::Path clip;
    clip.addRoundedRectangle (r, radius);
    g.reduceClipRegion (clip);

    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.35f), r.getX(), r.getY(),
                                             juce::Colours::transparentBlack, r.getX(), r.getY() + 14.0f, false));
    g.fillRect (r.withHeight (14.0f));
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.18f), r.getX(), r.getY(),
                                             juce::Colours::transparentBlack, r.getX() + 10.0f, r.getY(), false));
    g.fillRect (r.withWidth (10.0f));

    g.setColour (panelEdge.withAlpha (0.35f));
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawHorizontalLine ((int) r.getBottom() - 1, r.getX() + radius, r.getRight() - radius);
}
} // namespace automech::theme
