#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>

class ScorchAudioProcessor;

namespace scorch
{
/** Retro 8-bit visualizer on a 64 x 64 grid of square "pixels", clipped to a circle.

    Modes (click the display or the PIXEL tag below it to cycle):
      SPECTRUM - radial pixel spectrum: bars radiate from the centre, with peak-hold pixels
      CURVE    - current saturation transfer curve as a pixel line, with a live-level dot
      SCOPE    - dry waveform (deep orange) against the fried waveform (bright orange)

    Audio arrives through the processor's lock-free FIFO (2048-point Hann FFT for the
    spectrum). The display redraws at 60 fps with phosphor-style persistence and fades
    out when the sound stops. */
class PixelVisualizer final : public juce::Component, private juce::Timer
{
public:
    enum class Mode { Spectrum, Curve, Scope };

    explicit PixelVisualizer (ScorchAudioProcessor&);

    void setMode (Mode newMode);
    void cycleMode();
    Mode getMode() const noexcept { return mode; }
    juce::String getModeName() const;

    std::function<void()> onModeChanged;

    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool hitTest (int x, int y) override;

private:
    enum Ink : uint8_t { none = 0, primary, secondary, warm, axis, peak };

    static constexpr int gridSize = 64;
    static constexpr int historySize = 8192;
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int spectrumBins = 72;

    void timerCallback() override;
    void pullAudio();
    void decay();
    void renderCurrentMode();
    void renderSpectrum();
    void renderCurve();
    void renderScope();

    void plot (int col, int row, float intensity, Ink ink) noexcept;
    void plotRun (int col, int rowA, int rowB, float intensity, Ink ink) noexcept;
    bool insideCircle (int col, int row) const noexcept;
    float historyAt (const std::vector<float>& buffer, int samplesAgo) const noexcept;

    void rebuildStaticLayers (int sizePx, float scale);
    static juce::Colour inkColour (Ink ink) noexcept;

    ScorchAudioProcessor& processor;

    Mode mode = Mode::Spectrum;

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
    juce::Image frame, backgroundLayer;
    int cellPx = 4, gapPx = 1, originPx = 0;
    float cachedScale = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PixelVisualizer)
};
} // namespace scorch
