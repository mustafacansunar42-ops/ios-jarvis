#include "PixelVisualizer.h"
#include "FrierLookAndFeel.h"
#include "Palette.h"
#include "../PluginProcessor.h"

namespace frier
{
namespace
{
constexpr float persistence = 0.6f;      // per-frame phosphor decay
constexpr float minFrequency = 30.0f;
constexpr float spectrumFloorDb = -84.0f;
constexpr int idleFramesBeforeSleep = 45;
constexpr float curveYRange = 1.5f;      // vertical extent of the transfer-curve view

void blit (const juce::Image& source, juce::Image& dest)
{
    const juce::Image::BitmapData src (source, juce::Image::BitmapData::readOnly);
    juce::Image::BitmapData dst (dest, juce::Image::BitmapData::writeOnly);
    const auto bytes = (size_t) (src.width * src.pixelStride);

    for (int y = 0; y < src.height; ++y)
        std::memcpy (dst.getLinePointer (y), src.getLinePointer (y), bytes);
}
} // namespace

PixelVisualizer::PixelVisualizer (FrierAudioProcessor& p)
    : processor (p)
{
    dryHistory.assign ((size_t) historySize, 0.0f);
    wetHistory.assign ((size_t) historySize, 0.0f);
    pullDry.assign (2048, 0.0f);
    pullWet.assign (2048, 0.0f);
    fftData.assign ((size_t) fftSize * 2, 0.0f);

    ensureGrid (cols);
    setOpaque (false);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTitle ("Visualizer");
    startTimerHz (60);
}

void PixelVisualizer::ensureGrid (int newCols)
{
    cols = juce::jmax (16, newCols);
    intensity.assign ((size_t) (cols * rows), 0.0f);
    inks.assign ((size_t) (cols * rows), none);
    barLevel.assign ((size_t) cols, 0.0f);
    peakLevel.assign ((size_t) cols, 0.0f);
    peakVelocity.assign ((size_t) cols, 0.0f);
    peakHold.assign ((size_t) cols, 0);
}

juce::String PixelVisualizer::modeName() const
{
    switch (mode)
    {
        case Mode::Curve:    return "CURVE";
        case Mode::Spectrum: return "SPECTRUM";
        case Mode::Scope:    return "SCOPE";
    }
    return {};
}

juce::Colour PixelVisualizer::inkColour (Ink ink) const noexcept
{
    switch (ink)
    {
        case primary:   return palette::goldBright;
        case secondary: return palette::textDim;
        case hot:       return palette::hot;
        case axis:      return palette::panelBorder.brighter (0.25f);
        case peak:      return palette::cream;
        case none:      break;
    }
    return juce::Colours::transparentBlack;
}

//==============================================================================
void PixelVisualizer::mouseUp (const juce::MouseEvent& e)
{
    if (! e.mouseWasClicked())
        return;

    setMode (mode == Mode::Curve ? Mode::Spectrum : (mode == Mode::Spectrum ? Mode::Scope : Mode::Curve));
}

void PixelVisualizer::setMode (Mode newMode)
{
    mode = newMode;
    std::fill (intensity.begin(), intensity.end(), 0.0f);
    idleFrames = 0;
    repaint();
}

float PixelVisualizer::historyAt (const std::vector<float>& buffer, int samplesAgo) const noexcept
{
    return buffer[(size_t) ((historyWrite - 1 - samplesAgo) & (historySize - 1))];
}

void PixelVisualizer::plot (int col, int row, float value, Ink ink) noexcept
{
    if (col < 0 || col >= cols || row < 0 || row >= rows || value <= 0.0f)
        return;

    const auto index = (size_t) (row * cols + col);

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

void PixelVisualizer::renderCurve()
{
    auto& apvts = processor.apvts;
    const auto model = static_cast<Model> ((int) apvts.getRawParameterValue (params::id::model)->load());
    const float drive = juce::Decibels::decibelsToGain (apvts.getRawParameterValue (params::id::drive)->load());
    const float heat  = apvts.getRawParameterValue (params::id::heat)->load() * 0.01f;
    const float bias  = apvts.getRawParameterValue (params::id::bias)->load() * 0.01f;

    auto rowFor = [] (float y) { return juce::jlimit (0, rows - 1, juce::roundToInt ((0.5f - 0.5f * y / curveYRange) * (float) (rows - 1))); };
    auto colFor = [this] (float x) { return juce::jlimit (0, cols - 1, juce::roundToInt ((x + 1.0f) * 0.5f * (float) (cols - 1))); };

    const float offset = SaturationEngine::shape (model, 0.0f, heat, bias);
    auto curve = [&] (float x) { return SaturationEngine::shape (model, x * drive, heat, bias) - offset; };

    // dotted axes
    const int midRow = rowFor (0.0f), midCol = colFor (0.0f);
    for (int c = 0; c < cols; c += 2) plot (c, midRow, 0.6f, axis);
    for (int r = 0; r < rows; r += 2) plot (midCol, r, 0.6f, axis);

    // the transfer curve itself (always visible, brighter while sound plays)
    const float curveLevel = 0.55f + 0.45f * activity;
    int previousRow = -1;

    for (int c = 0; c < cols; ++c)
    {
        const float x = -1.0f + 2.0f * (float) c / (float) (cols - 1);
        const int r = rowFor (curve (x));
        plotRun (c, previousRow < 0 ? r : previousRow, r, curveLevel, primary);
        previousRow = r;
    }

    if (activity <= 0.0f)
        return;

    // recent input samples riding along the curve
    for (int k = 0; k < 96; ++k)
    {
        const float x = juce::jlimit (-1.0f, 1.0f, historyAt (dryHistory, k * 4));
        plot (colFor (x), rowFor (curve (x)), activity * (1.0f - (float) k / 110.0f), peak);
    }

    // live peak marker (2x2 pixels) and its mirror
    const float pk = juce::jmin (1.0f, livePeak);
    for (const float x : { pk, -pk })
    {
        const int c = colFor (x), r = rowFor (curve (x));
        const float level = activity * (x > 0.0f ? 1.0f : 0.55f);

        for (int dc = 0; dc < 2; ++dc)
            for (int dr = 0; dr < 2; ++dr)
                plot (c + dc - (x < 0.0f ? 1 : 0), r + dr - 1, level, hot);
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

    // faint decade markers (100 Hz, 1 kHz, 10 kHz)
    for (const float f : { 100.0f, 1000.0f, 10000.0f })
    {
        const int c = juce::roundToInt (std::log (f / minFrequency) / std::log (ratio) * (float) cols);
        for (int r = 0; r < rows; r += 3)
            plot (c, r, 0.45f, axis);
    }

    for (int c = 0; c < cols; ++c)
    {
        const float f0 = minFrequency * std::pow (ratio, (float) c / (float) cols);
        const float f1 = minFrequency * std::pow (ratio, (float) (c + 1) / (float) cols);
        const int b0 = juce::jlimit (1, lastBin, (int) (f0 * binsPerHz));
        const int b1 = juce::jlimit (b0, lastBin, (int) (f1 * binsPerHz));

        float magnitude = 0.0f;
        for (int b = b0; b <= b1; ++b)
            magnitude = juce::jmax (magnitude, fftData[(size_t) b]);

        // a full-scale sine reads 0 dB (Hann coherent gain 0.5)
        const float db = juce::Decibels::gainToDecibels (magnitude * 4.0f / (float) fftSize, -120.0f);
        const float level = juce::jlimit (0.0f, (float) rows, juce::jmap (db, spectrumFloorDb, 0.0f, 0.0f, (float) rows));

        auto& bar = barLevel[(size_t) c];
        bar = juce::jmax (level * juce::jmin (1.0f, activity * 1.5f), bar - 1.1f);

        auto& pk = peakLevel[(size_t) c];
        if (bar >= pk)
        {
            pk = bar;
            peakHold[(size_t) c] = 24;
            peakVelocity[(size_t) c] = 0.0f;
        }
        else if (peakHold[(size_t) c] > 0)
        {
            --peakHold[(size_t) c];
        }
        else
        {
            peakVelocity[(size_t) c] += 0.035f;
            pk = juce::jmax (0.0f, pk - peakVelocity[(size_t) c]);
        }

        const int height = (int) bar;
        for (int k = 0; k < height; ++k)
        {
            const float frac = (float) k / (float) (rows - 1);
            plot (c, rows - 1 - k, 0.4f + 0.6f * frac, frac > 0.8f ? hot : primary);
        }

        if (pk >= 1.0f)
            plot (c, rows - 1 - juce::jmin (rows - 1, (int) pk), juce::jmax (0.25f, activity), peak);
    }
}

void PixelVisualizer::renderScope()
{
    const int span = juce::jmin (historySize / 3, cols * 4);

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

    const float targetGain = juce::jlimit (1.0f, 8.0f, 0.9f / maxAbs);
    scopeGain += 0.15f * (targetGain - scopeGain);

    auto rowFor = [this] (float y)
    {
        return juce::jlimit (0, rows - 1, juce::roundToInt ((0.5f - 0.5f * y * scopeGain) * (float) (rows - 1)));
    };

    const int midRow = rowFor (0.0f);
    for (int c = 0; c < cols; c += 2)
        plot (c, midRow, 0.5f, axis);

    if (activity <= 0.0f)
        return;

    for (int c = 0; c < cols; ++c)
    {
        const int from = begin - (c * span) / cols;
        const int to   = begin - ((c + 1) * span) / cols;

        float dMin = 1.0e9f, dMax = -1.0e9f, wMin = 1.0e9f, wMax = -1.0e9f;
        for (int s = from; s > to; --s)
        {
            const float d = historyAt (dryHistory, s);
            const float w = historyAt (wetHistory, s);
            dMin = juce::jmin (dMin, d); dMax = juce::jmax (dMax, d);
            wMin = juce::jmin (wMin, w); wMax = juce::jmax (wMax, w);
        }

        if (dMax >= dMin)
            plotRun (c, rowFor (dMax), rowFor (dMin), 0.5f * activity, secondary);
        if (wMax >= wMin)
            plotRun (c, rowFor (wMax), rowFor (wMin), activity, primary);
    }
}

void PixelVisualizer::renderCurrentMode()
{
    switch (mode)
    {
        case Mode::Curve:    renderCurve();    break;
        case Mode::Spectrum: renderSpectrum(); break;
        case Mode::Scope:    renderScope();    break;
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
void PixelVisualizer::rebuildStaticLayers (int widthPx, int heightPx, float scale)
{
    cachedScale = scale;
    cellPx = juce::jmax (3, heightPx / rows);
    gapPx = juce::jmax (1, juce::roundToInt (scale));

    const int newCols = juce::jmax (16, widthPx / cellPx);
    if (newCols != cols)
    {
        ensureGrid (newCols);
        renderCurrentMode();   // don't show an empty frame after a resize
    }

    originX = (widthPx - cols * cellPx) / 2;
    originY = (heightPx - rows * cellPx) / 2;

    frame = juce::Image (juce::Image::ARGB, widthPx, heightPx, true, juce::SoftwareImageType());
    backgroundLayer = juce::Image (juce::Image::ARGB, widthPx, heightPx, true, juce::SoftwareImageType());

    juce::Graphics g (backgroundLayer);
    const auto area = backgroundLayer.getBounds().toFloat();

    g.fillAll (palette::display);

    // warm glow in the middle, like light behind the reference's eclipse
    g.setGradientFill (juce::ColourGradient (palette::glow.withAlpha (0.45f), area.getCentre(),
                                             palette::display.withAlpha (0.0f), area.getTopLeft(), true));
    g.fillRect (area);

    // unlit dot matrix
    const int size = cellPx - gapPx;
    g.setColour (palette::divider.withAlpha (0.35f));
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c)
            g.fillRect (originX + c * cellPx, originY + r * cellPx, size, size);

    // vignette
    g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, area.getCentre(),
                                             juce::Colours::black.withAlpha (0.55f), area.getTopLeft().translated (-area.getWidth() * 0.1f, 0.0f), true));
    g.fillRect (area);
}

void PixelVisualizer::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
    const int widthPx  = juce::jmax (1, juce::roundToInt ((float) getWidth()  * scale));
    const int heightPx = juce::jmax (1, juce::roundToInt ((float) getHeight() * scale));

    if (frame.getWidth() != widthPx || frame.getHeight() != heightPx || ! juce::exactlyEqual (scale, cachedScale))
        rebuildStaticLayers (widthPx, heightPx, scale);

    blit (backgroundLayer, frame);

    {
        juce::Graphics fg (frame);
        const int size = cellPx - gapPx;

        // bloom pass
        for (int r = 0; r < rows; ++r)
            for (int c = 0; c < cols; ++c)
            {
                const auto i = (size_t) (r * cols + c);
                const float v = intensity[i];

                if (v > 0.35f && inks[i] != axis)
                {
                    fg.setColour (inkColour ((Ink) inks[i]).withAlpha (0.11f * v));
                    fg.fillRect (originX + c * cellPx - gapPx - 1, originY + r * cellPx - gapPx - 1,
                                 size + 2 * gapPx + 2, size + 2 * gapPx + 2);
                }
            }

        // pixels
        for (int r = 0; r < rows; ++r)
            for (int c = 0; c < cols; ++c)
            {
                const auto i = (size_t) (r * cols + c);
                const float v = intensity[i];

                if (v > 0.0f)
                {
                    fg.setColour (inkColour ((Ink) inks[i]).withAlpha (juce::jmin (1.0f, 0.15f + v)));
                    fg.fillRect (originX + c * cellPx, originY + r * cellPx, size, size);
                }
            }

        // scanlines
        fg.setColour (juce::Colours::black.withAlpha (0.2f));
        const int pitch = juce::jmax (3, juce::roundToInt (3.0f * scale));
        for (int y = 0; y < heightPx; y += pitch)
            fg.fillRect (0, y, widthPx, juce::jmax (1, juce::roundToInt (scale)));
    }

    juce::Path clip;
    clip.addRoundedRectangle (bounds, 6.0f);
    g.saveState();
    g.reduceClipRegion (clip);
    g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
    g.drawImage (frame, bounds);
    g.restoreState();

    // "PIXEL" mode tag + mode indicator squares
    const auto tag = juce::Rectangle<float> (12.0f, 10.0f, 54.0f, 18.0f);
    g.setColour (palette::display.withAlpha (0.85f));
    g.fillRoundedRectangle (tag.withWidth (175.0f).expanded (3.0f), 4.0f);
    g.setColour (palette::gold.withAlpha (0.8f));
    g.drawRoundedRectangle (tag, 3.0f, 1.0f);
    g.setFont (FrierLookAndFeel::font (10.5f, 0.32f));
    g.drawText ("PIXEL", tag, juce::Justification::centred, false);

    for (int i = 0; i < 3; ++i)
    {
        const auto square = juce::Rectangle<float> (tag.getRight() + 9.0f + (float) i * 9.0f, tag.getCentreY() - 3.0f, 6.0f, 6.0f);
        g.setColour (i == (int) mode ? palette::goldBright : palette::ledOff);
        g.fillRect (square);
    }

    g.setColour (palette::textDim);
    g.drawText (modeName(), juce::Rectangle<float> (tag.getRight() + 40.0f, tag.getY(), 100.0f, tag.getHeight()),
                juce::Justification::centredLeft, false);

    // current model / drive readout, top right
    auto& apvts = processor.apvts;
    const auto modelIndex = (int) apvts.getRawParameterValue (params::id::model)->load();
    const auto driveText = apvts.getParameter (params::id::drive)->getCurrentValueAsText();
    const auto readout = params::modelNames[modelIndex].toUpperCase() + "   DRIVE " + driveText;
    const auto readoutArea = juce::Rectangle<float> (bounds.getRight() - 232.0f, tag.getY(), 220.0f, tag.getHeight());

    g.setColour (palette::display.withAlpha (0.85f));
    g.fillRoundedRectangle (readoutArea.withLeft (readoutArea.getRight() - 160.0f).expanded (3.0f), 4.0f);
    g.setColour (palette::textDim);
    g.drawText (readout, readoutArea, juce::Justification::centredRight, false);
}
} // namespace frier
