#include "PianoKeyboard.h"
#include "Theme.h"

namespace automech
{
using namespace juce;

bool PianoKeyboard::isBlack (int s) noexcept
{
    const int pc = s % 12;
    return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
}

void PianoKeyboard::setState (uint16_t newMask, int detectedPc)
{
    if (newMask != mask || detectedPc != detected)
    {
        mask = newMask;
        detected = detectedPc;
        repaint();
    }
}

Rectangle<float> PianoKeyboard::keyBounds (int s) const
{
    int numWhite = 0;
    for (int i = 0; i < kNumSemitones; ++i) if (! isBlack (i)) ++numWhite;

    const float ww = (float) getWidth() / (float) numWhite;
    int whiteIndex = 0;
    for (int i = 0; i < s; ++i) if (! isBlack (i)) ++whiteIndex;

    if (! isBlack (s))
        return { ww * (float) whiteIndex, 0.0f, ww, (float) getHeight() };

    const float bw = ww * 0.58f;
    return { ww * (float) whiteIndex - bw * 0.5f, 0.0f, bw, (float) getHeight() * 0.6f };
}

int PianoKeyboard::keyAt (Point<float> p) const
{
    for (int s = 0; s < kNumSemitones; ++s)
        if (isBlack (s) && keyBounds (s).contains (p)) return s;
    for (int s = 0; s < kNumSemitones; ++s)
        if (! isBlack (s) && keyBounds (s).contains (p)) return s;
    return -1;
}

void PianoKeyboard::mouseDown (const MouseEvent& e)
{
    const int s = keyAt (e.position);
    if (s >= 0 && onToggle) onToggle (s % 12);
}

void PianoKeyboard::paint (Graphics& g)
{
    auto drawKey = [&] (int s)
    {
        const bool black = isBlack (s);
        const bool on = enabled (s);
        const bool hit = detected == s % 12;
        auto r = keyBounds (s);
        if (! black) r = r.reduced (1.0f, 0.0f);
        const float radius = black ? 3.0f : 5.0f;

        Path key;
        key.addRoundedRectangle (r.getX(), r.getY(), r.getWidth(), r.getHeight(), radius, radius, false, false, true, true);

        if (black)
        {
            DropShadow (Colours::black.withAlpha (0.45f), 6, { 1, 3 }).drawForPath (g, key);
            const auto base = on ? theme::blackKey : theme::blackKey.darker (0.35f);
            g.setGradientFill (ColourGradient (base.brighter (0.35f), r.getX(), r.getY(), base.darker (0.3f), r.getX(), r.getBottom(), false));
            g.fillPath (key);
            g.setColour (Colours::white.withAlpha (0.12f));
            g.fillRoundedRectangle (r.reduced (r.getWidth() * 0.18f, 0.0f).withHeight (r.getHeight() * 0.55f).translated (0.0f, 3.0f), 2.0f);
        }
        else
        {
            const auto base = on ? theme::whiteKey : Colour (0xffCDB4B3);
            g.setGradientFill (ColourGradient (base, r.getX(), r.getY(), base.darker (0.08f), r.getX(), r.getBottom(), false));
            g.fillPath (key);
            // pink-red shadow at the bottom edge
            g.setGradientFill (ColourGradient (Colour (0x00E0262E), r.getX(), r.getBottom() - 22.0f,
                                               Colour (0x55E0262E), r.getX(), r.getBottom(), false));
            g.fillRect (r.withTop (r.getBottom() - 22.0f));
            g.setColour (Colour (0xff8E0C13).withAlpha (0.55f));
            g.strokePath (key, PathStrokeType (1.0f));
        }

        // Soft glow on scale notes, stronger red-pink glow on the detected note.
        if (on || hit)
        {
            const auto glowColour = hit ? Colour (0xffFF6B70) : Colours::white;
            const float strength = hit ? 0.75f : (black ? 0.18f : 0.55f);
            Graphics::ScopedSaveState save (g);
            g.reduceClipRegion (key);
            g.setGradientFill (ColourGradient (glowColour.withAlpha (strength), r.getCentreX(), r.getBottom() - r.getHeight() * 0.3f,
                                               glowColour.withAlpha (0.0f), r.getCentreX() + r.getWidth() * 1.1f, r.getBottom() - r.getHeight() * 0.3f, true));
            g.fillRect (r);
        }

        // Enable indicator
        const auto ind = Rectangle<float> (r.getWidth() * 0.36f, 6.0f).withCentre ({ r.getCentreX(), r.getBottom() - 14.0f });
        if (hit)           g.setColour (Colour (0xffFF3B3B));
        else if (on)       g.setColour (black ? theme::label : theme::bgTop);
        else               g.setColour (black ? Colour (0xff2A0306) : Colour (0xff4A0609));
        g.fillRoundedRectangle (ind, 2.0f);
    };

    for (int s = 0; s < kNumSemitones; ++s) if (! isBlack (s)) drawKey (s);
    for (int s = 0; s < kNumSemitones; ++s) if (isBlack (s)) drawKey (s);

    // Shadow under the keyboard's top rail
    g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.35f), 0.0f, 0.0f, Colours::transparentBlack, 0.0f, 10.0f, false));
    g.fillRect (0, 0, getWidth(), 10);
}
} // namespace automech
