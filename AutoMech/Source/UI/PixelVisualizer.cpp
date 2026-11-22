#include "PixelVisualizer.h"
#include "Theme.h"

namespace automech
{
using namespace juce;

PixelVisualizer::PixelVisualizer()
{
    trailValid.fill (false);
}

void PixelVisualizer::cycleMode() noexcept
{
    mode = (Mode) (((int) mode + 1) % 3);
}

String PixelVisualizer::modeName (Mode m)
{
    switch (m)
    {
        case Mode::PitchTrail: return "TRAIL";
        case Mode::Spectrum:   return "SPECTRUM";
        case Mode::Scope:      return "SCOPE";
    }
    return {};
}

void PixelVisualizer::pushSamples (const float* data, int n) noexcept
{
    for (int i = 0; i < n; ++i)
    {
        history[(size_t) historyPos] = data[i];
        historyPos = (historyPos + 1) % fftSize;
    }
}

Colour PixelVisualizer::pixelColour (float f)
{
    // dark red -> theme red -> light pink -> white
    f = jlimit (0.0f, 1.0f, f);
    const Colour c0 (0xff8E0C13), c1 (0xffE0262E), c2 (0xffFFB3B6), c3 (0xffFFFFFF);
    if (f < 0.35f) return c0.interpolatedWith (c1, f / 0.35f);
    if (f < 0.75f) return c1.interpolatedWith (c2, (f - 0.35f) / 0.4f);
    return c2.interpolatedWith (c3, (f - 0.75f) / 0.25f);
}

void PixelVisualizer::tick (const DisplayState& st)
{
    ++frameCount;
    mask = st.mask;
    targetNote = st.targetNote;

    // Level of the most recent ~20 ms decides the fade in / fade out.
    float peak = 0.0f;
    for (int i = 0; i < 1024; ++i)
        peak = jmax (peak, std::abs (history[(size_t) ((historyPos - 1 - i + fftSize * 2) % fftSize)]));
    level = peak;

    const bool active = level > 0.0015f;
    fade = active ? jmin (1.0f, fade + 0.12f) : jmax (0.0f, fade - 0.012f);

    // Pitch trail scrolls one column every `trailEvery` frames.
    if (frameCount % trailEvery == 0)
    {
        for (int c = 0; c < kCols - 1; ++c)
        {
            trailDetected[(size_t) c] = trailDetected[(size_t) c + 1];
            trailOutput[(size_t) c] = trailOutput[(size_t) c + 1];
            trailValid[(size_t) c] = trailValid[(size_t) c + 1];
        }
        trailDetected[kCols - 1] = st.detectedMidi;
        trailOutput[kCols - 1] = st.outputMidi;
        trailValid[kCols - 1] = st.voiced;
    }

    if (st.voiced)
        viewCentre += ((st.targetNote >= 0.0f ? st.targetNote : st.outputMidi) - viewCentre) * 0.06f;

    // Scope auto-gain
    scopeGain += (1.0f / jmax (0.05f, level) - scopeGain) * 0.1f;

    updateSpectrum();
}

void PixelVisualizer::updateSpectrum()
{
    for (int i = 0; i < fftSize; ++i)
        fftData[(size_t) i] = history[(size_t) ((historyPos + i) % fftSize)];
    std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);

    window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
    fft.performFrequencyOnlyForwardTransform (fftData.data());

    const float minHz = 40.0f, maxHz = jmin (16000.0f, (float) sampleRate * 0.5f);
    const float binHz = (float) sampleRate / (float) fftSize;

    for (int c = 0; c < kCols; ++c)
    {
        const float lo = minHz * std::pow (maxHz / minHz, (float) c / kCols);
        const float hi = minHz * std::pow (maxHz / minHz, (float) (c + 1) / kCols);
        const int b0 = jlimit (1, fftSize / 2 - 1, (int) (lo / binHz));
        const int b1 = jlimit (b0, fftSize / 2 - 1, (int) (hi / binHz));

        float mag = 0.0f;
        for (int b = b0; b <= b1; ++b)
            mag = jmax (mag, fftData[(size_t) b]);

        const float db = Decibels::gainToDecibels (mag / (fftSize * 0.25f), -100.0f);
        const float norm = jlimit (0.0f, 1.0f, (db + 72.0f) / 72.0f);

        auto& bar = bars[(size_t) c];
        bar = norm > bar ? norm : jmax (0.0f, bar - 0.02f);

        auto& pk = peaks[(size_t) c];
        if (bar >= pk) { pk = bar; peakHold[(size_t) c] = 24; }
        else if (peakHold[(size_t) c] > 0) --peakHold[(size_t) c];
        else pk = jmax (0.0f, pk - 0.008f);
    }
}

void PixelVisualizer::draw (Graphics& g, Rectangle<float> area) const
{
    const float px = area.getWidth() / (float) kCols;
    const int rows = jmax (8, (int) (area.getHeight() / px));
    const float top = area.getY() + (area.getHeight() - px * (float) rows) * 0.5f;
    const float gap = 1.0f;

    auto cell = [&] (int col, int row, Colour c)
    {
        // row 0 = bottom
        if (col < 0 || col >= kCols || row < 0 || row >= rows) return;
        g.setColour (c);
        g.fillRect (area.getX() + (float) col * px, top + (float) (rows - 1 - row) * px, px - gap, px - gap);
    };

    // Idle pattern: sparse dim flicker while silent.
    if (fade < 1.0f)
    {
        Random rng ((int64) (frameCount / 6));
        const float a = 0.16f * (1.0f - fade);
        for (int i = 0; i < kCols / 3; ++i)
            cell (rng.nextInt (kCols), rng.nextInt (jmax (1, rows / 3)), Colour (0xffE0262E).withAlpha (a));
    }

    if (fade <= 0.001f)
        return;

    switch (mode)
    {
        case Mode::Spectrum:
        {
            for (int c = 0; c < kCols; ++c)
            {
                const int h = (int) std::round (bars[(size_t) c] * (float) rows);
                for (int r = 0; r < h; ++r)
                    cell (c, r, pixelColour ((float) r / (float) (rows - 1)).withAlpha (fade));
                const int pr = (int) std::round (peaks[(size_t) c] * (float) (rows - 1));
                if (pr > 0) cell (c, pr, Colours::white.withAlpha (fade));
            }
            break;
        }

        case Mode::Scope:
        {
            const int span = 1024, perCol = span / kCols;
            const int start = historyPos - span + fftSize * 2;
            int prevRow = -1;
            for (int c = 0; c < kCols; ++c)
            {
                float lo = 1.0f, hi = -1.0f;
                for (int i = 0; i < perCol; ++i)
                {
                    const float v = history[(size_t) ((start + c * perCol + i) % fftSize)] * scopeGain * 0.9f;
                    lo = jmin (lo, v); hi = jmax (hi, v);
                }
                const auto toRow = [&] (float v) { return jlimit (0, rows - 1, (int) std::round ((v * 0.5f + 0.5f) * (float) (rows - 1))); };
                int r0 = toRow (lo), r1 = toRow (hi);
                if (prevRow >= 0) { r0 = jmin (r0, prevRow); r1 = jmax (r1, prevRow); }
                for (int r = r0; r <= r1; ++r)
                    cell (c, r, pixelColour (0.45f + std::abs ((float) r / (float) (rows - 1) - 0.5f) * 1.1f).withAlpha (fade));
                prevRow = toRow (0.5f * (lo + hi));
            }
            break;
        }

        case Mode::PitchTrail:
        {
            const float span = 14.0f;   // semitones visible
            const auto rowFor = [&] (float midi) { return (int) std::round ((midi - (viewCentre - span * 0.5f)) / span * (float) (rows - 1)); };

            // Note lanes for the active scale; the target note lane is brighter.
            for (int n = (int) std::floor (viewCentre - span * 0.5f); n <= (int) std::ceil (viewCentre + span * 0.5f); ++n)
            {
                if (! ((mask >> (unsigned) (((n % 12) + 12) % 12)) & 1u)) continue;
                const bool isTarget = targetNote >= 0.0f && n == (int) targetNote;
                const auto colour = Colour (isTarget ? 0xffE0262E : 0xff8E0C13).withAlpha ((isTarget ? 0.85f : 0.45f) * fade);
                const int r = rowFor ((float) n);
                for (int c = 0; c < kCols; c += (isTarget ? 1 : 2))
                    cell (c, r, colour);
            }

            int prevDet = -1, prevOut = -1;
            for (int c = 0; c < kCols; ++c)
            {
                if (! trailValid[(size_t) c]) { prevDet = prevOut = -1; continue; }
                const int rd = rowFor (trailDetected[(size_t) c]);
                const int ro = rowFor (trailOutput[(size_t) c]);
                const float age = 0.35f + 0.65f * (float) c / (float) kCols;

                auto drawRun = [&] (int r, int& prev, Colour colour)
                {
                    const int a = prev < 0 ? r : jmin (r, prev), b = prev < 0 ? r : jmax (r, prev);
                    for (int k = a; k <= b; ++k) cell (c, k, colour);
                    prev = r;
                };
                drawRun (rd, prevDet, Colour (0xffFFB3B6).withAlpha (0.75f * age * fade));
                drawRun (ro, prevOut, Colours::white.withAlpha (age * fade));
            }
            break;
        }
    }

    // Faint scanlines
    g.setColour (Colours::black.withAlpha (0.22f));
    for (float y = top; y < top + px * (float) rows; y += 3.0f)
        g.fillRect (area.getX(), y, area.getWidth(), 1.0f);
}
} // namespace automech
