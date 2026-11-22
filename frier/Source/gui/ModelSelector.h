#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace frier
{
/** Row of five buttons (TUBE / TAPE / TRANSISTOR / FUZZ / FRY) bound to the model choice parameter. */
class ModelSelector final : public juce::Component
{
public:
    explicit ModelSelector (juce::RangedAudioParameter& modelParameter);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    int numItems() const noexcept { return names.size(); }
    juce::Rectangle<float> itemBounds (int index) const;
    int itemAt (juce::Point<float>) const;

    juce::RangedAudioParameter& param;
    juce::ParameterAttachment attachment;
    juce::StringArray names;
    int selected = 0, hovered = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModelSelector)
};
} // namespace frier
