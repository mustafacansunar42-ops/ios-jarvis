#include "Parameters.h"

namespace frier::params
{
namespace
{
using Float = juce::AudioParameterFloat;
using Attrs = juce::AudioParameterFloatAttributes;

juce::String signedFixed (float v, int decimals)
{
    const auto s = juce::String (v, decimals);
    return v > 0.0f ? "+" + s : s;
}

juce::String formatDb (float v, int)        { return juce::String (v, 1) + " dB"; }
juce::String formatSignedDb (float v, int)  { return signedFixed (v, 1) + " dB"; }
juce::String formatPercent (float v, int)   { return juce::String (juce::roundToInt (v)) + " %"; }
juce::String formatSignedPct (float v, int) { return signedFixed ((float) juce::roundToInt (v), 0) + " %"; }

juce::String formatHz (float v, int)
{
    if (v < 1000.0f)
        return juce::String (juce::roundToInt (v)) + " Hz";

    return juce::String (v / 1000.0f, v < 10000.0f ? 2 : 1) + " kHz";
}

float parseHz (const juce::String& text)
{
    const auto t = text.trim().toLowerCase();
    const auto v = t.getFloatValue();
    return t.contains ("k") ? v * 1000.0f : v;
}

float parseNumber (const juce::String& text) { return text.trim().getFloatValue(); }

std::unique_ptr<Float> makeFloat (const char* pid,
                                  const juce::String& name,
                                  juce::NormalisableRange<float> range,
                                  float defaultValue,
                                  std::function<juce::String (float, int)> toText,
                                  std::function<float (const juce::String&)> fromText = parseNumber)
{
    auto attributes = Attrs().withStringFromValueFunction (std::move (toText))
                             .withValueFromStringFunction (std::move (fromText));

    return std::make_unique<Float> (juce::ParameterID { pid, 1 }, name, range, defaultValue, attributes);
}

juce::NormalisableRange<float> skewedRange (float lo, float hi, float centre)
{
    juce::NormalisableRange<float> r { lo, hi, 0.01f };
    r.setSkewForCentre (centre);
    return r;
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (makeFloat (id::drive,   "Drive",    { 0.0f, 48.0f, 0.01f },     12.0f,    formatDb));
    layout.add (makeFloat (id::tone,    "Tone",     { -6.0f, 6.0f, 0.01f },      0.0f,    formatSignedDb));
    layout.add (makeFloat (id::heat,    "Heat",     { 0.0f, 100.0f, 0.01f },    40.0f,    formatPercent));
    layout.add (makeFloat (id::crunch,  "Crunch",   { 0.0f, 100.0f, 0.01f },     0.0f,    formatPercent));
    layout.add (makeFloat (id::bias,    "Bias",     { -100.0f, 100.0f, 0.01f },  0.0f,    formatSignedPct));
    layout.add (makeFloat (id::lowCut,  "Low Cut",  skewedRange (20.0f, 500.0f, 100.0f),       20.0f, formatHz, parseHz));
    layout.add (makeFloat (id::highCut, "High Cut", skewedRange (2000.0f, 20000.0f, 6500.0f), 20000.0f, formatHz, parseHz));
    layout.add (makeFloat (id::mix,     "Mix",      { 0.0f, 100.0f, 0.01f },   100.0f,    formatPercent));
    layout.add (makeFloat (id::output,  "Output",   { -24.0f, 12.0f, 0.01f },    0.0f,    formatSignedDb));

    layout.add (std::make_unique<juce::AudioParameterBool>   (juce::ParameterID { id::autoGain, 1 }, "Auto Gain", true));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id::model, 1 }, "Model", modelNames, 0));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id::oversampling, 1 }, "Oversampling", oversamplingNames, 2));
    layout.add (std::make_unique<juce::AudioParameterBool>   (juce::ParameterID { id::bypass, 1 }, "Bypass", false));

    return layout;
}
} // namespace frier::params
