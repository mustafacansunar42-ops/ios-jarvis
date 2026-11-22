#include "FrierLookAndFeel.h"
#include "Palette.h"

namespace frier
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
} // namespace

FrierLookAndFeel::FrierLookAndFeel()
{
    using namespace palette;

    setColour (juce::ResizableWindow::backgroundColourId, background);
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, textDim);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, gold.withAlpha (0.16f));
    setColour (juce::PopupMenu::highlightedTextColourId, goldBright);
    setColour (juce::ComboBox::textColourId, cream);
    setColour (juce::ComboBox::backgroundColourId, panel);
    setColour (juce::ComboBox::outlineColourId, panelBorder);
    setColour (juce::ComboBox::arrowColourId, gold);
    setColour (juce::ToggleButton::textColourId, textDim);
    setColour (juce::ToggleButton::tickColourId, gold);
    setColour (juce::TextButton::textColourOffId, textDim);
    setColour (juce::TextButton::textColourOnId, goldBright);
    setColour (juce::TooltipWindow::backgroundColourId, panel);
    setColour (juce::TooltipWindow::textColourId, cream);
}

juce::Font FrierLookAndFeel::font (float height, float tracking, bool bold)
{
    return juce::Font (juce::FontOptions()
                           .withHeight (height)
                           .withKerningFactor (tracking)
                           .withStyle (bold ? "Bold" : "Regular"));
}

//==============================================================================
void FrierLookAndFeel::drawKnob (juce::Graphics& g, juce::Rectangle<float> bounds, float proportion,
                                 bool bipolar, bool highlighted)
{
    using namespace palette;

    const float diameter = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const auto centre = bounds.getCentre();
    const float radius = diameter * 0.5f - 6.0f;   // leaves room for the value arc
    const auto body = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre);

    // Soft drop shadow below the knob.
    {
        juce::ColourGradient shadow (juce::Colours::black.withAlpha (0.55f), centre.translated (0.0f, radius * 0.35f),
                                     juce::Colours::transparentBlack, centre.translated (0.0f, radius * 0.35f + radius * 1.25f), true);
        g.setGradientFill (shadow);
        g.fillEllipse (body.expanded (radius * 0.35f).translated (0.0f, radius * 0.3f));
    }

    // Value track + arc (thin, low-key: the reference shows bare knobs).
    {
        const float arcRadius = radius + 4.0f;
        juce::Path track;
        track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
        g.setColour (divider);
        g.strokePath (track, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float valueAngle = startAngle + proportion * (endAngle - startAngle);
        const float fromAngle = bipolar ? 0.0f : startAngle;

        if (std::abs (valueAngle - fromAngle) > 0.001f)
        {
            juce::Path arc;
            arc.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                               juce::jmin (fromAngle, valueAngle), juce::jmax (fromAngle, valueAngle), true);
            g.setColour (gold.withAlpha (highlighted ? 0.95f : 0.65f));
            g.strokePath (arc, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    // Domed body: light from the top-left, like the reference.
    {
        juce::ColourGradient dome (knobTop, body.getX() + radius * 0.55f, body.getY() + radius * 0.35f,
                                   knobBottom, body.getRight() - radius * 0.2f, body.getBottom(), true);
        g.setGradientFill (dome);
        g.fillEllipse (body);

        // inner bevel
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawEllipse (body.reduced (radius * 0.14f), 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.035f));
        g.drawEllipse (body.reduced (radius * 0.14f + 1.0f), 1.0f);
    }

    // Thin bright rim, brighter on top.
    {
        juce::ColourGradient rim (knobRim.withAlpha (highlighted ? 0.95f : 0.75f), centre.x, body.getY(),
                                  knobRim.withAlpha (0.18f), centre.x, body.getBottom(), false);
        g.setGradientFill (rim);
        g.drawEllipse (body.reduced (0.6f), 1.3f);
    }

    // Pointer: slim cream line toward the rim.
    {
        const float angle = startAngle + proportion * (endAngle - startAngle);
        const auto a = pointOnCircle (centre, radius * 0.38f, angle);
        const auto b = pointOnCircle (centre, radius * 0.86f, angle);

        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawLine ({ a.translated (0.0f, 1.0f), b.translated (0.0f, 1.0f) }, 2.6f);
        g.setColour (highlighted ? goldBright : cream);
        g.drawLine ({ a, b }, juce::jmax (1.6f, radius * 0.06f));
    }
}

void FrierLookAndFeel::drawLed (juce::Graphics& g, juce::Rectangle<float> area, bool on, juce::Colour onColour)
{
    if (on)
    {
        g.setColour (onColour.withAlpha (0.22f));
        g.fillRoundedRectangle (area.expanded (2.5f), 3.5f);
        g.setColour (onColour);
        g.fillRoundedRectangle (area, 2.0f);
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.fillRoundedRectangle (area.withHeight (area.getHeight() * 0.4f).reduced (1.0f, 0.5f), 1.0f);
    }
    else
    {
        g.setColour (palette::ledOff);
        g.fillRoundedRectangle (area, 2.0f);
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.drawRoundedRectangle (area.reduced (0.5f), 2.0f, 1.0f);
    }
}

//==============================================================================
void FrierLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool)
{
    const auto bounds = button.getLocalBounds().toFloat();
    const float ledSize = juce::jmin (10.0f, bounds.getHeight() - 4.0f);
    const auto led = juce::Rectangle<float> (ledSize, ledSize).withCentre ({ bounds.getRight() - ledSize * 0.5f - 2.0f,
                                                                              bounds.getCentreY() });

    drawLed (g, led, button.getToggleState(), button.findColour (juce::ToggleButton::tickColourId));

    g.setColour (highlighted ? palette::cream : button.findColour (juce::ToggleButton::textColourId));
    g.setFont (font (11.0f, 0.22f));
    g.drawText (button.getButtonText(), bounds.withRight (led.getX() - 7.0f), juce::Justification::centredRight, false);
}

void FrierLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                             bool highlighted, bool down)
{
    const auto r = button.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = button.getToggleState();

    g.setColour (on ? palette::gold.withAlpha (0.16f) : palette::panel.withAlpha (0.8f));
    g.fillRoundedRectangle (r, 4.0f);

    if (down)
    {
        g.setColour (juce::Colours::black.withAlpha (0.3f));
        g.fillRoundedRectangle (r, 4.0f);
    }

    g.setColour (on ? palette::gold.withAlpha (0.85f) : (highlighted ? palette::textDim : palette::panelBorder));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

void FrierLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool highlighted, bool)
{
    const bool on = button.getToggleState();
    g.setColour (on ? palette::goldBright : (highlighted ? palette::cream : palette::textDim));
    g.setFont (getTextButtonFont (button, button.getHeight()));
    g.drawText (button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, false);
}

juce::Font FrierLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return font (juce::jmin (12.0f, (float) buttonHeight * 0.5f), 0.2f);
}

//==============================================================================
void FrierLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                     int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);

    if ((bool) box.getProperties()[flatComboProperty])
        return;   // the parent draws the frame (preset bar)

    g.setColour (palette::panel);
    g.fillRoundedRectangle (r.reduced (0.5f), 3.0f);
    g.setColour (box.isMouseOver (true) ? palette::textDim : palette::panelBorder);
    g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);

    // up/down arrows, as on the reference's "Poly" selector
    const float cx = r.getRight() - 11.0f;
    const float cy = r.getCentreY();
    juce::Path arrows;
    arrows.addTriangle (cx - 3.0f, cy - 1.5f, cx + 3.0f, cy - 1.5f, cx, cy - 5.0f);
    arrows.addTriangle (cx - 3.0f, cy + 1.5f, cx + 3.0f, cy + 1.5f, cx, cy + 5.0f);
    g.setColour (palette::gold);
    g.fillPath (arrows);
}

juce::Font FrierLookAndFeel::getComboBoxFont (juce::ComboBox& box)
{
    if ((bool) box.getProperties()[flatComboProperty])
        return font (13.0f, 0.18f);

    return font (11.5f, 0.12f);
}

void FrierLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setFont (getComboBoxFont (box));

    if ((bool) box.getProperties()[flatComboProperty])
    {
        label.setBounds (box.getLocalBounds());
        label.setJustificationType (juce::Justification::centred);
    }
    else
    {
        label.setBounds (box.getLocalBounds().withTrimmedLeft (4).withTrimmedRight (20));
        label.setJustificationType (juce::Justification::centredLeft);
    }
}

//==============================================================================
void FrierLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);
    g.fillAll (palette::panel);
    g.setColour (palette::panelBorder);
    g.drawRect (r, 1.0f);
}

juce::Font FrierLookAndFeel::getPopupMenuFont()
{
    return font (14.0f, 0.1f);
}
} // namespace frier
