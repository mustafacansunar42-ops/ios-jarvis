#include "AutoMechLookAndFeel.h"
#include "Theme.h"

namespace automech
{
using namespace juce;

AutoMechLookAndFeel::AutoMechLookAndFeel()
{
    setColour (ResizableWindow::backgroundColourId, theme::bgBottom);
    setColour (PopupMenu::backgroundColourId, theme::knobRim);
    setColour (PopupMenu::textColourId, theme::text);
    setColour (PopupMenu::highlightedBackgroundColourId, theme::bgTop);
    setColour (PopupMenu::highlightedTextColourId, theme::text);
    setColour (ComboBox::textColourId, theme::text);
    setColour (ComboBox::arrowColourId, theme::text);
    setColour (ComboBox::backgroundColourId, theme::control);
    setColour (ComboBox::outlineColourId, Colours::transparentBlack);
    setColour (Label::textColourId, theme::text);
    setColour (TextButton::buttonColourId, theme::control);
    setColour (TextButton::textColourOffId, theme::text);
    setColour (TextButton::textColourOnId, theme::accentRed);
    setColour (AlertWindow::backgroundColourId, theme::panel);
    setColour (AlertWindow::textColourId, theme::text);
    setColour (AlertWindow::outlineColourId, theme::panelEdge);
}

//==============================================================================
void AutoMechLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                            float startAngle, float endAngle, Slider& slider)
{
    const float size = (float) jmin (w, h);
    const auto centre = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).getCentre();
    const float r = size * 0.5f - jmax (5.0f, size * 0.11f);
    const float alpha = slider.isEnabled() ? 1.0f : 0.5f;

    // Faint tick ring
    g.setColour (theme::label.withAlpha (0.22f * alpha));
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startAngle + (endAngle - startAngle) * (float) i / 10.0f;
        const auto p1 = centre.getPointOnCircumference (r + 3.0f, a);
        const auto p2 = centre.getPointOnCircumference (r + (i % 5 == 0 ? 7.0f : 5.5f), a);
        g.drawLine ({ p1, p2 }, 1.2f);
    }

    Path circle;
    circle.addEllipse (centre.x - r, centre.y - r, r * 2.0f, r * 2.0f);

    // Soft drop shadow under the knob
    DropShadow (Colours::black.withAlpha (0.45f), (int) (r * 0.45f), { 0, (int) (r * 0.2f) }).drawForPath (g, circle);

    // Darker rim / bevel
    g.setGradientFill (ColourGradient (theme::knobRim.brighter (0.35f), centre.x, centre.y - r,
                                       theme::knobRim.darker (0.5f), centre.x, centre.y + r, false));
    g.fillPath (circle);

    // Glossy dome: lit from the top-left
    const float br = r * 0.87f;
    ColourGradient body (theme::knobHighlight, centre.x - br * 0.45f, centre.y - br * 0.55f,
                         theme::knobBody.darker (0.5f), centre.x + br * 0.65f, centre.y + br * 0.85f, true);
    body.addColour (0.45, theme::knobBody);
    g.setGradientFill (body);
    g.fillEllipse (centre.x - br, centre.y - br, br * 2.0f, br * 2.0f);

    g.setColour (Colours::white.withAlpha (0.14f));
    g.drawEllipse (centre.x - br, centre.y - br, br * 2.0f, br * 2.0f, 1.0f);

    // Specular highlight
    const auto specCentre = Point<float> (centre.x - br * 0.32f, centre.y - br * 0.45f);
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.30f), specCentre.x, specCentre.y,
                                       Colours::white.withAlpha (0.0f), specCentre.x + br * 0.55f, specCentre.y, true));
    g.fillEllipse (specCentre.x - br * 0.55f, specCentre.y - br * 0.4f, br * 1.1f, br * 0.8f);

    // White pointer line
    const float angle = startAngle + pos * (endAngle - startAngle);
    const auto inner = centre.getPointOnCircumference (br * 0.2f, angle);
    const auto outer = centre.getPointOnCircumference (br * 0.8f, angle);
    const float thickness = jmax (2.2f, br * 0.12f);

    Path pointer;
    pointer.startNewSubPath (inner);
    pointer.lineTo (outer);
    const PathStrokeType stroke (thickness, PathStrokeType::curved, PathStrokeType::rounded);

    g.setColour (Colours::black.withAlpha (0.25f * alpha));
    g.strokePath (pointer, stroke, AffineTransform::translation (0.0f, 1.2f));
    g.setColour (Colours::white.withAlpha (alpha));
    g.strokePath (pointer, stroke);
}

//==============================================================================
void AutoMechLookAndFeel::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos, float minPos,
                                            float maxPos, Slider::SliderStyle style, Slider& slider)
{
    if (style != Slider::LinearHorizontal)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, minPos, maxPos, style, slider);
        return;
    }

    const float cy = (float) y + (float) h * 0.5f;
    const auto track = Rectangle<float> ((float) x, cy - 1.5f, (float) w, 3.0f);

    g.setColour (Colour (0xff4E0509));
    g.fillRoundedRectangle (track, 1.5f);
    g.setColour (Colours::white.withAlpha (0.45f));
    g.fillRoundedRectangle (track.withRight (pos), 1.5f);

    const float tr = 7.5f;
    const auto thumb = Rectangle<float> (pos - tr, cy - tr, tr * 2.0f, tr * 2.0f);
    Path p;
    p.addEllipse (thumb);
    DropShadow (Colours::black.withAlpha (0.4f), 5, { 0, 2 }).drawForPath (g, p);
    g.setGradientFill (ColourGradient (Colours::white, thumb.getCentreX(), thumb.getY(),
                                       Colour (0xffE9DCDC), thumb.getCentreX(), thumb.getBottom(), false));
    g.fillPath (p);
}

//==============================================================================
void AutoMechLookAndFeel::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box)
{
    const auto b = Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (0.5f);
    const float alpha = box.isEnabled() ? 1.0f : 0.5f;

    g.setGradientFill (ColourGradient (theme::control.brighter (0.12f).withMultipliedAlpha (alpha), 0.0f, 0.0f,
                                       theme::control.darker (0.25f).withMultipliedAlpha (alpha), 0.0f, (float) h, false));
    g.fillRoundedRectangle (b, 6.0f);
    g.setColour (Colours::white.withAlpha (0.13f));
    g.drawRoundedRectangle (b, 6.0f, 1.0f);

    const float ax = (float) w - 16.0f, ay = (float) h * 0.5f;
    Path chevron;
    chevron.startNewSubPath (ax - 4.5f, ay - 2.0f);
    chevron.lineTo (ax, ay + 2.5f);
    chevron.lineTo (ax + 4.5f, ay - 2.0f);
    g.setColour (theme::text.withAlpha (alpha));
    g.strokePath (chevron, PathStrokeType (1.7f, PathStrokeType::curved, PathStrokeType::rounded));
}

Font AutoMechLookAndFeel::getComboBoxFont (ComboBox& box)
{
    return theme::font (jmin (15.0f, (float) box.getHeight() * 0.5f), true);
}

void AutoMechLookAndFeel::positionComboBoxText (ComboBox& box, Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 30, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

//==============================================================================
void AutoMechLookAndFeel::drawButtonBackground (Graphics& g, Button& button, const Colour&, bool over, bool down)
{
    const auto b = button.getLocalBounds().toFloat().reduced (0.5f);
    const bool learn = button.getProperties()["style"].toString() == "learn";
    const bool on = button.getToggleState();
    const float radius = learn ? 7.0f : 5.0f;

    if (learn || on)
    {
        Path p;
        p.addRoundedRectangle (b, radius);
        DropShadow (Colours::black.withAlpha (0.3f), 6, { 0, 2 }).drawForPath (g, p);
        auto top = Colours::white, bottom = Colour (0xffF1DADB);
        if (down) { top = top.darker (0.08f); bottom = bottom.darker (0.08f); }
        else if (over) { bottom = bottom.brighter (0.05f); }
        g.setGradientFill (ColourGradient (top, 0.0f, b.getY(), bottom, 0.0f, b.getBottom(), false));
        g.fillPath (p);
        return;
    }

    auto base = theme::controlDark;
    if (over) base = base.brighter (0.15f);
    if (down) base = base.darker (0.15f);
    g.setGradientFill (ColourGradient (base.brighter (0.1f), 0.0f, b.getY(), base.darker (0.15f), 0.0f, b.getBottom(), false));
    g.fillRoundedRectangle (b, radius);
    g.setColour (Colours::white.withAlpha (0.12f));
    g.drawRoundedRectangle (b, radius, 1.0f);
}

Font AutoMechLookAndFeel::getTextButtonFont (TextButton& button, int buttonHeight)
{
    if (button.getProperties()["style"].toString() == "learn")
        return theme::font (15.0f, true, 0.08f);
    return theme::font (jmin (12.0f, (float) buttonHeight * 0.45f), true, 0.1f);
}

void AutoMechLookAndFeel::drawButtonText (Graphics& g, TextButton& button, bool, bool)
{
    const bool learn = button.getProperties()["style"].toString() == "learn";
    const bool on = learn || button.getToggleState();
    g.setFont (getTextButtonFont (button, button.getHeight()));
    g.setColour (on ? theme::accentRed : theme::text);
    g.drawFittedText (button.getButtonText(), button.getLocalBounds().reduced (4, 0), Justification::centred, 1);
}

// Pill switch (the label is drawn by the owning component).
void AutoMechLookAndFeel::drawToggleButton (Graphics& g, ToggleButton& button, bool over, bool)
{
    const auto area = button.getLocalBounds().toFloat();
    const float h = jmin (area.getHeight(), area.getWidth() / 1.9f);
    const auto track = Rectangle<float> (h * 1.9f, h).withCentre (area.getCentre());
    const bool on = button.getToggleState();

    g.setColour (on ? Colour (0xffFF8A8E) : Colour (0xff5E070B));
    g.fillRoundedRectangle (track, h * 0.5f);
    g.setColour (Colours::black.withAlpha (0.25f));
    g.drawRoundedRectangle (track.reduced (0.5f), h * 0.5f, 1.0f);

    const float kr = h * 0.5f - 2.5f;
    const float kx = on ? track.getRight() - h * 0.5f : track.getX() + h * 0.5f;
    Path knob;
    knob.addEllipse (kx - kr, track.getCentreY() - kr, kr * 2.0f, kr * 2.0f);
    DropShadow (Colours::black.withAlpha (0.4f), 4, { 0, 1 }).drawForPath (g, knob);
    g.setColour (over ? Colours::white : Colour (0xffF7EEEE));
    g.fillPath (knob);
}

//==============================================================================
void AutoMechLookAndFeel::drawPopupMenuBackground (Graphics& g, int w, int h)
{
    g.fillAll (theme::knobRim);
    g.setColour (theme::panelEdge.withAlpha (0.5f));
    g.drawRect (0, 0, w, h, 1);
}

Font AutoMechLookAndFeel::getPopupMenuFont()
{
    return theme::font (15.0f, true);
}
} // namespace automech
