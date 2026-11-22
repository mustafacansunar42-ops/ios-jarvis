#include "ModelSelector.h"
#include "FrierLookAndFeel.h"
#include "Palette.h"
#include "../Parameters.h"

namespace frier
{
ModelSelector::ModelSelector (juce::RangedAudioParameter& modelParameter)
    : param (modelParameter),
      attachment (modelParameter, [this] (float value)
                  {
                      selected = juce::roundToInt (value);
                      repaint();
                  }),
      names (params::modelNames)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTitle ("Model");
    attachment.sendInitialUpdate();
}

juce::Rectangle<float> ModelSelector::itemBounds (int index) const
{
    constexpr float spacing = 8.0f;
    const float w = ((float) getWidth() - spacing * (float) (numItems() - 1)) / (float) numItems();
    return { (float) index * (w + spacing), 0.0f, w, (float) getHeight() };
}

int ModelSelector::itemAt (juce::Point<float> pos) const
{
    for (int i = 0; i < numItems(); ++i)
        if (itemBounds (i).contains (pos))
            return i;

    return -1;
}

void ModelSelector::paint (juce::Graphics& g)
{
    for (int i = 0; i < numItems(); ++i)
    {
        const auto r = itemBounds (i).reduced (0.5f);
        const bool on = i == selected;
        const bool over = i == hovered;

        g.setColour (on ? palette::gold.withAlpha (0.09f) : palette::panel.withAlpha (0.75f));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (on ? palette::gold.withAlpha (0.75f) : (over ? palette::textFaint : palette::panelBorder));
        g.drawRoundedRectangle (r, 4.0f, 1.0f);

        const auto led = juce::Rectangle<float> (9.0f, 9.0f).withCentre ({ r.getX() + 15.0f, r.getCentreY() });
        FrierLookAndFeel::drawLed (g, led, on, palette::gold);

        g.setColour (on ? palette::cream : (over ? palette::textDim.brighter (0.2f) : palette::textDim));
        g.setFont (FrierLookAndFeel::font (11.5f, 0.2f));
        g.drawFittedText (names[i].toUpperCase(), r.withTrimmedLeft (24.0f).withTrimmedRight (4.0f).toNearestInt(),
                          juce::Justification::centred, 1, 0.85f);
    }
}

void ModelSelector::mouseDown (const juce::MouseEvent& e)
{
    const int index = itemAt (e.position);

    if (index >= 0 && index != selected)
        attachment.setValueAsCompleteGesture ((float) index);
}

void ModelSelector::mouseMove (const juce::MouseEvent& e)
{
    const int index = itemAt (e.position);

    if (index != hovered)
    {
        hovered = index;
        repaint();
    }
}

void ModelSelector::mouseExit (const juce::MouseEvent&)
{
    hovered = -1;
    repaint();
}
} // namespace frier
