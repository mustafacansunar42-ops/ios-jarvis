#include "Controls.h"
#include "Theme.h"

namespace automech
{
using namespace juce;

namespace
{
void setupDefaultValue (Slider& s, APVTS& state, const String& id)
{
    if (auto* p = state.getParameter (id))
        s.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
}
} // namespace

//==============================================================================
KnobSlider::KnobSlider()
{
    setSliderStyle (Slider::RotaryVerticalDrag);
    setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    setScrollWheelEnabled (true);
}

void KnobSlider::mouseDown (const MouseEvent& e)
{
    lastY = e.position.y;
    Slider::mouseDown (e);
}

void KnobSlider::mouseDrag (const MouseEvent& e)
{
    if (! isEnabled() || e.mods.isPopupMenu())
        return;

    const float dy = lastY - e.position.y;
    lastY = e.position.y;
    const double perPixel = e.mods.isShiftDown() ? 0.0008 : 0.0055;
    const double proportion = jlimit (0.0, 1.0, valueToProportionOfLength (getValue()) + dy * perPixel);
    setValue (proportionOfLengthToValue (proportion), sendNotificationSync);
}

//==============================================================================
LabeledKnob::LabeledKnob (APVTS& state, const String& id, const String& label, bool large)
    : labelText (label.toUpperCase()), isLarge (large)
{
    addAndMakeVisible (slider);
    attachment = std::make_unique<APVTS::SliderAttachment> (state, id, slider);
    setupDefaultValue (slider, state, id);
    slider.onValueChange = [this] { repaint(); };
}

void LabeledKnob::resized()
{
    const int knob = isLarge ? 78 : 56;
    slider.setBounds (Rectangle<int> (knob, knob).withCentre ({ getWidth() / 2, knob / 2 + 2 }));
}

void LabeledKnob::paint (Graphics& g)
{
    const int top = slider.getBottom() + 2;
    g.setColour (theme::text);
    g.setFont (theme::font (isLarge ? 12.0f : 11.0f, true, 0.06f));
    g.drawText (labelText, 0, top, getWidth(), 15, Justification::centred);

    g.setColour (theme::text.withAlpha (0.92f));
    g.setFont (theme::font (isLarge ? 13.0f : 12.0f, true));
    g.drawText (slider.getTextFromValue (slider.getValue()), 0, top + 16, getWidth(), 16, Justification::centred);
}

//==============================================================================
LabeledCombo::LabeledCombo (APVTS& state, const String& id, const String& label, const StringArray& items)
    : labelText (label.toUpperCase())
{
    combo.addItemList (items, 1);
    addAndMakeVisible (combo);
    attachment = std::make_unique<APVTS::ComboBoxAttachment> (state, id, combo);
}

void LabeledCombo::resized()
{
    combo.setBounds (getLocalBounds().withTrimmedTop (16).withHeight (jmin (34, getHeight() - 16)));
}

void LabeledCombo::paint (Graphics& g)
{
    g.setColour (theme::label.withAlpha (0.85f));
    g.setFont (theme::font (9.5f, true, 0.14f));
    g.drawText (labelText, 2, 0, getWidth(), 13, Justification::centredLeft);
}

//==============================================================================
LabeledToggle::LabeledToggle (APVTS& state, const String& id, const String& label)
    : labelText (label.toUpperCase())
{
    addAndMakeVisible (toggle);
    attachment = std::make_unique<APVTS::ButtonAttachment> (state, id, toggle);
}

void LabeledToggle::resized()
{
    toggle.setBounds (getLocalBounds().withHeight (30).withSizeKeepingCentre (54, 30).withY (getHeight() / 2 - 26));
}

void LabeledToggle::paint (Graphics& g)
{
    g.setColour (theme::text);
    g.setFont (theme::font (11.0f, true, 0.08f));
    g.drawFittedText (labelText, 0, toggle.getBottom() + 6, getWidth(), 15, Justification::centred, 1);
}

//==============================================================================
LabeledHSlider::LabeledHSlider (APVTS& state, const String& id, const String& label)
    : labelText (label.toUpperCase())
{
    slider.setSliderStyle (Slider::LinearHorizontal);
    slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible (slider);
    attachment = std::make_unique<APVTS::SliderAttachment> (state, id, slider);
    setupDefaultValue (slider, state, id);
    slider.onValueChange = [this] { repaint(); };
}

void LabeledHSlider::resized()
{
    slider.setBounds (0, getHeight() / 2 - 6, getWidth(), 26);
}

void LabeledHSlider::paint (Graphics& g)
{
    const int y = slider.getY() - 14;
    g.setColour (theme::text);
    g.setFont (theme::font (11.0f, true, 0.08f));
    g.drawText (labelText, 0, y, getWidth(), 14, Justification::centredLeft);
    g.drawText (slider.getTextFromValue (slider.getValue()), 0, y, getWidth(), 14, Justification::centredRight);
}

//==============================================================================
InfoText::InfoText (String t, std::function<String()> fn) : title (std::move (t)), valueFn (std::move (fn)) {}

void InfoText::refresh()
{
    const auto v = valueFn ? valueFn() : String();
    if (v != lastValue) { lastValue = v; repaint(); }
}

void InfoText::paint (Graphics& g)
{
    g.setColour (theme::label.withAlpha (0.85f));
    g.setFont (theme::font (9.5f, true, 0.14f));
    g.drawText (title.toUpperCase(), 0, getHeight() / 2 - 22, getWidth(), 13, Justification::centredLeft);
    g.setColour (theme::text);
    g.setFont (theme::font (18.0f, true));
    g.drawText (lastValue, 0, getHeight() / 2 - 6, getWidth(), 24, Justification::centredLeft);
}

//==============================================================================
IconButton::IconButton (const String& name, IconFn fn, bool round)
    : Button (name), iconFn (std::move (fn)), isRound (round) {}

void IconButton::paintButton (Graphics& g, bool over, bool down)
{
    const auto b = getLocalBounds().toFloat().reduced (0.5f);
    auto base = getToggleState() ? Colour (0xffF4D6D7) : theme::controlDark;
    if (over) base = base.brighter (0.12f);
    if (down) base = base.darker (0.12f);

    g.setGradientFill (ColourGradient (base.brighter (0.1f), 0.0f, b.getY(), base.darker (0.15f), 0.0f, b.getBottom(), false));
    if (isRound) g.fillEllipse (b); else g.fillRoundedRectangle (b, 5.0f);
    g.setColour (Colours::white.withAlpha (0.14f));
    if (isRound) g.drawEllipse (b, 1.0f); else g.drawRoundedRectangle (b, 5.0f, 1.0f);

    if (iconFn) iconFn (g, b, getToggleState());
}

//==============================================================================
TabStrip::TabStrip (StringArray n) : names (std::move (n)) {}

void TabStrip::setCurrent (int index)
{
    if (index == current) return;
    current = index;
    repaint();
    if (onChange) onChange (current);
}

void TabStrip::mouseDown (const MouseEvent& e)
{
    const int count = names.size();
    const int index = jlimit (0, count - 1, (int) (e.position.x / ((float) getWidth() / (float) count)));
    setCurrent (index);
}

void TabStrip::paint (Graphics& g)
{
    const int count = names.size();
    const float w = (float) getWidth() / (float) count;

    for (int i = 0; i < count; ++i)
    {
        auto r = Rectangle<float> (w * (float) i, 0.0f, w, (float) getHeight()).reduced (3.0f, 0.0f);
        const bool active = i == current;

        g.setColour (active ? theme::bgTop.brighter (0.15f) : theme::controlDark.withAlpha (0.9f));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (Colours::white.withAlpha (active ? 0.22f : 0.1f));
        g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);

        g.setColour (active ? theme::text : theme::label.withAlpha (0.8f));
        g.setFont (theme::font (11.0f, true, 0.12f));
        g.drawText (names[i], r, Justification::centred);

        if (active)
        {
            g.setColour (Colours::white);
            g.fillRoundedRectangle (r.getCentreX() - 22.0f, r.getBottom() - 4.0f, 44.0f, 2.5f, 1.2f);
        }
    }
}

//==============================================================================
void TabPage::add (std::unique_ptr<Component> c, int width)
{
    addAndMakeVisible (*c);
    items.push_back ({ std::move (c), width });
}

void TabPage::addGap (int width)
{
    items.push_back ({ nullptr, width });
}

void TabPage::refreshInfo()
{
    for (auto& item : items)
        if (auto* info = dynamic_cast<InfoText*> (item.comp.get()))
            info->refresh();
}

void TabPage::resized()
{
    int x = 18;
    for (auto& item : items)
    {
        if (item.comp != nullptr)
            item.comp->setBounds (x, 4, item.width, getHeight() - 8);
        x += item.width + 16;
    }
}
} // namespace automech
