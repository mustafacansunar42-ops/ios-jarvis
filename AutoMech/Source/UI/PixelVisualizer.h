#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>

namespace automech
{
// Snapshot of processor telemetry for one GUI frame.
struct DisplayState
{
    float detectedHz = 0.0f, detectedMidi = 0.0f, outputMidi = 0.0f, targetNote = -1.0f;
    bool voiced = false, locked = false;
    uint16_t mask = 0x0FFF;
};

// Retro 8-bit visualizer: a low-resolution grid of square pixels with 1 px
// gaps and scanlines. Not a Component: the pitch display draws it as a
// background layer so the note readout sits on top.
class PixelVisualizer
{
public:
    enum class Mode { PitchTrail = 0, Spectrum, Scope };

    PixelVisualizer();

    void setSampleRate (double sr) noexcept { sampleRate = sr; }
    void pushSamples (const float* data, int n) noexcept;   // GUI thread
    void tick (const DisplayState& state);                  // once per frame
    void draw (juce::Graphics& g, juce::Rectangle<float> area) const;

    void cycleMode() noexcept;
    Mode getMode() const noexcept { return mode; }
    static juce::String modeName (Mode m);

    static constexpr int kCols = 128;

private:
    static constexpr int fftOrder = 11, fftSize = 1 << fftOrder, trailEvery = 2;

    void updateSpectrum();
    static juce::Colour pixelColour (float heightFraction);

    Mode mode = Mode::PitchTrail;
    double sampleRate = 48000.0;

    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, fftSize> history {};
    int historyPos = 0;
    std::array<float, fftSize * 2> fftData {};

    std::array<float, kCols> bars {}, peaks {};
    std::array<int, kCols> peakHold {};
    std::array<float, kCols> trailDetected {}, trailOutput {};
    std::array<bool, kCols> trailValid {};
    int frameCount = 0;

    float viewCentre = 60.0f, scopeGain = 1.0f, level = 0.0f, fade = 0.0f;
    float targetNote = -1.0f;
    uint16_t mask = 0x0FFF;
};
} // namespace automech
