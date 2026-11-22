#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

class FrierAudioProcessor;

namespace frier
{
/** Retro 8-bit visualizer drawn on a low-resolution grid of square "pixels".

    Modes (click the display or the PIXEL tag to cycle):
      CURVE    - current transfer curve as a pixel line, plus the live signal on it
      SPECTRUM - 2048-point Hann FFT with falling peak-hold pixels
      SCOPE    - dry waveform (dim) against the fried waveform (gold)

    Audio arrives through the processor's lock-free FIFO; the display redraws at
    60 fps with phosphor-style persistence, and fades out when the sound stops. */
class PixelVisualizer final : public juce::Component, private juce::Timer
{
public:
    enum class Mode { Curve, Spectrum, Scope };

    explicit PixelVisualizer (FrierAudioProcessor&);

    void setMode (Mode newMode);
    Mode getMode() const noexcept { return mode; }

    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    enum Ink : uint8_t { none = 0, primary, secondary, hot, axis, peak };

    static constexpr int rows = 48;
    static constexpr int historySize = 8192;
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;

    void timerCallback() override;
    void pullAudio();
    void decay();
    void renderCurrentMode();
    void renderCurve();
    void renderSpectrum();
    void renderScope();

    void plot (int col, int row, float intensity, Ink ink) noexcept;
    void plotRun (int col, int rowA, int rowB, float intensity, Ink ink) noexcept;
    void ensureGrid (int newCols);
    float historyAt (const std::vector<float>& buffer, int samplesAgo) const noexcept;

    void rebuildStaticLayers (int widthPx, int heightPx, float scale);
    juce::Colour inkColour (Ink ink) const noexcept;
    juce::String modeName() const;

    FrierAudioProcessor& processor;

    Mode mode = Mode::Curve;
    int cols = 128;

    std::vector<float> intensity;
    std::vector<uint8_t> inks;

    // audio history (GUI thread only)
    std::vector<float> dryHistory, wetHistory;
    int historyWrite = 0;
    std::vector<float> pullDry, pullWet;

    float activity = 0.0f;       // 0..1 fade for signal-driven content
    float livePeak = 0.0f;
    float scopeGain = 1.0f;
    float lastKey = -1.0f;       // detects parameter edits that change the picture
    int idleFrames = 0;          // repainting stops once the display has settled

    // spectrum
    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann, false };
    std::vector<float> fftData;
    std::vector<float> barLevel, peakLevel, peakVelocity;
    std::vector<int> peakHold;

    // rendering caches (physical pixels)
    juce::Image frame, backgroundLayer, overlayLayer;
    int cellPx = 4, gapPx = 1, originX = 0, originY = 0;
    float cachedScale = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PixelVisualizer)
};
} // namespace frier
