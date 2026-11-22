#include "ScorchLookAndFeel.h"
#include "Palette.h"

namespace scorch
{
namespace
{
constexpr float startAngle = -juce::MathConstants<float>::pi * 0.75f;
constexpr float endAngle   =  juce::MathConstants<float>::pi * 0.75f;

juce::Point<float> pointOnCircle (juce::Point<float> centre, float radius, float angle)
{
    // angle measured clockwise from 12 o'clock
    return { centre.x + radius * std::sin (angle), centre.y - radius * std::cos (angle) };
}

void strokeArc (juce::Graphics& g, juce::Point<float> c, float r, float from, float to, float thickness)
{
    juce::Path arc;
    arc.addCentredArc (c.x, c.y, r, r, 0.0f, from, to, true);
    g.strokePath (arc, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
} // namespace

ScorchLookAndFeel::ScorchLookAndFeel()
{
    using namespace palette;

    setColour (juce::ResizableWindow::backgroundColourId, background);
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, label);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.25f));
    setColour (juce::PopupMenu::highlightedTextColourId, value);
    setColour (juce::ComboBox::textColourId, label);
    setColour (juce::ComboBox::backgroundColourId, panel);
    setColour (juce::ComboBox::outlineColourId, outline);
    setColour (juce::ComboBox::arrowColourId, label);
    setColour (juce::ToggleButton::textColourId, label);
    setColour (juce::ToggleButton::tickColourId, accent);
    setColour (juce::TooltipWindow::backgroundColourId, panel);
    setColour (juce::TooltipWindow::textColourId, value);
}

juce::Font ScorchLookAndFeel::font (float height, float tracking, bool bold)
{
    return juce::Font (juce::FontOptions()
                           .withHeight (height)
                           .withKerningFactor (tracking)
                           .withStyle (bold ? "Bold" : "Regular"));
}

//==============================================================================
void ScorchLookAndFeel::drawKnob (juce::Graphics& g, juce::Rectangle<float> bounds, float proportion,
                                 bool bipolar, bool highlighted)
{
    using namespace palette;

    const float size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const auto centre = bounds.getCentre();
    const float ringRadius = size * 0.5f - 3.0f;
    const float capRadius  = ringRadius - 6.0f;
    const auto cap = juce::Rectangle<float> (capRadius * 2.0f, capRadius * 2.0f).withCentre (centre);

    const float valueAngle = startAngle + proportion * (endAngle - startAngle);
    const float fromAngle  = bipolar ? 0.0f : startAngle;

    // Ring track (thin, dark orange)
    g.setColour (deep.withAlpha (0.65f));
    strokeArc (g, centre, ringRadius, startAngle, endAngle, 2.0f);

    // Bright value arc with a soft glow
    if (std::abs (valueAngle - fromAngle) > 0.001f)
    {
        const float a0 = juce::jmin (fromAngle, valueAngle);
        const float a1 = juce::jmax (fromAngle, valueAngle);

        g.setColour (highlight.withAlpha (highlighted ? 0.35f : 0.22f));
        strokeArc (g, centre, ringRadius, a0, a1, 6.0f);
        g.setColour (highlighted ? highlight.brighter (0.25f) : highlight);
        strokeArc (g, centre, ringRadius, a0, a1, 2.2f);
    }

    // Soft drop shadow under the cap
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.8f), centre.translated (0.0f, capRadius * 0.25f),
                                             juce::Colours::transparentBlack, centre.translated (0.0f, capRadius * 1.45f), true));
    g.fillEllipse (cap.expanded (capRadius * 0.3f).translated (0.0f, capRadius * 0.22f));

    // Glossy black cap
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2c2c30), cap.getX() + capRadius * 0.6f, cap.getY() + capRadius * 0.35f,
                                             juce::Colour (0xff040405), cap.getRight(), cap.getBottom(), true));
    g.fillEllipse (cap);

    // Bevel: dark outer edge, faint light inner edge
    g.setColour (juce::Colours::black);
    g.drawEllipse (cap, 1.2f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.22f), centre.x, cap.getY(),
                                             juce::Colours::white.withAlpha (0.0f), centre.x, centre.y + capRadius * 0.3f, false));
    g.drawEllipse (cap.reduced (1.2f), 1.0f);

    // Top highlight (gloss)
    {
        const auto gloss = juce::Rectangle<float> (capRadius * 1.25f, capRadius * 0.75f)
                               .withCentre ({ centre.x, cap.getY() + capRadius * 0.48f });
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.16f), gloss.getCentreX(), gloss.getY(),
                                                 juce::Colours::white.withAlpha (0.0f), gloss.getCentreX(), gloss.getBottom(), false));
        g.fillEllipse (gloss);
    }

    // Short orange pointer
    {
        const auto a = pointOnCircle (centre, capRadius * 0.42f, valueAngle);
        const auto b = pointOnCircle (centre, capRadius * 0.86f, valueAngle);
        g.setColour (highlight.withAlpha (0.35f));
        g.drawLine ({ a, b }, 4.0f);
        g.setColour (highlighted ? highlight.brighter (0.3f) : highlight);
        g.drawLine ({ a, b }, 2.0f);
    }
}

//==============================================================================
void ScorchLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool)
{
    using namespace palette;

    const auto r = button.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = button.getToggleState();
    const auto onColour = button.findColour (juce::ToggleButton::tickColourId);
    const float radius = r.getHeight() * 0.5f;

    g.setColour (on ? onColour.withAlpha (0.22f) : panel);
    g.fillRoundedRectangle (r, radius);
    g.setColour (on ? onColour : (highlighted ? labelDim : outline));
    g.drawRoundedRectangle (r, radius, 1.0f);

    // status dot
    const auto dot = juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ r.getX() + radius + 2.0f, r.getCentreY() });
    if (on)
    {
        g.setColour (onColour.withAlpha (0.35f));
        g.fillEllipse (dot.expanded (2.5f));
    }
    g.setColour (on ? onColour.brighter (0.3f) : outline.brighter (0.3f));
    g.fillEllipse (dot);

    g.setColour (on ? value : (highlighted ? value : label));
    g.setFont (font (11.5f, 0.06f));
    g.drawText (button.getButtonText(), r.withTrimmedLeft (radius + 8.0f).withTrimmedRight (radius * 0.6f),
                juce::Justification::centred, false);
}

//==============================================================================
void ScorchLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                     int, int, int, int, juce::ComboBox& box)
{
    using namespace palette;

    const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);

    g.setColour (panel);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (box.isMouseOver (true) ? labelDim : outline);
    g.drawRoundedRectangle (r, 5.0f, 1.0f);

    // small down arrow
    const float cx = r.getRight() - 13.0f;
    const float cy = r.getCentreY();
    juce::Path arrow;
    arrow.addTriangle (cx - 3.5f, cy - 1.8f, cx + 3.5f, cy - 1.8f, cx, cy + 2.6f);
    g.setColour (label);
    g.fillPath (arrow);
}

juce::Font ScorchLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return font (13.0f, 0.04f);
}

void ScorchLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& lbl)
{
    lbl.setFont (getComboBoxFont (box));
    lbl.setBounds (box.getLocalBounds().withTrimmedLeft (22).withTrimmedRight (22));
    lbl.setJustificationType (juce::Justification::centred);
}

void ScorchLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (palette::panel);
    g.setColour (palette::outline);
    g.drawRect (juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height), 1.0f);
}

juce::Font ScorchLookAndFeel::getPopupMenuFont()
{
    return font (14.0f, 0.03f);
}
} // namespace scorch
