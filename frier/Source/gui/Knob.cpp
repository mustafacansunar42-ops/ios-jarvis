#include "Knob.h"
#include "FrierLookAndFeel.h"
#include "Palette.h"

namespace frier
{
namespace
{
constexpr int labelHeight = 16;
constexpr int valueHeight = 16;
constexpr int gap = 6;

constexpr float pixelsPerFullTurn     = 220.0f;
constexpr float finePixelsPerFullTurn = 1400.0f;
} // namespace

Knob::Knob (juce::RangedAudioParameter& parameter, juce::String labelText, Size knobSize)
    : param (parameter),
      attachment (parameter, [this] (float value)
                  {
                      normalisedValue = param.convertTo0to1 (value);
                      repaint();
                  }),
      label (std::move (labelText)),
      size (knobSize)
{
    const auto& range = param.getNormalisableRange();
    bipolar = range.start < 0.0f && range.end > 0.0f;

    setRepaintsOnMouseActivity (true);
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    setTitle (param.getName (64));
    attachment.sendInitialUpdate();
}

int Knob::knobDiameter (Size s) noexcept   { return s == Size::Large ? 92 : 58; }
int Knob::preferredHeight (Size s) noexcept { return labelHeight + gap + knobDiameter (s) + gap + valueHeight; }

juce::Rectangle<float> Knob::knobArea() const
{
    const float d = (float) knobDiameter (size);
    return { ((float) getWidth() - d) * 0.5f, (float) (labelHeight + gap), d, d };
}

void Knob::paint (juce::Graphics& g)
{
    const bool active = dragging || isMouseOver (true);
    const auto bounds = getLocalBounds();

    // label above (small spaced caps, like the reference)
    g.setColour (active ? palette::cream : palette::textDim);
    g.setFont (FrierLookAndFeel::font (size == Size::Large ? 12.0f : 10.5f, 0.24f));
    g.drawText (label.toUpperCase(), bounds.withHeight (labelHeight), juce::Justification::centred, false);

    FrierLookAndFeel::drawKnob (g, knobArea(), normalisedValue, bipolar, active);

    // value below
    const auto valueArea = bounds.withTop (bounds.getBottom() - valueHeight);
    g.setColour (active ? palette::goldBright : palette::textFaint.brighter (0.35f));
    g.setFont (FrierLookAndFeel::font (size == Size::Large ? 12.0f : 10.5f, 0.06f));
    g.drawText (param.getCurrentValueAsText(), valueArea, juce::Justification::centred, false);
}

//==============================================================================
void Knob::setNormalised (float normalised, bool asGesture)
{
    const float value = param.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, normalised));

    if (asGesture)
        attachment.setValueAsPartOfGesture (value);
    else
        attachment.setValueAsCompleteGesture (value);
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
} // namespace frier
