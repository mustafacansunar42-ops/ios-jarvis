#pragma once

#include <juce_graphics/juce_graphics.h>

/** Colours sampled from the reference GUI: near-black panels, thin warm-grey
    outlines, brushed-gold accents and cream text. The orange/amber "heat"
    colours come from the fallback palette and only appear where the signal is hot. */
namespace frier::palette
{
inline const juce::Colour background   { 0xff0c0b0a };
inline const juce::Colour backgroundHi { 0xff1b1713 };
inline const juce::Colour glow         { 0xff3a2c1c };
inline const juce::Colour panel        { 0xff121110 };
inline const juce::Colour panelBorder  { 0xff3a342d };
inline const juce::Colour divider      { 0xff2a2622 };
inline const juce::Colour display      { 0xff080706 };

inline const juce::Colour gold         { 0xffc9a46b };
inline const juce::Colour goldBright   { 0xffe6c88e };
inline const juce::Colour cream        { 0xffece3d3 };
inline const juce::Colour textDim      { 0xffa0978a };
inline const juce::Colour textFaint    { 0xff5f5951 };
inline const juce::Colour ledOff       { 0xff4a4640 };

inline const juce::Colour knobTop      { 0xff302d29 };
inline const juce::Colour knobBottom   { 0xff0d0c0b };
inline const juce::Colour knobRim      { 0xffc9bfae };

inline const juce::Colour amber        { 0xffffb020 };
inline const juce::Colour hot          { 0xffff5a1f };
} // namespace frier::palette
