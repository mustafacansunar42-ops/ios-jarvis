#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"
#include "gui/ScorchLookAndFeel.h"
#include "gui/Knob.h"
#include "gui/LevelMeter.h"
#include "gui/PixelVisualizer.h"

namespace scorch
{
/** Small orange chevron used for the preset previous / next arrows. */
class ChevronButton final : public juce::Button
{
public:
    explicit ChevronButton (bool pointsLeft);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    bool left;
};

/** The "PIXEL" tag under the display: shows the current view and cycles it on click. */
class PixelTagButton final : public juce::Button
{
public:
    explicit PixelTagButton (PixelVisualizer&);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    PixelVisualizer& visualizer;
};

/** All editor content, laid out in fixed 760 x 920 design units.
    The editor scales this component with an AffineTransform when resized. */
class MainView final : public juce::Component, private juce::Timer
{
public:
    static constexpr int designWidth = 760;
    static constexpr int designHeight = 920;

    explicit MainView (ScorchAudioProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void stepPreset (int delta);
    void syncPreset();

    juce::RangedAudioParameter& param (const char* id);

    ScorchAudioProcessor& processor;

    // header
    ChevronButton prevButton { true }, nextButton { false };
    juce::ToggleButton bypassButton { "bypass" };

    // display
    PixelVisualizer visualizer;
    PixelTagButton pixelTag { visualizer };

    // dropdowns
    juce::ComboBox modelBox, presetBox, oversamplingBox;

    // knobs
    Knob drive, tone, heat, crunch, bias, lowCut, highCut, mix, output;
    juce::ToggleButton autoGainButton { "auto gain" };

    // footer meters
    LevelMeter inputMeter, outputMeter;

    // attachments
    juce::AudioProcessorValueTreeState::ButtonAttachment bypassAttachment, autoGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modelAttachment, oversamplingAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainView)
};
} // namespace scorch

//==============================================================================
class ScorchAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit ScorchAudioProcessorEditor (ScorchAudioProcessor&);
    ~ScorchAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    scorch::ScorchLookAndFeel lookAndFeel;   // declared first: outlives every child
    scorch::MainView view;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScorchAudioProcessorEditor)
};
