#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

namespace scorch::params
{
namespace id
{
inline constexpr const char* drive        = "drive";
inline constexpr const char* tone         = "tone";
inline constexpr const char* heat         = "heat";
inline constexpr const char* crunch       = "crunch";
inline constexpr const char* bias         = "bias";
inline constexpr const char* lowCut       = "lowCut";
inline constexpr const char* highCut      = "highCut";
inline constexpr const char* mix          = "mix";
inline constexpr const char* output       = "output";
inline constexpr const char* autoGain     = "autoGain";
inline constexpr const char* model        = "model";
inline constexpr const char* oversampling = "oversampling";
inline constexpr const char* bypass       = "bypass";
} // namespace id

inline const juce::StringArray modelNames        { "Tube", "Tape", "Transistor", "Fuzz", "Fry" };
inline const juce::StringArray oversamplingNames { "1x", "2x", "4x", "8x" };

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

//==============================================================================
struct Preset
{
    const char* name;
    int   model;          // index into modelNames
    float drive;          // dB
    float tone;           // dB tilt
    float heat;           // %
    float crunch;         // %
    float bias;           // %
    float lowCut;         // Hz
    float highCut;        // Hz
    float mix;            // %
    float output;         // dB
    bool  autoGain;
    int   oversampling;   // index into oversamplingNames
};

inline constexpr std::array<Preset, 5> factoryPresets {{
    //  name           model drive  tone  heat crunch bias  lowCut highCut  mix  out   auto  os
    { "Warm Toast",    0,    9.0f,  1.0f, 35.0f, 10.0f, 15.0f,  30.0f, 16000.0f, 100.0f,  0.0f, true, 2 },
    { "Crispy Tape",   1,   15.0f,  2.0f, 55.0f, 35.0f,  0.0f,  40.0f, 14000.0f,  85.0f,  0.0f, true, 2 },
    { "Deep Fry",      2,   30.0f, -1.5f, 70.0f, 20.0f, 10.0f,  60.0f, 11000.0f, 100.0f, -1.0f, true, 2 },
    { "Fuzz Pedal",    3,   36.0f,  2.5f, 60.0f, 40.0f, 30.0f,  90.0f,  8000.0f, 100.0f, -2.0f, true, 2 },
    { "Bit Burn",      4,   18.0f,  0.0f, 75.0f,  0.0f,  0.0f,  20.0f, 20000.0f,  70.0f, -3.0f, true, 2 },
}};
} // namespace scorch::params
