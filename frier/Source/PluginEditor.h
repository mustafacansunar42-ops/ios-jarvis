#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"
#include "gui/FrierLookAndFeel.h"
#include "gui/Knob.h"
#include "gui/LevelMeter.h"
#include "gui/ModelSelector.h"
#include "gui/PixelVisualizer.h"

namespace frier
{
/** Small gold chevron used for the preset previous / next arrows. */
class ChevronButton final : public juce::Button
{
public:
    explicit ChevronButton (bool pointsLeft);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    bool left;
};

/** All editor content, laid out in fixed 1000 x 640 design units.
    The editor scales this component with an AffineTransform when resized. */
class MainView final : public juce::Component, private juce::Timer
{
public:
    static constexpr int designWidth = 1000;
    static constexpr int designHeight = 640;

    explicit MainView (FrierAudioProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void stepPreset (int delta);
    void syncHeader();

    juce::RangedAudioParameter& param (const char* id);

    FrierAudioProcessor& processor;

    // header
    juce::ComboBox presetBox;
    ChevronButton prevButton { true }, nextButton { false };
    juce::TextButton aButton { "A" }, bButton { "B" };
    juce::ToggleButton bypassButton { "BYPASS" };

    // centre
    PixelVisualizer visualizer;
    ModelSelector modelSelector;
    juce::ComboBox oversamplingBox;

    // knobs
    Knob drive, tone, heat, crunch, bias, lowCut, highCut, mix, output;
    juce::ToggleButton autoGainButton { "AUTO" };

    // meters
    LevelMeter inputMeter, outputMeter;

    // attachments
    juce::AudioProcessorValueTreeState::ButtonAttachment bypassAttachment, autoGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> oversamplingAttachment;   // created after the items exist

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainView)
};
} // namespace frier

//==============================================================================
class FrierAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit FrierAudioProcessorEditor (FrierAudioProcessor&);
    ~FrierAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    frier::FrierLookAndFeel lookAndFeel;   // declared first: outlives every child
    frier::MainView view;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FrierAudioProcessorEditor)
};
