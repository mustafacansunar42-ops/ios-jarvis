#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace automech
{
using APVTS = juce::AudioProcessorValueTreeState;

// Rotary slider: vertical drag, Shift = fine, double-click = reset, wheel.
class KnobSlider : public juce::Slider
{
public:
    KnobSlider();
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

private:
    float lastY = 0.0f;
};

// Knob with a white uppercase label and value text underneath.
class LabeledKnob : public juce::Component
{
public:
    LabeledKnob (APVTS&, const juce::String& paramId, const juce::String& label, bool large);
    void paint (juce::Graphics&) override;
    void resized() override;

    static int preferredWidth (bool large) { return large ? 104 : 80; }

    KnobSlider slider;

private:
    juce::String labelText;
    bool isLarge;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

// Small caps label above a dropdown.
class LabeledCombo : public juce::Component
{
public:
    LabeledCombo (APVTS&, const juce::String& paramId, const juce::String& label, const juce::StringArray& items);
    void paint (juce::Graphics&) override;
    void resized() override;

    juce::ComboBox combo;

private:
    juce::String labelText;
    std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
};

// Pill switch with label underneath.
class LabeledToggle : public juce::Component
{
public:
    LabeledToggle (APVTS&, const juce::String& paramId, const juce::String& label);
    void paint (juce::Graphics&) override;
    void resized() override;

    juce::ToggleButton toggle;

private:
    juce::String labelText;
    std::unique_ptr<APVTS::ButtonAttachment> attachment;
};

// "TITLE        value" above a thin horizontal slider.
class LabeledHSlider : public juce::Component
{
public:
    LabeledHSlider (APVTS&, const juce::String& paramId, const juce::String& label);
    void paint (juce::Graphics&) override;
    void resized() override;

    juce::Slider slider;

private:
    juce::String labelText;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

// Dynamic text block (title + value supplied by a callback).
class InfoText : public juce::Component
{
public:
    InfoText (juce::String title, std::function<juce::String()> valueFn);
    void paint (juce::Graphics&) override;
    void refresh();

private:
    juce::String title, lastValue;
    std::function<juce::String()> valueFn;
};

// Small dark button that draws a vector icon.
class IconButton : public juce::Button
{
public:
    using IconFn = std::function<void (juce::Graphics&, juce::Rectangle<float>, bool on)>;
    IconButton (const juce::String& name, IconFn fn, bool round = false);
    void paintButton (juce::Graphics&, bool over, bool down) override;

private:
    IconFn iconFn;
    bool isRound;
};

// Tab strip: active tab is lighter with a white underline.
class TabStrip : public juce::Component
{
public:
    explicit TabStrip (juce::StringArray names);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void setCurrent (int index);
    int getCurrent() const noexcept { return current; }

    std::function<void (int)> onChange;

private:
    juce::StringArray names;
    int current = 0;
};

// One tab page: a single row of controls laid out left to right.
class TabPage : public juce::Component
{
public:
    void add (std::unique_ptr<juce::Component> c, int width);
    void addGap (int width);
    void resized() override;
    template <typename T> T* last() { return dynamic_cast<T*> (items.back().comp.get()); }
    void refreshInfo();

private:
    struct Item { std::unique_ptr<juce::Component> comp; int width; };
    std::vector<Item> items;
};
} // namespace automech
