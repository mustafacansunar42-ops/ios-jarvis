#include "PitchDisplay.h"
#include "Theme.h"
#include "../Parameters.h"

namespace automech
{
using namespace juce;

PitchDisplay::PitchDisplay()
{
    setOpaque (false);
}

void PitchDisplay::update (const DisplayState& s)
{
    state = s;
    visualizer.tick (s);

    if (s.voiced)
    {
        const int nearest = (int) std::lround (s.outputMidi);
        const float cents = (s.detectedMidi - (s.targetNote >= 0.0f ? s.targetNote : (float) nearest)) * 100.0f;
        needleCents += (jlimit (-50.0f, 50.0f, cents) - needleCents) * 0.35f;

        const int shown = s.targetNote >= 0.0f ? (int) s.targetNote : nearest;
        noteName = keyNames()[((shown % 12) + 12) % 12];
        octave = String (shown / 12 - 1);
        const int c = (int) std::lround (cents);
        centsText = (c > 0 ? "+" : "") + String (c) + String::fromUTF8 (" \xc2\xa2");
        hzText = String (s.detectedHz, 1) + " Hz";
    }
    else
    {
        needleCents *= 0.85f;
    }

    repaint();
}

Rectangle<float> PitchDisplay::pixelToggleArea() const
{
    return { (float) getWidth() - 74.0f, (float) getHeight() - 26.0f, 56.0f, 18.0f };
}

Rectangle<float> PitchDisplay::modeArea() const
{
    return { (float) getWidth() - 160.0f, (float) getHeight() - 26.0f, 82.0f, 18.0f };
}

void PitchDisplay::mouseDown (const MouseEvent& e)
{
    if (pixelToggleArea().contains (e.position))
        visualizerOn = ! visualizerOn;
    else if (visualizerOn && modeArea().contains (e.position))
        visualizer.cycleMode();
    else if (visualizerOn)
        visualizer.cycleMode();
    repaint();
}

void PitchDisplay::paint (Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float radius = 10.0f;

    // Bezel + black glass
    g.setColour (Colour (0xff5A060A));
    g.fillRoundedRectangle (bounds, radius + 2.0f);
    const auto glass = bounds.reduced (3.0f);
    g.setGradientFill (ColourGradient (Colour (0xff140A0B), 0.0f, glass.getY(), theme::display, 0.0f, glass.getBottom(), false));
    g.fillRoundedRectangle (glass, radius);

    {
        Graphics::ScopedSaveState save (g);
        Path clip;
        clip.addRoundedRectangle (glass, radius);
        g.reduceClipRegion (clip);

        if (visualizerOn)
            visualizer.draw (g, glass.reduced (10.0f, 12.0f));

        // Fade behind the note readout so it stays legible.
        g.setGradientFill (ColourGradient (theme::display.withAlpha (0.92f), glass.getX(), 0.0f,
                                           theme::display.withAlpha (0.0f), glass.getX() + 300.0f, 0.0f, false));
        g.fillRect (glass.withWidth (300.0f));

        // Glass reflection
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.06f), 0.0f, glass.getY(),
                                           Colours::white.withAlpha (0.0f), 0.0f, glass.getCentreY(), false));
        g.fillRect (glass.withHeight (glass.getHeight() * 0.5f));
    }

    // Deviation scale and needle
    const float cx = glass.getCentreX() + 40.0f;
    const float halfW = glass.getWidth() * 0.22f;
    const float cy = glass.getCentreY();
    g.setColour (Colours::white.withAlpha (0.16f));
    g.drawHorizontalLine ((int) cy, cx - halfW, cx + halfW);
    for (int i = -5; i <= 5; ++i)
    {
        const float x = cx + halfW * (float) i / 5.0f;
        const float h = i == 0 ? 14.0f : (i % 5 == 0 ? 10.0f : 6.0f);
        g.setColour (Colours::white.withAlpha (i == 0 ? 0.45f : 0.2f));
        g.fillRect (x - 0.5f, cy - h * 0.5f, 1.0f, h);
    }

    const float nx = cx + halfW * needleCents / 50.0f;
    g.setColour (Colours::white.withAlpha (0.55f));
    g.fillRect (nx - 0.75f, glass.getY() + 22.0f, 1.5f, glass.getHeight() - 44.0f);
    const auto capsule = Rectangle<float> (10.0f, 28.0f).withCentre ({ nx, cy });
    Path cap;
    cap.addRoundedRectangle (capsule, 5.0f);
    DropShadow (Colours::white.withAlpha (state.locked ? 0.45f : 0.2f), 10, {}).drawForPath (g, cap);
    g.setColour (Colours::white);
    g.fillPath (cap);

    // Note readout
    const float tx = glass.getX() + 26.0f;
    const auto bigFont = theme::font (78.0f, false);
    g.setColour (Colours::white.withAlpha (state.voiced ? 1.0f : 0.55f));
    g.setFont (bigFont);
    const float noteW = GlyphArrangement::getStringWidth (bigFont, noteName);
    g.drawText (noteName, Rectangle<float> (tx, glass.getY() + 14.0f, noteW + 10.0f, 84.0f), Justification::centredLeft);

    g.setFont (theme::font (20.0f));
    g.drawText (octave, Rectangle<float> (tx + noteW + 4.0f, glass.getY() + 24.0f, 30.0f, 22.0f), Justification::centredLeft);

    const float infoX = tx + noteW + 40.0f;
    g.setColour (Colours::white);
    g.setFont (theme::font (19.0f, true));
    g.drawText (centsText, Rectangle<float> (infoX, glass.getY() + 50.0f, 120.0f, 22.0f), Justification::centredLeft);
    g.setColour (Colours::white.withAlpha (0.5f));
    g.setFont (theme::font (13.0f, true));
    g.drawText (hzText, Rectangle<float> (infoX, glass.getY() + 72.0f, 120.0f, 18.0f), Justification::centredLeft);

    // Status
    const auto statusFont = theme::font (10.5f, true, 0.18f);
    g.setFont (statusFont);
    const auto statusArea = Rectangle<float> (glass.getRight() - 220.0f, glass.getY() + 12.0f, 200.0f, 16.0f);
    const float lockedW = GlyphArrangement::getStringWidth (statusFont, "LOCKED");
    g.setColour (Colours::white.withAlpha (state.locked ? 1.0f : 0.3f));
    g.drawText ("LOCKED", statusArea, Justification::centredRight);
    g.setColour (Colours::white.withAlpha (0.35f));
    g.drawText (String::fromUTF8 ("\xc2\xb7"), statusArea.withTrimmedRight (lockedW + 6.0f), Justification::centredRight);
    g.setColour (Colours::white.withAlpha (state.voiced ? 0.75f : 0.3f));
    g.drawText ("TRACKING", statusArea.withTrimmedRight (lockedW + 18.0f), Justification::centredRight);

    // Visualizer controls
    g.setFont (theme::font (9.5f, true, 0.18f));
    const auto pa = pixelToggleArea();
    g.setColour (visualizerOn ? theme::bgTop : Colours::white.withAlpha (0.12f));
    g.fillRect (pa.reduced (2.0f));
    g.setColour (Colours::white.withAlpha (visualizerOn ? 1.0f : 0.5f));
    g.drawText ("PIXEL", pa, Justification::centred);
    if (visualizerOn)
    {
        g.setColour (Colours::white.withAlpha (0.45f));
        g.drawText (PixelVisualizer::modeName (visualizer.getMode()), modeArea(), Justification::centredRight);
    }
}
} // namespace automech
