#include "LevelMeter.h"
#include "ScorchLookAndFeel.h"
#include "Palette.h"

namespace scorch
{
namespace
{
constexpr float minDb = -60.0f;
constexpr float maxDb = 6.0f;
constexpr int refreshHz = 30;
constexpr float fallDbPerFrame = 26.0f / (float) refreshHz;   // 26 dB/s release
constexpr int segments = 52;
constexpr float titleWidth = 34.0f;
} // namespace

LevelMeter::LevelMeter (std::array<std::atomic<float>, 2>& src, juce::String titleText)
    : sources (src), title (std::move (titleText))
{
    setInterceptsMouseClicks (false, false);
    startTimerHz (refreshHz);
}

float LevelMeter::dbToProportion (float db) noexcept
{
    return juce::jlimit (0.0f, 1.0f, (db - minDb) / (maxDb - minDb));
}

void LevelMeter::timerCallback()
{
    bool changed = false;

    for (size_t ch = 0; ch < 2; ++ch)
    {
        const float peak = sources[ch].exchange (0.0f);
        const float db = juce::Decibels::gainToDecibels (peak, -100.0f);
        const float next = juce::jmax (db, levelDb[ch] - fallDbPerFrame);

        if (next >= holdDb[ch])
        {
            holdDb[ch] = next;
            holdFrames[ch] = refreshHz;   // hold for 1 s
        }
        else if (--holdFrames[ch] < 0)
        {
            holdDb[ch] -= fallDbPerFrame;
        }

        changed = changed || std::abs (next - levelDb[ch]) > 0.05f || holdDb[ch] > minDb;
        levelDb[ch] = next;
    }

    if (changed)
        repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setFont (ScorchLookAndFeel::font (11.0f, 0.06f));
    g.setColour (palette::label);
    g.drawText (title, bounds.removeFromLeft (titleWidth), juce::Justification::centredLeft, false);

    const float rowGap = 3.0f;
    const float rowHeight = (bounds.getHeight() - rowGap) * 0.5f;
    const float pitch = bounds.getWidth() / (float) segments;
    const float zeroDb = dbToProportion (0.0f);
    const float warnDb = dbToProportion (-12.0f);

    for (int ch = 0; ch < 2; ++ch)
    {
        const float y = bounds.getY() + (float) ch * (rowHeight + rowGap);
        const float level = dbToProportion (levelDb[(size_t) ch]);
        const float hold = dbToProportion (holdDb[(size_t) ch]);
        const int holdSegment = juce::jlimit (-1, segments - 1, (int) std::ceil (hold * (float) segments) - 1);

        for (int s = 0; s < segments; ++s)
        {
            const float pos = ((float) s + 0.5f) / (float) segments;
            const auto seg = juce::Rectangle<float> (bounds.getX() + (float) s * pitch, y, pitch - 1.5f, rowHeight);
            const auto colour = pos > zeroDb ? palette::hot : (pos > warnDb ? palette::highlight : palette::accent);
            const bool lit = pos <= level;

            if (lit)
                g.setColour (colour);
            else if (s == holdSegment)
                g.setColour (colour.withAlpha (0.75f));
            else
                g.setColour (palette::deep.withAlpha (0.18f));

            g.fillRect (seg);
        }
    }
}
} // namespace scorch
