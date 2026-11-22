// Offline checks for SCORCH: DSP sanity, latency compensation, bypass, state,
// plus PNG snapshots of the editor in each visualizer mode.
// Build with -DSCORCH_BUILD_TESTS=ON, run: ScorchTests [snapshot-output-dir]

#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cstdio>
#include <random>
#include <thread>

namespace
{
int failures = 0;

void check (bool ok, const juce::String& what)
{
    std::printf ("[%s] %s\n", ok ? " OK " : "FAIL", what.toRawUTF8());
    if (! ok)
        ++failures;
}

void setParam (ScorchAudioProcessor& p, const char* id, float value)
{
    auto* param = p.apvts.getParameter (id);
    param->setValueNotifyingHost (param->convertTo0to1 (value));
}

struct RunResult
{
    bool finite = true;
    float peak = 0.0f;
    double inRms = 0.0, outRms = 0.0;
};

/** Feeds a 2-channel sine through the processor using random block sizes. */
RunResult run (ScorchAudioProcessor& p, double fs, float freq, float amplitude, int totalSamples,
               std::vector<float>* captureIn = nullptr, std::vector<float>* captureOut = nullptr)
{
    std::mt19937 rng (1234);
    std::uniform_int_distribution<int> blockDist (1, 1500);

    juce::AudioBuffer<float> buffer (2, 1500);
    juce::MidiBuffer midi;
    RunResult r;
    double inE = 0.0, outE = 0.0;
    int n = 0, pos = 0;

    while (pos < totalSamples)
    {
        const int block = juce::jmin (blockDist (rng), totalSamples - pos);
        buffer.setSize (2, block, false, false, true);

        for (int i = 0; i < block; ++i)
        {
            const float s = amplitude * (float) std::sin (juce::MathConstants<double>::twoPi * freq * (pos + i) / fs);
            buffer.setSample (0, i, s);
            buffer.setSample (1, i, s * 0.9f);
            if (captureIn) captureIn->push_back (s);
        }

        p.processBlock (buffer, midi);

        for (int i = 0; i < block; ++i)
        {
            const float in = amplitude * (float) std::sin (juce::MathConstants<double>::twoPi * freq * (pos + i) / fs);
            const float out = buffer.getSample (0, i);
            r.finite = r.finite && std::isfinite (out) && std::isfinite (buffer.getSample (1, i));
            r.peak = juce::jmax (r.peak, std::abs (out));

            // measure the second half only (after smoothing / auto-gain settle)
            if (pos + i > totalSamples / 2)
            {
                inE += (double) in * in;
                outE += (double) out * out;
                ++n;
            }

            if (captureOut) captureOut->push_back (out);
        }

        pos += block;
    }

    r.inRms = juce::Decibels::gainToDecibels (std::sqrt (inE / juce::jmax (1, n)));
    r.outRms = juce::Decibels::gainToDecibels (std::sqrt (outE / juce::jmax (1, n)));
    return r;
}

void resetToDefaults (ScorchAudioProcessor& p)
{
    for (auto* param : p.getParameters())
        param->setValueNotifyingHost (param->getDefaultValue());
}

void dspTests()
{
    const double fs = 48000.0;
    const juce::StringArray models { "Tube", "Tape", "Transistor", "Fuzz", "Fry" };

    for (int os = 0; os < 4; ++os)
    {
        for (int m = 0; m < 5; ++m)
        {
            ScorchAudioProcessor p;
            resetToDefaults (p);
            setParam (p, scorch::params::id::model, (float) m);
            setParam (p, scorch::params::id::oversampling, (float) os);
            setParam (p, scorch::params::id::drive, 36.0f);
            setParam (p, scorch::params::id::heat, 80.0f);
            setParam (p, scorch::params::id::bias, 40.0f);
            setParam (p, scorch::params::id::crunch, 60.0f);
            p.prepareToPlay (fs, 512);

            const auto r = run (p, fs, 220.0f, 0.25f, (int) fs);
            check (r.finite && r.peak < 4.0f && std::abs (r.outRms - r.inRms) < 3.0,
                   juce::String::formatted ("%-10s %dx  latency %3d  in %6.1f dB  out %6.1f dB  peak %.2f",
                                            models[m].toRawUTF8(), 1 << os, p.getLatencySamples(),
                                            r.inRms, r.outRms, r.peak));
        }
    }

    // Dry path is delay-compensated: with mix at 0 the output is the input delayed by the reported latency.
    for (int os = 0; os < 4; ++os)
    {
        ScorchAudioProcessor p;
        resetToDefaults (p);
        setParam (p, scorch::params::id::mix, 0.0f);
        setParam (p, scorch::params::id::autoGain, 0.0f);
        setParam (p, scorch::params::id::oversampling, (float) os);
        p.prepareToPlay (fs, 512);

        std::vector<float> in, out;
        run (p, fs, 440.0f, 0.5f, 24000, &in, &out);
        const int lat = p.getLatencySamples();
        float maxErr = 0.0f;
        for (size_t i = (size_t) lat + 4000; i < out.size(); ++i)
            maxErr = juce::jmax (maxErr, std::abs (out[i] - in[i - (size_t) lat]));

        check (maxErr < 1.0e-5f, juce::String::formatted ("dry path aligned at %dx (latency %d, max error %.2g)", 1 << os, lat, (double) maxErr));
    }

    // Wet path alignment: at low drive the wet signal should line up with the dry one.
    // Broadband noise, so the low-cut filter's phase lead at low frequencies doesn't dominate.
    {
        ScorchAudioProcessor p;
        resetToDefaults (p);
        setParam (p, scorch::params::id::model, 2.0f);   // transistor, nearly linear at low level
        setParam (p, scorch::params::id::drive, 0.0f);
        setParam (p, scorch::params::id::heat, 0.0f);
        setParam (p, scorch::params::id::autoGain, 0.0f);
        p.prepareToPlay (fs, 512);

        std::mt19937 rng (7);
        std::uniform_real_distribution<float> noise (-0.05f, 0.05f);
        std::vector<float> in, out;
        juce::AudioBuffer<float> buffer (2, 300);
        juce::MidiBuffer midi;

        for (int b = 0; b < 100; ++b)
        {
            for (int i = 0; i < 300; ++i)
            {
                const float s = noise (rng);
                buffer.setSample (0, i, s);
                buffer.setSample (1, i, s);
                in.push_back (s);
            }
            p.processBlock (buffer, midi);
            for (int i = 0; i < 300; ++i)
                out.push_back (buffer.getSample (0, i));
        }

        const int lat = p.getLatencySamples();
        int bestLag = 0; double best = -1.0e9;
        for (int lag = 0; lag < 400; ++lag)
        {
            double acc = 0.0;
            for (size_t i = 12000; i < 28000; ++i)
                acc += (double) out[i] * in[i - (size_t) lag];
            if (acc > best) { best = acc; bestLag = lag; }
        }
        check (bestLag == lat, juce::String::formatted ("wet path lag %d vs reported latency %d", bestLag, lat));
    }

    // Bypass: output equals the delay-compensated input.
    {
        ScorchAudioProcessor p;
        resetToDefaults (p);
        setParam (p, scorch::params::id::drive, 40.0f);
        setParam (p, scorch::params::id::bypass, 1.0f);
        p.prepareToPlay (fs, 512);

        std::vector<float> in, out;
        run (p, fs, 440.0f, 0.5f, 24000, &in, &out);
        const int lat = p.getLatencySamples();
        float maxErr = 0.0f;
        for (size_t i = (size_t) lat + 4000; i < out.size(); ++i)
            maxErr = juce::jmax (maxErr, std::abs (out[i] - in[i - (size_t) lat]));
        check (maxErr < 1.0e-5f, juce::String::formatted ("bypass is clean (max error %.2g)", (double) maxErr));
    }

    // Silence in, silence out (no denormals/NaN, gate closes, DC removed).
    {
        ScorchAudioProcessor p;
        resetToDefaults (p);
        setParam (p, scorch::params::id::model, 3.0f);
        setParam (p, scorch::params::id::bias, 100.0f);
        p.prepareToPlay (fs, 512);
        const auto r = run (p, fs, 100.0f, 0.0f, (int) fs);
        check (r.finite && r.peak < 1.0e-3f, juce::String::formatted ("silence stays silent (peak %.2g)", (double) r.peak));
    }

    // State round trip.
    {
        ScorchAudioProcessor a, b;
        a.setCurrentProgram (3);
        setParam (a, scorch::params::id::tone, -3.3f);
        juce::MemoryBlock state;
        a.getStateInformation (state);
        b.setStateInformation (state.getData(), (int) state.getSize());
        const float tone = b.apvts.getRawParameterValue (scorch::params::id::tone)->load();
        const float model = b.apvts.getRawParameterValue (scorch::params::id::model)->load();
        check (std::abs (tone + 3.3f) < 0.02f && juce::roundToInt (model) == 3 && b.getCurrentProgram() == 3,
               "state save / restore round trip");
    }

    // Presets load.
    {
        ScorchAudioProcessor p;
        for (int i = 0; i < p.getNumPrograms(); ++i)
        {
            p.setCurrentProgram (i);
            const auto& preset = scorch::params::factoryPresets[(size_t) i];
            const float drive = p.apvts.getRawParameterValue (scorch::params::id::drive)->load();
            check (std::abs (drive - preset.drive) < 0.02f, "preset loads: " + p.getProgramName (i));
        }
    }
}

template <typename T>
T* findChild (juce::Component& root)
{
    for (auto* c : root.getChildren())
    {
        if (auto* t = dynamic_cast<T*> (c))
            return t;
        if (auto* t = findChild<T> (*c))
            return t;
    }
    return nullptr;
}

void snapshots (const juce::File& dir)
{
    dir.createDirectory();

    ScorchAudioProcessor p;
    p.setCurrentProgram (0);
    p.prepareToPlay (48000.0, 480);

    std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
    editor->setVisible (true);
    auto* viz = findChild<scorch::PixelVisualizer> (*editor);

    std::atomic<bool> running { true };
    std::thread audio ([&]
    {
        juce::AudioBuffer<float> buffer (2, 480);
        juce::MidiBuffer midi;
        long long t = 0;
        while (running)
        {
            for (int i = 0; i < 480; ++i, ++t)
            {
                const double time = (double) t / 48000.0;
                const float env = 0.6f + 0.4f * (float) std::sin (time * 3.0);
                const float s = env * 0.35f * (float) (std::sin (juce::MathConstants<double>::twoPi * 110.0 * time)
                                                     + 0.4 * std::sin (juce::MathConstants<double>::twoPi * 330.0 * time));
                buffer.setSample (0, i, s);
                buffer.setSample (1, i, s);
            }
            p.processBlock (buffer, midi);
            std::this_thread::sleep_for (std::chrono::milliseconds (10));
        }
    });

    const std::array<std::pair<scorch::PixelVisualizer::Mode, const char*>, 3> modes {{
        { scorch::PixelVisualizer::Mode::Curve,    "curve" },
        { scorch::PixelVisualizer::Mode::Spectrum, "spectrum" },
        { scorch::PixelVisualizer::Mode::Scope,    "scope" },
    }};

    for (const auto& [mode, name] : modes)
    {
        viz->setMode (mode);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (900);

        for (const float scale : { 1.0f, 2.0f })
        {
            const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, scale);
            const auto file = dir.getChildFile (juce::String ("scorch-") + name + (scale > 1.0f ? "@2x" : "") + ".png");
            file.deleteFile();
            juce::FileOutputStream stream (file);
            juce::PNGImageFormat().writeImageToStream (image, stream);
            check (stream.getStatus().wasOk(), "snapshot " + file.getFileName());
        }
    }

    // resized editor keeps aspect ratio
    editor->setSize (570, 690);
    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
    juce::FileOutputStream small (dir.getChildFile ("scorch-small.png"));
    small.setPosition (0); small.truncate();
    juce::PNGImageFormat().writeImageToStream (image, small);

    running = false;
    audio.join();
    editor.reset();
}
} // namespace

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;

    dspTests();

    if (argc > 1)
        snapshots (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
