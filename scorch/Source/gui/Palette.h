#pragma once

#include <juce_graphics/juce_graphics.h>

/** SCORCH colour scheme: near-black panels with a dark-orange accent. */
namespace scorch::palette
{
inline const juce::Colour background   { 0xff0a0a0b };
inline const juce::Colour glow         { 0xff3a1a08 };
inline const juce::Colour panel        { 0xff141416 };
inline const juce::Colour outline      { 0xff2a2a2e };
inline const juce::Colour display      { 0xff060606 };

inline const juce::Colour accent       { 0xffcc5500 };
inline const juce::Colour highlight    { 0xffe8742a };
inline const juce::Colour deep         { 0xff8a3a08 };
inline const juce::Colour amber        { 0xffffb020 };
inline const juce::Colour hot          { 0xffff5a1f };
inline const juce::Colour spark        { 0xffffe2b8 };   // brightest peaks

inline const juce::Colour label        { 0xffd9cfc4 };
inline const juce::Colour value        { 0xffffffff };
inline const juce::Colour labelDim     { 0xff7d746b };
} // namespace scorch::palette
