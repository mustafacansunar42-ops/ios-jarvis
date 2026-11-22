// Developer tools for AutoMech (built with -DAUTOMECH_BUILD_TOOLS=ON).
//
//   AutoMechTools test              offline DSP checks (pitch really gets corrected)
//   AutoMechTools snapshot out.png [mode]  render the editor to a PNG (mode 0 trail, 1 spectrum, 2 scope)

#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>
#include <cstdio>

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kBlock = 512;

// Harmonic-rich, vowel-ish test tone.
std::vector<float> makeVoice (float hz, double seconds)
{
    const int n = (int) (seconds * kSampleRate);
    std::vector<float> out ((size_t) n);
    double phase = 0.0;
    for (int i = 0; i < n; ++i)
    {
        phase += hz / kSampleRate;
        float s = 0.0f;
        for (int k = 1; k <= 12; ++k)
        {
            // crude formant emphasis around 700 Hz and 1200 Hz
            const float fk = hz * (float) k;
            const float env = 1.0f / (float) k + 0.6f * std::exp (-std::pow ((fk - 700.0f) / 250.0f, 2.0f))
                                                 + 0.4f * std::exp (-std::pow ((fk - 1200.0f) / 300.0f, 2.0f));
            s += env * (float) std::sin (juce::MathConstants<double>::twoPi * phase * k);
        }
        out[(size_t) i] = 0.2f * s;
    }
    return out;
}

// Full-rate YIN on a long window: an independent reference measurement.
float measurePitch (const std::vector<float>& x, int start, int length)
{
    const int maxTau = (int) (kSampleRate / 50.0);
    const int window = length - maxTau;
    std::vector<double> d ((size_t) maxTau + 1, 0.0), cm ((size_t) maxTau + 1, 1.0);
    double running = 0.0;
    for (int tau = 1; tau <= maxTau; ++tau)
    {
        double sum = 0.0;
        for (int j = 0; j < window; ++j)
        {
            const double v = x[(size_t) (start + j)] - x[(size_t) (start + j + tau)];
            sum += v * v;
        }
        d[(size_t) tau] = sum;
        running += sum;
        cm[(size_t) tau] = running > 0 ? sum * tau / running : 1.0;
    }
    int tau = -1;
    for (int t = 20; t < maxTau; ++t)
        if (cm[(size_t) t] < 0.15) { while (t + 1 < maxTau && cm[(size_t) t + 1] < cm[(size_t) t]) ++t; tau = t; break; }
    if (tau < 0) return 0.0f;
    const double s0 = cm[(size_t) tau - 1], s1 = cm[(size_t) tau], s2 = cm[(size_t) tau + 1];
    const double denom = s0 + s2 - 2.0 * s1;
    const double shift = std::abs (denom) > 1e-12 ? 0.5 * (s0 - s2) / denom : 0.0;
    return (float) (kSampleRate / (tau + shift));
}

struct Result { float pitch; float gainDb; bool finite; };

Result run (AutoMechProcessor& p, const std::vector<float>& input)
{
    p.prepareToPlay (kSampleRate, kBlock);
    juce::AudioBuffer<float> buffer (2, kBlock);
    juce::MidiBuffer midi;
    std::vector<float> out (input.size(), 0.0f);
    bool finite = true;

    for (size_t pos = 0; pos + kBlock <= input.size(); pos += kBlock)
    {
        for (int ch = 0; ch < 2; ++ch)
            buffer.copyFrom (ch, 0, input.data() + pos, kBlock);
        p.processBlock (buffer, midi);
        for (int i = 0; i < kBlock; ++i)
        {
            const float v = buffer.getSample (0, i);
            finite = finite && std::isfinite (v);
            out[pos + (size_t) i] = v;
        }
    }

    const int start = (int) input.size() - (int) kSampleRate;   // last second
    double ein = 0, eout = 0;
    for (size_t i = (size_t) start; i < input.size(); ++i) { ein += input[i] * input[i]; eout += out[i] * out[i]; }
    return { measurePitch (out, start, 8192), (float) (10.0 * std::log10 ((eout + 1e-12) / (ein + 1e-12))), finite };
}

void set (AutoMechProcessor& p, const juce::String& id, float v) { p.setParam (id, v); }

int runTests()
{
    using namespace automech;
    int failures = 0;

    auto check = [&failures] (const char* name, float got, float want, float tolCents, float gainDb, bool finite)
    {
        const float cents = got > 0 ? 1200.0f * std::log2 (got / want) : 9999.0f;
        const bool ok = std::abs (cents) <= tolCents && finite && gainDb > -6.0f && gainDb < 6.0f;
        std::printf ("%-44s out %8.2f Hz  want %8.2f Hz  (%+7.1f ct, gain %+5.1f dB)  %s\n",
                     name, got, want, cents, gainDb, ok ? "PASS" : "FAIL");
        if (! ok) ++failures;
    };

    struct Case { const char* name; float in; float want; };
    const Case cases[] = {
        { "C major, A4 +46 ct  -> A4",        451.9f, 440.00f },
        { "C major, A4 -60 ct  -> A4",        425.3f, 440.00f },
        { "C major, C4 -44 ct  -> C4",        255.1f, 261.63f },
        { "C major, A2 -80 ct  -> A2 (male)", 105.0f, 110.00f },
        { "C major, E3 +30 ct  -> E3",        166.9f, 164.81f },
    };

    for (auto& c : cases)
    {
        AutoMechProcessor p;
        set (p, ids::key, 0); set (p, ids::scale, scaleMajor); set (p, ids::voiceType, 4);
        auto r = run (p, makeVoice (c.in, 3.0));
        check (c.name, r.pitch, c.want, 8.0f, r.gainDb, r.finite);
    }

    {
        AutoMechProcessor p;
        set (p, ids::correction, 0); set (p, ids::scale, scaleMajor);
        auto r = run (p, makeVoice (451.9f, 3.0));
        check ("Correction 0% leaves pitch alone", r.pitch, 451.9f, 8.0f, r.gainDb, r.finite);
    }
    {
        AutoMechProcessor p;
        set (p, ids::bypass, 1);
        auto r = run (p, makeVoice (451.9f, 3.0));
        check ("Bypass passes dry signal", r.pitch, 451.9f, 3.0f, r.gainDb, r.finite);
    }
    {
        AutoMechProcessor p;
        set (p, ids::key, 0); set (p, ids::mechMode, 1); set (p, ids::mechMix, 100);
        auto r = run (p, makeVoice (451.9f, 3.0));
        check ("Mech robot flattens to key root (C5)", r.pitch, 523.25f, 10.0f, r.gainDb, r.finite);
    }
    {
        AutoMechProcessor p;
        set (p, ids::key, 0); set (p, ids::scale, scaleMajor); set (p, ids::speed, 50);
        auto r = run (p, makeVoice (451.9f, 3.0));
        check ("Speed 50 ms still settles on A4", r.pitch, 440.0f, 8.0f, r.gainDb, r.finite);
    }
    {
        AutoMechProcessor p;
        set (p, ids::key, 0); set (p, ids::scale, scaleMajor);
        set (p, ids::harmInterval (0), 9); set (p, ids::harmLevel (0), 70);
        set (p, ids::formant, 4); set (p, ids::drive, 30); set (p, ids::cleanOn, 1); set (p, ids::cleanup, 40);
        auto r = run (p, makeVoice (451.9f, 3.0));
        std::printf ("%-44s finite=%s gain %+5.1f dB  %s\n", "Harmony + formant + drive + clean (stability)",
                     r.finite ? "yes" : "no", r.gainDb, r.finite ? "PASS" : "FAIL");
        if (! r.finite) ++failures;
    }

    {
        AutoMechProcessor p;
        p.prepareToPlay (kSampleRate, kBlock);
        std::printf ("Reported latency: %d samples (%.1f ms)\n", p.getLatencySamples(), 1000.0 * p.getLatencySamples() / kSampleRate);
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILURES", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}

int snapshot (const juce::String& path, int visMode)
{
    juce::ScopedJuceInitialiser_GUI gui;
    AutoMechProcessor p;
    p.prepareToPlay (kSampleRate, kBlock);
    p.setParam (automech::ids::scale, automech::scaleMajor);

    std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
    auto* ed = dynamic_cast<AutoMechEditor*> (editor.get());
    for (int m = 0; m < visMode && ed != nullptr; ++m) ed->cycleVisualizerMode();

    // A sung phrase (A2 slightly flat, then up to C3) feeds the meters & visualizer.
    const float notes[] = { 108.0f, 108.5f, 109.0f, 128.0f, 129.5f, 131.0f };
    juce::AudioBuffer<float> buffer (2, kBlock);
    juce::MidiBuffer midi;
    const int blocksPerFrame = (int) (kSampleRate / 60.0 / kBlock) + 1;
    int frame = 0;
    for (float hz : notes)
    {
        auto voice = makeVoice (hz, 0.5);
        for (size_t pos = 0; pos + kBlock <= voice.size(); pos += kBlock)
        {
            for (int ch = 0; ch < 2; ++ch) buffer.copyFrom (ch, 0, voice.data() + pos, kBlock);
            p.processBlock (buffer, midi);
            if (++frame % blocksPerFrame == 0 && ed != nullptr) ed->pollProcessor();
        }
    }
    // Finish on a held A2 so the display shows a locked note.
    auto tail = makeVoice (109.2f, 0.8);
    for (size_t pos = 0; pos + kBlock <= tail.size(); pos += kBlock)
    {
        for (int ch = 0; ch < 2; ++ch) buffer.copyFrom (ch, 0, tail.data() + pos, kBlock);
        p.processBlock (buffer, midi);
        if (++frame % blocksPerFrame == 0 && ed != nullptr) ed->pollProcessor();
    }

    auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
    juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile (path);
    out.deleteFile();
    juce::FileOutputStream stream (out);
    juce::PNGImageFormat png;
    const bool ok = stream.openedOk() && png.writeImageToStream (image, stream);
    std::printf ("%s %s\n", ok ? "Wrote" : "Failed to write", out.getFullPathName().toRawUTF8());
    editor.reset();
    return ok ? 0 : 1;
}
} // namespace

int main (int argc, char** argv)
{
    const juce::String mode = argc > 1 ? argv[1] : "test";
    if (mode == "snapshot")
        return snapshot (argc > 2 ? argv[2] : "automech.png", argc > 3 ? juce::String (argv[3]).getIntValue() : 0);
    return runTests();
}
