#include "LevelMeter.h"
#include "FrierLookAndFeel.h"
#include "Palette.h"

namespace frier
{
namespace
{
constexpr float minDb = -60.0f;
constexpr float maxDb = 6.0f;
constexpr int refreshHz = 30;
constexpr float fallDbPerFrame = 26.0f / (float) refreshHz;   // 26 dB/s release
constexpr int segments = 44;
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

    g.setFont (FrierLookAndFeel::font (10.0f, 0.25f));
    g.setColour (palette::textDim);
    g.drawText (title, bounds.removeFromTop (14.0f), juce::Justification::centred, false);

    auto labels = bounds.removeFromBottom (14.0f);
    bounds.removeFromTop (6.0f);
    bounds.removeFromBottom (6.0f);

    const float columnGap = 4.0f;
    const float columnWidth = (bounds.getWidth() - columnGap) * 0.5f;
    const float segmentPitch = bounds.getHeight() / (float) segments;
    const float zeroDb = dbToProportion (0.0f);
    const float warnDb = dbToProportion (-12.0f);

    for (int ch = 0; ch < 2; ++ch)
    {
        const auto column = juce::Rectangle<float> (bounds.getX() + (float) ch * (columnWidth + columnGap),
                                                    bounds.getY(), columnWidth, bounds.getHeight());

        const float level = dbToProportion (levelDb[(size_t) ch]);
        const float hold = dbToProportion (holdDb[(size_t) ch]);
        const int holdSegment = juce::jlimit (-1, segments - 1, (int) std::ceil (hold * (float) segments) - 1);

        for (int s = 0; s < segments; ++s)
        {
            const float pos = ((float) s + 0.5f) / (float) segments;
            const auto seg = juce::Rectangle<float> (column.getX(), column.getBottom() - (float) (s + 1) * segmentPitch,
                                                     column.getWidth(), segmentPitch - 1.0f);

            const auto colour = pos > zeroDb ? palette::hot : (pos > warnDb ? palette::amber : palette::gold);
            const bool lit = pos <= level;

            if (lit || s == holdSegment)
                g.setColour (lit ? colour : colour.withAlpha (0.7f));
            else
                g.setColour (palette::divider.withAlpha (0.8f));

            g.fillRect (seg);
        }
    }

    g.setColour (palette::textDim);
    g.drawText ("L", labels.withWidth (columnWidth + columnGap * 0.5f), juce::Justification::centred, false);
    g.drawText ("R", labels.withTrimmedLeft (columnWidth + columnGap * 0.5f), juce::Justification::centred, false);
}
} // namespace frier
