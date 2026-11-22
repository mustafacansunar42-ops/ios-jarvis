#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace automech::ids
{
// Main
inline constexpr const char* key         = "key";
inline constexpr const char* scale       = "scale";
inline constexpr const char* reference   = "reference";
inline constexpr const char* correction  = "correction";
inline constexpr const char* speed       = "speed";
inline constexpr const char* noteTrans   = "noteTrans";
inline constexpr const char* flex        = "flex";
inline constexpr const char* human       = "human";
inline constexpr const char* formant     = "formant";
inline constexpr const char* vibrato     = "vibrato";
inline constexpr const char* mix         = "mix";
inline constexpr const char* bypass      = "bypass";
// Clean
inline constexpr const char* cleanOn     = "cleanOn";
inline constexpr const char* cleanup     = "cleanup";
inline constexpr const char* lowCut      = "lowCut";
inline constexpr const char* hum         = "hum";
inline constexpr const char* tighten     = "tighten";
inline constexpr const char* onset       = "onset";
inline constexpr const char* sensitivity = "sensitivity";
// Voice
inline constexpr const char* voiceType       = "voiceType";
inline constexpr const char* formantPreserve = "formantPreserve";
// Vibrato
inline constexpr const char* vibRate   = "vibRate";
inline constexpr const char* vibDelay  = "vibDelay";
inline constexpr const char* vibShape  = "vibShape";
// Mech
inline constexpr const char* mechMode = "mechMode";
inline constexpr const char* mechFreq = "mechFreq";
inline constexpr const char* mechMix  = "mechMix";
// FX
inline constexpr const char* drive = "drive";
inline constexpr const char* width = "width";
// MIDI
inline constexpr const char* midiFollow = "midiFollow";

// Harmony voice n (0..2) and custom-scale note n (0..11)
inline juce::String harmInterval (int n) { return "harm" + juce::String (n + 1) + "Interval"; }
inline juce::String harmLevel (int n)    { return "harm" + juce::String (n + 1) + "Level"; }
inline juce::String harmPan (int n)      { return "harm" + juce::String (n + 1) + "Pan"; }
inline juce::String noteEnabled (int n)  { return "noteEn" + juce::String (n); }
} // namespace automech::ids

namespace automech
{
inline const juce::StringArray& keyNames()
{
    static const juce::StringArray names { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return names;
}

inline const juce::StringArray& scaleNames()
{
    static const juce::StringArray names { "Chromatic", "Major", "Minor", "Dorian", "Phrygian", "Lydian",
                                           "Mixolydian", "Pentatonic Major", "Pentatonic Minor", "Blues",
                                           "Harmonic Minor", "Custom" };
    return names;
}

inline const juce::StringArray& voiceTypeNames()
{
    static const juce::StringArray names { "Soprano", "Alto", "Tenor", "Bass", "Instrument" };
    return names;
}

// Pitch search range per voice type (Hz)
inline std::pair<float, float> voiceRange (int type)
{
    switch (type)
    {
        case 0:  return { 170.0f, 1100.0f };
        case 1:  return { 120.0f, 800.0f };
        case 2:  return { 80.0f, 700.0f };
        case 3:  return { 55.0f, 420.0f };
        default: return { 50.0f, 1400.0f };
    }
}

inline const juce::StringArray& harmonyIntervalNames()
{
    // Scale steps relative to the corrected note; index 7 is "Off".
    static const juce::StringArray names { "-7 steps", "-6 steps", "-5 steps", "-4 steps", "-3 steps",
                                           "-2 steps", "-1 step", "Off", "+1 step", "+2 steps",
                                           "+3 steps", "+4 steps", "+5 steps", "+6 steps", "+7 steps" };
    return names;
}

inline juce::String midiNoteName (int note)
{
    if (note < 0) return "-";
    return keyNames()[note % 12] + juce::String (note / 12 - 1);
}
} // namespace automech
