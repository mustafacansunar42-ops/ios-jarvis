#pragma once

#include "PluginProcessor.h"
#include "UI/AutoMechLookAndFeel.h"
#include "UI/Controls.h"
#include "UI/PianoKeyboard.h"
#include "UI/PitchDisplay.h"

class AutoMechEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    static constexpr int kDesignWidth = 1000, kDesignHeight = 830;

    explicit AutoMechEditor (AutoMechProcessor&);
    ~AutoMechEditor() override;

    void paint (juce::Graphics&) override {}
    void resized() override;

    // Pulls telemetry + visualizer audio from the processor (called by the timer).
    void pollProcessor();
    juce::Component& getRoot() noexcept { return root; }
    void cycleVisualizerMode() { display.getVisualizer().cycleMode(); display.repaint(); }

private:
    void timerCallback() override { pollProcessor(); }
    void paintRoot (juce::Graphics&);
    void buildHeader();
    void buildMainControls();
    void buildTabs();
    void showPresetMenu();
    void showScaleMenu();
    void showSettingsMenu();
    void toggleKeyboardNote (int pitchClass);

    struct Root : juce::Component
    {
        std::function<void (juce::Graphics&)> painter;
        void paint (juce::Graphics& g) override { if (painter) painter (g); }
    };

    AutoMechProcessor& processor;
    automech::AutoMechLookAndFeel lookAndFeel;
    Root root;

    // Header
    std::unique_ptr<automech::IconButton> scaleButton, settingsButton, midiButton;
    juce::TextButton presetButton { juce::String::fromUTF8 ("PRESET \xe2\x96\xbe") }, abButton { "A/B" }, bypassButton { "BYPASS" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment, midiAttachment;

    // Main area
    automech::PitchDisplay display;
    std::unique_ptr<automech::LabeledCombo> keyCombo, scaleCombo, referenceCombo;
    juce::TextButton learnButton { "LEARN" };
    automech::PianoKeyboard keyboard;
    std::vector<std::unique_ptr<automech::LabeledKnob>> bigKnobs, smallKnobs;

    // Tabs
    automech::TabStrip tabs { { "CLEAN", "VOICE", "VIBRATO", "HARMONY", "MECH", "FX", "MIDI" } };
    std::vector<std::unique_ptr<automech::TabPage>> pages;

    std::unique_ptr<juce::FileChooser> fileChooser;
    std::vector<float> visScratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoMechEditor)
};
