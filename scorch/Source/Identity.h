#pragma once

// The manufacturer name is defined once, in CMakeLists.txt (SCORCH_MANUFACTURER).
// JUCE passes it to the compiler as JucePlugin_Manufacturer.
#ifndef JucePlugin_Manufacturer
 #error "JucePlugin_Manufacturer comes from SCORCH_MANUFACTURER in CMakeLists.txt"
#endif

namespace scorch::identity
{
inline constexpr const char* manufacturer = JucePlugin_Manufacturer;
inline constexpr const char* pluginName   = "SCORCH";
inline constexpr const char* tagline      = "Saturation & distortion, warm to extreme";
} // namespace scorch::identity
