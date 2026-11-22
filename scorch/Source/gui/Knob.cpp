#include "Knob.h"
#include "ScorchLookAndFeel.h"
#include "Palette.h"

namespace scorch
{
namespace
{
constexpr float pixelsPerFullTurn     = 220.0f;
constexpr float finePixelsPerFullTurn = 1400.0f;
} // namespace

Knob::Knob (juce::RangedAudioParameter& parameter, juce::String labelText)
    : param (parameter),
      attachment (parameter, [this] (float v)
                  {
                      normalisedValue = param.convertTo0to1 (v);
                      repaint();
                  }),
      label (std::move (labelText))
{
    const auto& range = param.getNormalisableRange();
    bipolar = range.start < 0.0f && range.end > 0.0f;

    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    setTitle (param.getName (64));
    attachment.sendInitialUpdate();
}

juce::Rectangle<float> Knob::knobArea() const
{
    const float d = (float) knobSize;
    return { ((float) getWidth() - d) * 0.5f, (float) (bubbleHeight + 4), d, d };
}

bool Knob::hitTest (int x, int y)
{
    // the bubble area above the knob is not clickable
    return knobArea().expanded (4.0f).withBottom ((float) getHeight()).contains ((float) x, (float) y);
}

void Knob::paint (juce::Graphics& g)
{
    const bool active = dragging || isMouseOver (true);
    const auto area = knobArea();

    ScorchLookAndFeel::drawKnob (g, area, normalisedValue, bipolar, active);

    // lowercase label under the knob
    g.setColour (active ? palette::value : palette::label);
    g.setFont (ScorchLookAndFeel::font (12.0f, 0.05f));
    g.drawText (label.toLowerCase(), juce::Rectangle<float> (0.0f, area.getBottom() + 2.0f, (float) getWidth(), (float) labelHeight),
                juce::Justification::centred, false);

    // value tooltip bubble
    if (active)
    {
        const auto text = param.getCurrentValueAsText();
        const auto f = ScorchLookAndFeel::font (11.5f, 0.03f);
        const float w = juce::jmin ((float) getWidth() - 2.0f,
                                    juce::GlyphArrangement::getStringWidth (f, text) + 16.0f);
        const auto bubble = juce::Rectangle<float> (w, (float) bubbleHeight - 3.0f)
                                .withCentre ({ (float) getWidth() * 0.5f, (float) bubbleHeight * 0.5f - 1.0f });

        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillRoundedRectangle (bubble.translated (0.0f, 1.5f), 4.0f);
        g.setColour (palette::panel);
        g.fillRoundedRectangle (bubble, 4.0f);
        g.setColour (palette::accent.withAlpha (0.8f));
        g.drawRoundedRectangle (bubble.reduced (0.5f), 4.0f, 1.0f);

        juce::Path tail;
        const float cx = bubble.getCentreX();
        tail.addTriangle (cx - 4.0f, bubble.getBottom() - 0.5f, cx + 4.0f, bubble.getBottom() - 0.5f, cx, bubble.getBottom() + 3.5f);
        g.setColour (palette::accent.withAlpha (0.8f));
        g.fillPath (tail);

        g.setColour (palette::value);
        g.setFont (f);
        g.drawText (text, bubble, juce::Justification::centred, false);
    }
}

//==============================================================================
void Knob::setNormalised (float normalised, bool asGesture)
{
    const float v = param.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, normalised));

    if (asGesture)
        attachment.setValueAsPartOfGesture (v);
    else
        attachment.setValueAsCompleteGesture (v);
}

void Knob::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        return;

    dragging = true;
    lastDragY = e.position.y;
    dragNormalised = normalisedValue;
    attachment.beginGesture();
    repaint();
}

void Knob::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging)
        return;

    const float dy = lastDragY - e.position.y;
    lastDragY = e.position.y;

    const float pixelsForFullRange = e.mods.isShiftDown() ? finePixelsPerFullTurn : pixelsPerFullTurn;
    dragNormalised = juce::jlimit (0.0f, 1.0f, dragNormalised + dy / pixelsForFullRange);
    setNormalised (dragNormalised, true);
}

void Knob::mouseUp (const juce::MouseEvent&)
{
    if (! dragging)
        return;

    dragging = false;
    attachment.endGesture();
    repaint();
}

void Knob::mouseDoubleClick (const juce::MouseEvent&)
{
    setNormalised (param.getDefaultValue(), false);
}

void Knob::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (dragging)
        return;

    float delta = std::abs (wheel.deltaY) > std::abs (wheel.deltaX) ? wheel.deltaY : -wheel.deltaX;
    if (wheel.isReversed)
        delta = -delta;

    const float step = e.mods.isShiftDown() ? 0.08f : 0.5f;
    setNormalised (normalisedValue + delta * step * 0.1f * (wheel.isSmooth ? 1.0f : 1.5f), false);
}
} // namespace scorch
