#include "PixelVisualizer.h"
#include "ScorchLookAndFeel.h"
#include "Palette.h"
#include "../PluginProcessor.h"

namespace scorch
{
namespace
{
constexpr float persistence = 0.6f;       // per-frame phosphor decay
constexpr float minFrequency = 30.0f;
constexpr float spectrumFloorDb = -84.0f;
constexpr int idleFramesBeforeSleep = 45;

constexpr float gridCentre = 31.5f;       // centre of the 64 x 64 grid
constexpr float circleRadius = 31.6f;     // cells farther than this are outside the display
constexpr float spectrumInner = 6.0f;     // radial spectrum: inner radius (cells)
constexpr float spectrumOuter = 30.0f;    // radial spectrum: outer radius (cells)
constexpr float curveYRange = 1.5f;       // transfer curve vertical extent
constexpr int curveMargin = 9;            // keeps the curve view inside the circle

void blit (const juce::Image& source, juce::Image& dest)
{
    const juce::Image::BitmapData src (source, juce::Image::BitmapData::readOnly);
    juce::Image::BitmapData dst (dest, juce::Image::BitmapData::writeOnly);
    const auto bytes = (size_t) (src.width * src.pixelStride);

    for (int y = 0; y < src.height; ++y)
        std::memcpy (dst.getLinePointer (y), src.getLinePointer (y), bytes);
}

int curveCol (float x)
{
    const float span = (float) (64 - 1 - 2 * curveMargin);
    return juce::jlimit (0, 63, curveMargin + juce::roundToInt ((x + 1.0f) * 0.5f * span));
}

int curveRow (float y)
{
    const float span = (float) (64 - 1 - 2 * curveMargin);
    return juce::jlimit (0, 63, curveMargin + juce::roundToInt ((0.5f - 0.5f * y / curveYRange) * span));
}
} // namespace

PixelVisualizer::PixelVisualizer (ScorchAudioProcessor& p)
    : processor (p)
{
    dryHistory.assign ((size_t) historySize, 0.0f);
    wetHistory.assign ((size_t) historySize, 0.0f);
    pullDry.assign (2048, 0.0f);
    pullWet.assign (2048, 0.0f);
    fftData.assign ((size_t) fftSize * 2, 0.0f);

    intensity.assign ((size_t) (gridSize * gridSize), 0.0f);
    inks.assign ((size_t) (gridSize * gridSize), none);
    barLevel.assign ((size_t) spectrumBins, 0.0f);
    peakLevel.assign ((size_t) spectrumBins, 0.0f);
    peakVelocity.assign ((size_t) spectrumBins, 0.0f);
    peakHold.assign ((size_t) spectrumBins, 0);

    setOpaque (false);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTitle ("Visualizer");
    startTimerHz (60);
}

juce::String PixelVisualizer::getModeName() const
{
    switch (mode)
    {
        case Mode::Spectrum: return "spectrum";
        case Mode::Curve:    return "curve";
        case Mode::Scope:    return "scope";
    }
    return {};
}

juce::Colour PixelVisualizer::inkColour (Ink ink) noexcept
{
    switch (ink)
    {
        case primary:   return palette::highlight;
        case secondary: return palette::deep.brighter (0.35f);
        case warm:      return palette::amber;
        case axis:      return palette::outline.brighter (0.4f);
        case peak:      return palette::spark;
        case none:      break;
    }
    return juce::Colours::transparentBlack;
}

//==============================================================================
bool PixelVisualizer::hitTest (int x, int y)
{
    const auto c = getLocalBounds().toFloat().getCentre();
    const float r = (float) juce::jmin (getWidth(), getHeight()) * 0.5f;
    return juce::Point<float> ((float) x, (float) y).getDistanceFrom (c) <= r;
}

void PixelVisualizer::mouseUp (const juce::MouseEvent& e)
{
    if (e.mouseWasClicked())
        cycleMode();
}

void PixelVisualizer::cycleMode()
{
    setMode (mode == Mode::Spectrum ? Mode::Curve : (mode == Mode::Curve ? Mode::Scope : Mode::Spectrum));
}

void PixelVisualizer::setMode (Mode newMode)
{
    mode = newMode;
    std::fill (intensity.begin(), intensity.end(), 0.0f);
    idleFrames = 0;
    renderCurrentMode();
    repaint();

    if (onModeChanged)
        onModeChanged();
}

float PixelVisualizer::historyAt (const std::vector<float>& buffer, int samplesAgo) const noexcept
{
    return buffer[(size_t) ((historyWrite - 1 - samplesAgo) & (historySize - 1))];
}

bool PixelVisualizer::insideCircle (int col, int row) const noexcept
{
    const float dx = (float) col - gridCentre;
    const float dy = (float) row - gridCentre;
    return dx * dx + dy * dy <= circleRadius * circleRadius;
}

void PixelVisualizer::plot (int col, int row, float value, Ink ink) noexcept
{
    if (col < 0 || col >= gridSize || row < 0 || row >= gridSize || value <= 0.0f || ! insideCircle (col, row))
        return;

    const auto index = (size_t) (row * gridSize + col);

    if (value >= intensity[index])
    {
        intensity[index] = juce::jmin (1.0f, value);
        inks[index] = ink;
    }
}

void PixelVisualizer::plotRun (int col, int rowA, int rowB, float value, Ink ink) noexcept
{
    for (int r = juce::jmin (rowA, rowB); r <= juce::jmax (rowA, rowB); ++r)
        plot (col, r, value, ink);
}

//==============================================================================
void PixelVisualizer::pullAudio()
{
    auto& fifo = processor.getVisualizerFifo();
    float newPeak = 0.0f;

    for (;;)
    {
        const int n = fifo.pull (pullDry.data(), pullWet.data(), (int) pullDry.size());

        if (n == 0)
            break;

        for (int i = 0; i < n; ++i)
        {
            dryHistory[(size_t) historyWrite] = pullDry[(size_t) i];
            wetHistory[(size_t) historyWrite] = pullWet[(size_t) i];
            historyWrite = (historyWrite + 1) & (historySize - 1);
            newPeak = juce::jmax (newPeak, std::abs (pullDry[(size_t) i]), std::abs (pullWet[(size_t) i]));
        }
    }

    // Fade in quickly when sound plays, fade out over ~1 s when it stops.
    if (newPeak > 1.0e-3f)
        activity = juce::jmin (1.0f, activity + 0.2f);
    else
        activity = activity < 0.003f ? 0.0f : activity * 0.94f;

    const float driven = processor.getMeters().drivenInput.exchange (0.0f);
    livePeak = juce::jmax (driven, livePeak * 0.85f);
}

void PixelVisualizer::decay()
{
    for (auto& v : intensity)
        v = v < 0.02f ? 0.0f : v * persistence;
}

void PixelVisualizer::renderCurrentMode()
{
    switch (mode)
    {
        case Mode::Spectrum: renderSpectrum(); break;
        case Mode::Curve:    renderCurve();    break;
        case Mode::Scope:    renderScope();    break;
    }
}

void PixelVisualizer::renderSpectrum()
{
    const double sampleRate = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 44100.0;
    const float maxFrequency = (float) juce::jmin (20000.0, sampleRate * 0.5);

    for (int i = 0; i < fftSize; ++i)
        fftData[(size_t) i] = historyAt (wetHistory, fftSize - 1 - i);

    std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);
    window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
    fft.performFrequencyOnlyForwardTransform (fftData.data());

    const float binsPerHz = (float) fftSize / (float) sampleRate;
    const int lastBin = fftSize / 2 - 1;
    const float ratio = maxFrequency / minFrequency;

    // dotted inner ring
    for (int a = 0; a < 48; a += 2)
    {
        const float angle = juce::MathConstants<float>::twoPi * (float) a / 48.0f;
        plot (juce::roundToInt (gridCentre + (spectrumInner - 1.5f) * std::sin (angle)),
              juce::roundToInt (gridCentre - (spectrumInner - 1.5f) * std::cos (angle)), 0.6f, axis);
    }

    for (int b = 0; b < spectrumBins; ++b)
    {
        const float f0 = minFrequency * std::pow (ratio, (float) b / (float) spectrumBins);
        const float f1 = minFrequency * std::pow (ratio, (float) (b + 1) / (float) spectrumBins);
        const int b0 = juce::jlimit (1, lastBin, (int) (f0 * binsPerHz));
        const int b1 = juce::jlimit (b0, lastBin, (int) (f1 * binsPerHz));

        float magnitude = 0.0f;
        for (int k = b0; k <= b1; ++k)
            magnitude = juce::jmax (magnitude, fftData[(size_t) k]);

        // a full-scale sine reads 0 dB (Hann coherent gain 0.5)
        const float db = juce::Decibels::gainToDecibels (magnitude * 4.0f / (float) fftSize, -120.0f);
        const float level = juce::jlimit (0.0f, 1.0f, juce::jmap (db, spectrumFloorDb, 0.0f, 0.0f, 1.0f));

        auto& bar = barLevel[(size_t) b];
        bar = juce::jmax (level * juce::jmin (1.0f, activity * 1.5f), bar - 0.025f);

        auto& pk = peakLevel[(size_t) b];
        if (bar >= pk)
        {
            pk = bar;
            peakHold[(size_t) b] = 24;
            peakVelocity[(size_t) b] = 0.0f;
        }
        else if (peakHold[(size_t) b] > 0)
        {
            --peakHold[(size_t) b];
        }
        else
        {
            peakVelocity[(size_t) b] += 0.0008f;
            pk = juce::jmax (0.0f, pk - peakVelocity[(size_t) b]);
        }

        // bars radiate clockwise from 12 o'clock, low to high frequency
        const float angle = juce::MathConstants<float>::twoPi * ((float) b + 0.5f) / (float) spectrumBins;
        const float sx = std::sin (angle), cy = -std::cos (angle);
        const float length = bar * (spectrumOuter - spectrumInner);

        for (float t = 0.0f; t <= length; t += 0.5f)
        {
            const float frac = t / (spectrumOuter - spectrumInner);
            const float r = spectrumInner + t;
            plot (juce::roundToInt (gridCentre + r * sx), juce::roundToInt (gridCentre + r * cy),
                  0.45f + 0.55f * frac, frac > 0.72f ? warm : primary);
        }

        if (pk > 0.02f)
        {
            const float r = spectrumInner + pk * (spectrumOuter - spectrumInner) + 0.8f;
            plot (juce::roundToInt (gridCentre + r * sx), juce::roundToInt (gridCentre + r * cy),
                  juce::jmax (0.3f, activity), peak);
        }
    }
}

void PixelVisualizer::renderCurve()
{
    auto& apvts = processor.apvts;
    const auto model = static_cast<Model> ((int) apvts.getRawParameterValue (params::id::model)->load());
    const float drive = juce::Decibels::decibelsToGain (apvts.getRawParameterValue (params::id::drive)->load());
    const float heat  = apvts.getRawParameterValue (params::id::heat)->load() * 0.01f;
    const float bias  = apvts.getRawParameterValue (params::id::bias)->load() * 0.01f;

    const float offset = SaturationEngine::shape (model, 0.0f, heat, bias);
    auto curve = [&] (float x) { return SaturationEngine::shape (model, x * drive, heat, bias) - offset; };

    // dotted axes
    const int midRow = curveRow (0.0f), midCol = curveCol (0.0f);
    for (int c = 0; c < gridSize; c += 2) plot (c, midRow, 0.6f, axis);
    for (int r = 0; r < gridSize; r += 2) plot (midCol, r, 0.6f, axis);

    // the curve itself: always visible, brighter while sound plays
    const float level = 0.6f + 0.4f * activity;
    int previousRow = -1;
    const int first = curveCol (-1.0f), last = curveCol (1.0f);

    for (int c = first; c <= last; ++c)
    {
        const float x = -1.0f + 2.0f * (float) (c - first) / (float) (last - first);
        const int r = curveRow (curve (x));
        plotRun (c, previousRow < 0 ? r : previousRow, r, level, primary);
        previousRow = r;
    }

    if (activity <= 0.0f)
        return;

    // recent input samples riding along the curve
    for (int k = 0; k < 64; ++k)
    {
        const float x = juce::jlimit (-1.0f, 1.0f, historyAt (dryHistory, k * 5));
        plot (curveCol (x), curveRow (curve (x)), activity * (1.0f - (float) k / 72.0f), warm);
    }

    // live-level dot (2 x 2) and its mirror
    const float pk = juce::jmin (1.0f, livePeak);
    for (const float x : { pk, -pk })
    {
        const int c = curveCol (x), r = curveRow (curve (x));
        const float v = activity * (x > 0.0f ? 1.0f : 0.6f);

        for (int dc = 0; dc < 2; ++dc)
            for (int dr = 0; dr < 2; ++dr)
                plot (c + dc - (x < 0.0f ? 1 : 0), r + dr - 1, v, peak);
    }
}

void PixelVisualizer::renderScope()
{
    const int span = gridSize * 8;

    // rising zero-crossing trigger on the dry signal, for a stable picture
    int begin = span;
    for (int ago = span; ago < span + 2048 && ago < historySize - 2; ++ago)
    {
        if (historyAt (dryHistory, ago + 1) < 0.0f && historyAt (dryHistory, ago) >= 0.0f)
        {
            begin = ago;
            break;
        }
    }

    float maxAbs = 1.0e-3f;
    for (int s = 0; s < span; ++s)
        maxAbs = juce::jmax (maxAbs, std::abs (historyAt (dryHistory, begin - s)), std::abs (historyAt (wetHistory, begin - s)));

    scopeGain += 0.15f * (juce::jlimit (1.0f, 8.0f, 0.85f / maxAbs) - scopeGain);

    auto rowFor = [this] (float y)
    {
        return juce::jlimit (0, gridSize - 1, juce::roundToInt (gridCentre - y * scopeGain * 22.0f));
    };

    for (int c = 0; c < gridSize; c += 2)
        plot (c, rowFor (0.0f), 0.5f, axis);

    if (activity <= 0.0f)
        return;

    for (int c = 0; c < gridSize; ++c)
    {
        const int from = begin - (c * span) / gridSize;
        const int to   = begin - ((c + 1) * span) / gridSize;

        float dMin = 1.0e9f, dMax = -1.0e9f, wMin = 1.0e9f, wMax = -1.0e9f;
        for (int s = from; s > to; --s)
        {
            const float d = historyAt (dryHistory, s);
            const float w = historyAt (wetHistory, s);
            dMin = juce::jmin (dMin, d); dMax = juce::jmax (dMax, d);
            wMin = juce::jmin (wMin, w); wMax = juce::jmax (wMax, w);
        }

        if (dMax >= dMin)
            plotRun (c, rowFor (dMax), rowFor (dMin), 0.55f * activity, secondary);
        if (wMax >= wMin)
            plotRun (c, rowFor (wMax), rowFor (wMin), activity, primary);
    }
}

//==============================================================================
void PixelVisualizer::timerCallback()
{
    pullAudio();
    decay();
    renderCurrentMode();

    // Stop repainting once everything has settled (no audio and no edits).
    bool anyMoving = activity > 0.0f;
    for (size_t i = 0; i < intensity.size() && ! anyMoving; ++i)
        anyMoving = intensity[i] > 0.0f && inks[i] != axis && mode != Mode::Curve;

    auto& apvts = processor.apvts;
    const float key = apvts.getRawParameterValue (params::id::model)->load() * 1000.0f
                    + apvts.getRawParameterValue (params::id::drive)->load() * 7.0f
                    + apvts.getRawParameterValue (params::id::heat)->load() * 0.13f
                    + apvts.getRawParameterValue (params::id::bias)->load() * 0.017f;

    if (anyMoving || ! juce::exactlyEqual (key, lastKey))
        idleFrames = 0;
    else
        ++idleFrames;

    lastKey = key;

    if (idleFrames < idleFramesBeforeSleep)
        repaint();
}

//==============================================================================
void PixelVisualizer::rebuildStaticLayers (int sizePx, float scale)
{
    cachedScale = scale;
    cellPx = juce::jmax (3, sizePx / gridSize);
    gapPx = juce::jmax (1, juce::roundToInt (scale));
    originPx = (sizePx - gridSize * cellPx) / 2;

    frame = juce::Image (juce::Image::ARGB, sizePx, sizePx, true, juce::SoftwareImageType());
    backgroundLayer = juce::Image (juce::Image::ARGB, sizePx, sizePx, true, juce::SoftwareImageType());

    juce::Graphics g (backgroundLayer);
    const auto area = backgroundLayer.getBounds().toFloat();

    g.fillAll (palette::display);

    // faint orange glow in the middle
    g.setGradientFill (juce::ColourGradient (palette::glow.withAlpha (0.55f), area.getCentre(),
                                             palette::display.withAlpha (0.0f), area.getTopLeft().translated (area.getWidth() * 0.15f, 0.0f), true));
    g.fillRect (area);

    // unlit dot matrix (inside the circle only)
    const int size = cellPx - gapPx;
    g.setColour (palette::deep.withAlpha (0.13f));
    for (int r = 0; r < gridSize; ++r)
        for (int c = 0; c < gridSize; ++c)
            if (insideCircle (c, r))
                g.fillRect (originPx + c * cellPx, originPx + r * cellPx, size, size);

    // inner shadow toward the rim
    g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, area.getCentre(),
                                             juce::Colours::black.withAlpha (0.75f), area.getCentre().translated (area.getWidth() * 0.5f, 0.0f), true));
    g.fillRect (area);
}

void PixelVisualizer::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
    const int sizePx = juce::jmax (1, juce::roundToInt ((float) juce::jmin (getWidth(), getHeight()) * scale));

    if (frame.getWidth() != sizePx || ! juce::exactlyEqual (scale, cachedScale))
        rebuildStaticLayers (sizePx, scale);

    blit (backgroundLayer, frame);

    {
        juce::Graphics fg (frame);
        const int size = cellPx - gapPx;

        // bloom pass
        for (int r = 0; r < gridSize; ++r)
            for (int c = 0; c < gridSize; ++c)
            {
                const auto i = (size_t) (r * gridSize + c);
                const float v = intensity[i];

                if (v > 0.35f && inks[i] != axis)
                {
                    fg.setColour (inkColour ((Ink) inks[i]).withAlpha (0.13f * v));
                    fg.fillRect (originPx + c * cellPx - gapPx - 1, originPx + r * cellPx - gapPx - 1,
                                 size + 2 * gapPx + 2, size + 2 * gapPx + 2);
                }
            }

        // pixels
        for (int r = 0; r < gridSize; ++r)
            for (int c = 0; c < gridSize; ++c)
            {
                const auto i = (size_t) (r * gridSize + c);
                const float v = intensity[i];

                if (v > 0.0f)
                {
                    fg.setColour (inkColour ((Ink) inks[i]).withAlpha (juce::jmin (1.0f, 0.15f + v)));
                    fg.fillRect (originPx + c * cellPx, originPx + r * cellPx, size, size);
                }
            }

        // faint scanlines
        fg.setColour (juce::Colours::black.withAlpha (0.22f));
        const int pitch = juce::jmax (3, juce::roundToInt (3.0f * scale));
        for (int y = 0; y < sizePx; y += pitch)
            fg.fillRect (0, y, sizePx, juce::jmax (1, juce::roundToInt (scale)));
    }

    const auto square = bounds.withSizeKeepingCentre (juce::jmin (bounds.getWidth(), bounds.getHeight()),
                                                      juce::jmin (bounds.getWidth(), bounds.getHeight()));
    juce::Path circle;
    circle.addEllipse (square);

    g.saveState();
    g.reduceClipRegion (circle);
    g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
    g.drawImage (frame, square);
    g.restoreState();
}
} // namespace scorch
