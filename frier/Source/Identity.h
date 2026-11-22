#pragma once

// The manufacturer name is defined once, in CMakeLists.txt (FRIER_MANUFACTURER).
// JUCE passes it to the compiler as JucePlugin_Manufacturer.
#ifndef JucePlugin_Manufacturer
 #error "JucePlugin_Manufacturer comes from FRIER_MANUFACTURER in CMakeLists.txt"
#endif

namespace frier::identity
{
inline constexpr const char* manufacturer = JucePlugin_Manufacturer;
inline constexpr const char* pluginName   = "FRIER";
inline constexpr const char* tagline      = "Saturation & distortion, warm to extreme";
} // namespace frier::identity
