#pragma once

#include "Biquad.h"
#include <vector>

namespace automech
{
// Monophonic YIN pitch detector. Runs on a decimated (~12 kHz) copy of the
// signal so the analysis is cheap enough to live on the audio thread.
// All memory is allocated in prepare(); push() never allocates.
class PitchDetector
{
public:
    void prepare (double sampleRate);
    void reset();

    void setRange (float minHz, float maxHz) noexcept;
    void setSensitivity (float amount01) noexcept;

    void push (float x) noexcept;

    float getFrequency() const noexcept  { return frequency; }
    float getConfidence() const noexcept { return confidence; }
    bool  isVoiced() const noexcept      { return voiced; }

private:
    void analyse() noexcept;

    static constexpr int kWindow = 256;
    static constexpr int kHop = 64;

    double rate = 12000.0;
    int decimation = 4, decimCounter = 0;
    Biquad aa1, aa2;

    int maxTauCap = 400, minTau = 8, maxTau = 200;
    std::vector<float> ring, frame, diff, cmnd;
    int ringSize = 0, writeIdx = 0, hopCounter = 0;

    float threshold = 0.2f, gate = 0.001f;
    float frequency = 0.0f, confidence = 0.0f;
    bool voiced = false;
    float history[3] {};
};
} // namespace automech
