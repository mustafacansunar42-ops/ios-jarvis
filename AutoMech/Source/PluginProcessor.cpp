#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

using namespace automech;

namespace
{
using Attr = juce::AudioParameterFloatAttributes;

std::unique_ptr<juce::AudioParameterFloat> floatParam (const juce::String& id, const juce::String& name,
                                                       juce::NormalisableRange<float> range, float def,
                                                       const char* unit, bool showSign = false)
{
    const juce::String unitStr (unit);
    return std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { id, 1 }, name, range, def,
        Attr().withLabel (unitStr.trim())
              .withStringFromValueFunction ([unitStr, showSign] (float v, int)
              {
                  const int i = (int) std::lround (v);
                  return (showSign && i > 0 ? "+" : "") + juce::String (i) + unitStr;
              })
              .withValueFromStringFunction ([] (const juce::String& s) { return s.getFloatValue(); }));
}

juce::NormalisableRange<float> skewed (float lo, float hi, float centre)
{
    juce::NormalisableRange<float> r (lo, hi);
    r.setSkewForCentre (centre);
    return r;
}

// Krumhansl-Kessler key profiles.
constexpr float kMajorProfile[12] = { 6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f };
constexpr float kMinorProfile[12] = { 6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f };

float correlate (const std::array<float, 12>& h, const float* profile, int key)
{
    float mh = 0, mp = 0;
    for (int i = 0; i < 12; ++i) { mh += h[(size_t) i]; mp += profile[i]; }
    mh /= 12.0f; mp /= 12.0f;

    float num = 0, dh = 0, dp = 0;
    for (int i = 0; i < 12; ++i)
    {
        const float a = h[(size_t) ((i + key) % 12)] - mh;
        const float b = profile[i] - mp;
        num += a * b; dh += a * a; dp += b * b;
    }
    return (dh > 0 && dp > 0) ? num / std::sqrt (dh * dp) : 0.0f;
}
} // namespace

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout AutoMechProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    auto choice = [&] (const juce::String& id, const juce::String& name, const juce::StringArray& items, int def)
    {
        layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name, items, def));
    };
    auto boolean = [&] (const juce::String& id, const juce::String& name, bool def)
    {
        layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def));
    };

    // Main
    choice (ids::key, "Key", keyNames(), 0);
    choice (ids::scale, "Scale", scaleNames(), scaleChromatic);
    layout.add (floatParam (ids::reference, "Reference", { 415.0f, 466.0f, 1.0f }, 440.0f, " Hz"));
    layout.add (floatParam (ids::correction, "Correction", { 0.0f, 100.0f }, 100.0f, "%"));
    layout.add (floatParam (ids::speed, "Speed", skewed (0.0f, 400.0f, 60.0f), 0.0f, " ms"));
    layout.add (floatParam (ids::noteTrans, "Note Transition", skewed (0.0f, 400.0f, 60.0f), 0.0f, " ms"));
    layout.add (floatParam (ids::flex, "Flex", { 0.0f, 100.0f }, 0.0f, " ct"));
    layout.add (floatParam (ids::human, "Human", { 0.0f, 100.0f }, 0.0f, "%"));
    layout.add (floatParam (ids::formant, "Formant", { -12.0f, 12.0f, 0.1f }, 0.0f, " st", true));
    layout.add (floatParam (ids::vibrato, "Vibrato", { -100.0f, 100.0f }, 0.0f, " ct", true));
    layout.add (floatParam (ids::mix, "Mix", { 0.0f, 100.0f }, 100.0f, "%"));
    boolean (ids::bypass, "Bypass", false);

    // Clean
    boolean (ids::cleanOn, "Clean", false);
    layout.add (floatParam (ids::cleanup, "Cleanup", { 0.0f, 100.0f }, 0.0f, "%"));
    layout.add (floatParam (ids::lowCut, "Low Cut", skewed (20.0f, 400.0f, 100.0f), 20.0f, " Hz"));
    choice (ids::hum, "Hum", { "Off", "50 Hz", "60 Hz" }, 0);
    layout.add (floatParam (ids::tighten, "Tighten", { 0.0f, 100.0f }, 0.0f, "%"));
    layout.add (floatParam (ids::onset, "Onset", { 0.0f, 100.0f }, 0.0f, "%"));
    layout.add (floatParam (ids::sensitivity, "Sensitivity", { 0.0f, 100.0f }, 60.0f, "%"));

    // Voice
    choice (ids::voiceType, "Voice Type", voiceTypeNames(), 2);
    boolean (ids::formantPreserve, "Formant Preserve", true);

    // Vibrato
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ids::vibRate, 1 }, "Vibrato Rate", juce::NormalisableRange<float> (1.0f, 10.0f, 0.1f), 5.5f,
        Attr().withLabel ("Hz").withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " Hz"; })));
    layout.add (floatParam (ids::vibDelay, "Vibrato Delay", { 0.0f, 1000.0f }, 200.0f, " ms"));
    choice (ids::vibShape, "Vibrato Shape", { "Sine", "Triangle", "Square" }, 0);

    // Harmony
    for (int h = 0; h < 3; ++h)
    {
        const auto n = juce::String (h + 1);
        choice (ids::harmInterval (h), "Harmony " + n + " Interval", harmonyIntervalNames(), 7);
        layout.add (floatParam (ids::harmLevel (h), "Harmony " + n + " Level", { 0.0f, 100.0f }, 70.0f, "%"));
        layout.add (floatParam (ids::harmPan (h), "Harmony " + n + " Pan", { -100.0f, 100.0f },
                                h == 0 ? -50.0f : (h == 1 ? 50.0f : 0.0f), "", true));
    }

    // Mech
    choice (ids::mechMode, "Mech Mode", { "Off", "Robot", "Ring", "Comb" }, 0);
    layout.add (floatParam (ids::mechFreq, "Mech Freq", skewed (20.0f, 2000.0f, 200.0f), 110.0f, " Hz"));
    layout.add (floatParam (ids::mechMix, "Mech Mix", { 0.0f, 100.0f }, 100.0f, "%"));

    // FX
    layout.add (floatParam (ids::drive, "Drive", { 0.0f, 100.0f }, 0.0f, "%"));
    layout.add (floatParam (ids::width, "Width", { 0.0f, 200.0f }, 100.0f, "%"));

    // MIDI
    boolean (ids::midiFollow, "MIDI Follow", false);

    // Custom scale notes
    for (int n = 0; n < 12; ++n)
        boolean (ids::noteEnabled (n), "Note " + keyNames()[n], true);

    return layout;
}

//==============================================================================
AutoMechProcessor::AutoMechProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "AUTOMECH", createLayout())
{
    auto p = [this] (const juce::String& id) { return apvts.getRawParameterValue (id); };

    raw.key = p (ids::key);               raw.scale = p (ids::scale);           raw.reference = p (ids::reference);
    raw.correction = p (ids::correction); raw.speed = p (ids::speed);           raw.noteTrans = p (ids::noteTrans);
    raw.flex = p (ids::flex);             raw.human = p (ids::human);           raw.formant = p (ids::formant);
    raw.vibrato = p (ids::vibrato);       raw.mix = p (ids::mix);               raw.bypass = p (ids::bypass);
    raw.cleanOn = p (ids::cleanOn);       raw.cleanup = p (ids::cleanup);       raw.lowCut = p (ids::lowCut);
    raw.hum = p (ids::hum);               raw.tighten = p (ids::tighten);       raw.onset = p (ids::onset);
    raw.sensitivity = p (ids::sensitivity);
    raw.voiceType = p (ids::voiceType);   raw.formantPreserve = p (ids::formantPreserve);
    raw.vibRate = p (ids::vibRate);       raw.vibDelay = p (ids::vibDelay);     raw.vibShape = p (ids::vibShape);
    raw.mechMode = p (ids::mechMode);     raw.mechFreq = p (ids::mechFreq);     raw.mechMix = p (ids::mechMix);
    raw.drive = p (ids::drive);           raw.width = p (ids::width);           raw.midiFollow = p (ids::midiFollow);

    for (int h = 0; h < 3; ++h)
    {
        raw.harmInterval[(size_t) h] = p (ids::harmInterval (h));
        raw.harmLevel[(size_t) h] = p (ids::harmLevel (h));
        raw.harmPan[(size_t) h] = p (ids::harmPan (h));
    }
    for (int n = 0; n < 12; ++n)
        raw.noteEnabled[(size_t) n] = p (ids::noteEnabled (n));

    visBuffer.assign ((size_t) visFifo.getTotalSize(), 0.0f);
}

bool AutoMechProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void AutoMechProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;

    detector.prepare (sampleRate);
    shifter.prepare (sampleRate);
    latency = shifter.getLatencySamples();
    setLatencySamples (latency);

    int size = 1;
    while (size < latency + 8) size <<= 1;
    for (auto& r : dryRing) r.assign ((size_t) size, 0.0f);
    dryMask = size - 1;
    dryWrite = 0;

    combBuffer.assign ((size_t) (sampleRate / 20.0) + 8, 0.0f);
    combWrite = 0;
    ringPhase = 0.0;

    lowCutFilter.reset();
    for (auto& f : humFilters) f.reset();
    lastLowCut = -1.0f;
    lastHum = lastVoiceType = -1;
    gateEnv = 0.0f;
    gateGain = 1.0f;

    controlCounter = 0;
    wasVoiced = false;
    currentNote = -1.0f;
    smoothedOffset = 0.0f;
    mainRatio = 1.0f;
    harmActive = { false, false, false };

    const double rampSeconds = 0.03;
    mixSmooth.reset (sampleRate, rampSeconds);          mixSmooth.setCurrentAndTargetValue (raw.mix->load() * 0.01f);
    bypassSmooth.reset (sampleRate, rampSeconds);       bypassSmooth.setCurrentAndTargetValue (raw.bypass->load());
    formantSmooth.reset (sampleRate, rampSeconds);      formantSmooth.setCurrentAndTargetValue (raw.formant->load());
    driveSmooth.reset (sampleRate, rampSeconds);        driveSmooth.setCurrentAndTargetValue (raw.drive->load() * 0.01f);
    widthSmooth.reset (sampleRate, rampSeconds);        widthSmooth.setCurrentAndTargetValue (raw.width->load() * 0.01f);
    mechMixSmooth.reset (sampleRate, rampSeconds);      mechMixSmooth.setCurrentAndTargetValue (raw.mechMix->load() * 0.01f);
    for (auto& g : harmGain) { g.reset (sampleRate, 0.05); g.setCurrentAndTargetValue (0.0f); }

    numHeld = 0;
    updateFilters();
}

float AutoMechProcessor::param (const char* id) const noexcept
{
    return apvts.getRawParameterValue (id)->load();
}

uint16_t AutoMechProcessor::currentScaleMask() const noexcept
{
    uint16_t custom = 0;
    for (int n = 0; n < 12; ++n)
        if (raw.noteEnabled[(size_t) n]->load() > 0.5f)
            custom = (uint16_t) (custom | (1u << (unsigned) n));

    return ScaleQuantizer::absoluteMask ((int) raw.key->load(), (int) raw.scale->load(), custom);
}

void AutoMechProcessor::updateFilters() noexcept
{
    const float lc = raw.lowCut->load();
    if (std::abs (lc - lastLowCut) > 0.01f)
    {
        lowCutFilter.setHighpass (currentSampleRate, lc, 0.707);
        lastLowCut = lc;
    }

    const int hum = (int) raw.hum->load();
    if (hum != lastHum)
    {
        const double base = hum == 2 ? 60.0 : 50.0;
        for (int i = 0; i < 3; ++i)
            humFilters[(size_t) i].setNotch (currentSampleRate, base * (i + 1), 30.0);
        lastHum = hum;
    }

    const int vt = (int) raw.voiceType->load();
    if (vt != lastVoiceType)
    {
        const auto range = voiceRange (vt);
        detector.setRange (range.first, range.second);
        lastVoiceType = vt;
    }

    detector.setSensitivity (raw.sensitivity->load() * 0.01f);
}

void AutoMechProcessor::handleMidi (const juce::MidiBuffer& midi) noexcept
{
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn())
        {
            if (numHeld < (int) heldNotes.size())
                heldNotes[(size_t) numHeld++] = msg.getNoteNumber();
        }
        else if (msg.isNoteOff() || msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            if (! msg.isNoteOff()) { numHeld = 0; continue; }
            for (int i = 0; i < numHeld; ++i)
            {
                if (heldNotes[(size_t) i] == msg.getNoteNumber())
                {
                    for (int j = i; j < numHeld - 1; ++j) heldNotes[(size_t) j] = heldNotes[(size_t) j + 1];
                    --numHeld;
                    break;
                }
            }
        }
    }
    telemetry.heldMidiNote.store (numHeld > 0 ? heldNotes[(size_t) numHeld - 1] : -1);
}

//==============================================================================
void AutoMechProcessor::updateControl() noexcept
{
    const float sr = (float) currentSampleRate;
    const float dtMs = 1000.0f * kControlInterval / sr;
    const bool voiced = detector.isVoiced();
    const float f0 = detector.getFrequency();
    const float ref = raw.reference->load();
    const int mechMode = (int) raw.mechMode->load();
    const float mechAmt = raw.mechMix->load() * 0.01f;

    if (voiced && f0 > 0.0f)
    {
        unvoicedTicks = 0;
        detMidi = 69.0f + 12.0f * std::log2 (f0 / ref);

        if (! wasVoiced)
        {
            detSmooth = detMidi;
            if (currentNote < 0.0f) noteTimeMs = 0.0f;
        }

        // ---- target note
        const float tighten = raw.tighten->load() * 0.01f;
        const float hysteresis = 0.35f - 0.30f * tighten;
        float note;

        if (raw.midiFollow->load() > 0.5f && numHeld > 0)
            note = ScaleQuantizer::foldNear (heldNotes[(size_t) numHeld - 1] % 12, detMidi);
        else
            note = ScaleQuantizer::nearest (detMidi, blockMask, currentNote, hysteresis);

        if (std::abs (note - currentNote) > 0.01f)
        {
            glideFrom = currentNote < 0.0f ? note : glide;
            glideProgress = currentNote < 0.0f ? 1.0f : 0.0f;
            currentNote = note;
            noteTimeMs = 0.0f;
        }

        const float transMs = raw.noteTrans->load();
        glideProgress = transMs <= 0.0f ? 1.0f : std::min (1.0f, glideProgress + dtMs / transMs);
        glide = glideFrom + (currentNote - glideFrom) * glideProgress;

        // ---- humanise / flex / onset
        detSmooth += (detMidi - detSmooth) * (1.0f - std::exp (-dtMs / 150.0f));
        const float variation = detMidi - detSmooth;
        const float desired = glide + raw.human->load() * 0.01f * variation;

        const float flex = raw.flex->load();
        const float devCents = std::abs (detMidi - glide) * 100.0f;
        float flexFactor = 1.0f;
        if (flex > 0.0f)
        {
            const float x = std::min (1.0f, devCents / flex);
            flexFactor = x * x;
        }

        const float onsetMs = raw.onset->load() * 2.0f;
        const float onsetFactor = onsetMs > 0.0f ? std::min (1.0f, noteTimeMs / onsetMs) : 1.0f;

        const float amount = raw.correction->load() * 0.01f * flexFactor * onsetFactor;
        float target = detMidi + amount * (desired - detMidi);

        // ---- vibrato
        const float depth = raw.vibrato->load() * 0.01f;
        const float vibDelay = raw.vibDelay->load();
        if (std::abs (depth) > 0.0001f && noteTimeMs > vibDelay)
        {
            vibPhase += raw.vibRate->load() * dtMs * 0.001f;
            vibPhase -= std::floor (vibPhase);
            float lfo;
            switch ((int) raw.vibShape->load())
            {
                case 1:  lfo = 1.0f - 4.0f * std::abs (vibPhase - 0.5f); break;
                case 2:  lfo = vibPhase < 0.5f ? 1.0f : -1.0f; break;
                default: lfo = std::sin (juce::MathConstants<float>::twoPi * vibPhase); break;
            }
            const float fadeIn = std::min (1.0f, (noteTimeMs - vibDelay) / 250.0f);
            target += lfo * depth * fadeIn;
        }
        else if (std::abs (depth) <= 0.0001f)
        {
            vibPhase = 0.0f;
        }

        // ---- mech robot: flatten everything to the key root
        float speedMs = raw.speed->load();
        if (mechMode == 1)
        {
            const float robotNote = ScaleQuantizer::foldNear ((int) raw.key->load(), detSmooth);
            target += (robotNote - target) * mechAmt;
            speedMs *= (1.0f - mechAmt);
        }

        // ---- retune speed
        const float offset = target - detMidi;
        smoothedOffset = speedMs <= 0.0f ? offset
                                         : smoothedOffset + (offset - smoothedOffset) * (1.0f - std::exp (-dtMs / speedMs));
        smoothedOffset = juce::jlimit (-24.0f, 24.0f, smoothedOffset);
        outMidi = detMidi + smoothedOffset;
        mainRatio = std::pow (2.0f, smoothedOffset / 12.0f);

        // ---- harmonies
        for (int h = 0; h < 3; ++h)
        {
            const int steps = (int) raw.harmInterval[(size_t) h]->load() - 7;
            harmActive[(size_t) h] = steps != 0;
            if (steps != 0)
            {
                const int hn = ScaleQuantizer::stepFrom ((int) currentNote, steps, blockMask);
                const float hMidi = outMidi + ((float) hn - currentNote);
                harmRatio[(size_t) h] = juce::jlimit (0.25f, 4.0f, std::pow (2.0f, (hMidi - detMidi) / 12.0f));
            }
        }

        shifter.setPeriod (sr / f0, true);
        noteTimeMs += dtMs;

        // ---- learn
        if (learning.load())
        {
            const int pc = ((int) std::lround (detMidi) % 12 + 12) % 12;
            learnHistogram[(size_t) pc] += detector.getConfidence();
        }
    }
    else
    {
        smoothedOffset *= std::exp (-dtMs / 20.0f);
        mainRatio = 1.0f;
        harmActive = { false, false, false };
        shifter.setPeriod (0.0f, false);

        if (++unvoicedTicks * dtMs > 80.0f)
            currentNote = -1.0f;
    }

    if (learning.load())
    {
        learnSamplesLeft -= kControlInterval;
        if (learnSamplesLeft <= 0)
            finishLearn();
    }

    wasVoiced = voiced;

    telemetry.detectedHz.store (voiced ? f0 : 0.0f);
    telemetry.detectedMidi.store (detMidi);
    telemetry.outputMidi.store (outMidi);
    telemetry.targetNote.store (currentNote);
    telemetry.confidence.store (detector.getConfidence());
    telemetry.voiced.store (voiced);
    telemetry.locked.store (voiced && currentNote >= 0.0f && std::abs (outMidi - currentNote) < 0.1f);
}

//==============================================================================
void AutoMechProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, numSamples);

    handleMidi (midi);
    updateFilters();
    blockMask = currentScaleMask();

    mixSmooth.setTargetValue (raw.mix->load() * 0.01f);
    bypassSmooth.setTargetValue (raw.bypass->load() > 0.5f ? 1.0f : 0.0f);
    formantSmooth.setTargetValue (raw.formant->load());
    driveSmooth.setTargetValue (raw.drive->load() * 0.01f);
    widthSmooth.setTargetValue (raw.width->load() * 0.01f);
    mechMixSmooth.setTargetValue (raw.mechMix->load() * 0.01f);

    const bool cleanOn = raw.cleanOn->load() > 0.5f;
    const bool humOn = (int) raw.hum->load() != 0;
    const float cleanup = raw.cleanup->load() * 0.01f;
    const float gateThreshold = juce::Decibels::decibelsToGain (-80.0f + 45.0f * cleanup);
    const float sr = (float) currentSampleRate;
    const float envRelease = std::exp (-1.0f / (0.05f * sr));
    const float gateAttack = 1.0f - std::exp (-1.0f / (0.002f * sr));
    const float gateRelease = 1.0f - std::exp (-1.0f / (0.08f * sr));
    const bool formantPreserve = raw.formantPreserve->load() > 0.5f;
    const int mechMode = (int) raw.mechMode->load();
    const float mechFreq = raw.mechFreq->load();
    const int combDelay = juce::jlimit (1, (int) combBuffer.size() - 1, (int) std::lround (sr / mechFreq));

    float panL[3], panR[3];
    for (int h = 0; h < 3; ++h)
    {
        const float angle = (raw.harmPan[(size_t) h]->load() * 0.01f + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        panL[h] = std::cos (angle);
        panR[h] = std::sin (angle);
    }

    const float* inL = buffer.getReadPointer (0);
    const float* inR = numIn > 1 ? buffer.getReadPointer (1) : inL;
    float* outL = buffer.getWritePointer (0);
    float* outR = numOut > 1 ? buffer.getWritePointer (1) : nullptr;

    float visChunk[512];
    int visCount = 0;

    for (int i = 0; i < numSamples; ++i)
    {
        const float l = inL[i], r = inR[i];

        // ---- latency-aligned dry signal
        dryRing[0][(size_t) dryWrite] = l;
        dryRing[1][(size_t) dryWrite] = r;
        const int dryRead = (dryWrite - latency) & dryMask;
        const float dryL = dryRing[0][(size_t) dryRead];
        const float dryR = dryRing[1][(size_t) dryRead];
        dryWrite = (dryWrite + 1) & dryMask;

        // ---- clean chain
        float x = 0.5f * (l + r);
        if (cleanOn)
        {
            x = lowCutFilter.process (x);
            if (humOn)
                for (auto& f : humFilters) x = f.process (x);

            if (cleanup > 0.0f)
            {
                gateEnv = std::max (std::abs (x), gateEnv * envRelease);
                const float targetGain = gateEnv > gateThreshold ? 1.0f : 0.02f;
                gateGain += (targetGain - gateGain) * (targetGain > gateGain ? gateAttack : gateRelease);
                x *= gateGain;
            }
        }

        detector.push (x);
        shifter.pushSample (x);

        if (++controlCounter >= kControlInterval)
        {
            controlCounter = 0;
            updateControl();
        }

        // ---- correction
        const float formantFactor = std::pow (2.0f, formantSmooth.getNextValue() / 12.0f);
        float wet = shifter.render (0, mainRatio, formantPreserve ? formantFactor : formantFactor * mainRatio);

        // ---- mech
        const float mm = mechMixSmooth.getNextValue();
        if (mechMode == 2)
        {
            ringPhase += mechFreq / sr;
            ringPhase -= std::floor (ringPhase);
            const float ring = wet * std::sin (juce::MathConstants<float>::twoPi * (float) ringPhase);
            wet += (ring - wet) * mm;
        }
        else if (mechMode == 3)
        {
            const int readIdx = (combWrite - combDelay + (int) combBuffer.size()) % (int) combBuffer.size();
            const float comb = wet + 0.82f * combBuffer[(size_t) readIdx];
            combBuffer[(size_t) combWrite] = comb;
            combWrite = (combWrite + 1) % (int) combBuffer.size();
            wet += (comb * 0.45f - wet) * mm;
        }

        float wl = wet, wr = wet;

        // ---- harmonies
        for (int h = 0; h < 3; ++h)
        {
            auto& gain = harmGain[(size_t) h];
            gain.setTargetValue (harmActive[(size_t) h] ? raw.harmLevel[(size_t) h]->load() * 0.01f : 0.0f);
            const float g = gain.getNextValue();
            if (g > 0.0001f || gain.isSmoothing())
            {
                const float ratio = harmRatio[(size_t) h];
                const float s = shifter.render (h + 1, ratio, formantPreserve ? formantFactor : formantFactor * ratio) * g;
                wl += s * panL[h];
                wr += s * panR[h];
            }
        }

        // ---- fx
        const float drive = driveSmooth.getNextValue();
        if (drive > 0.001f)
        {
            const float k = 1.0f + drive * 9.0f;
            const float norm = 1.0f / std::tanh (k);
            wl = std::tanh (wl * k) * norm;
            wr = std::tanh (wr * k) * norm;
        }

        const float width = widthSmooth.getNextValue();
        const float mid = 0.5f * (wl + wr), side = 0.5f * (wl - wr) * width;
        wl = mid + side;
        wr = mid - side;

        // ---- mix & bypass
        const float mix = mixSmooth.getNextValue();
        float ol = dryL + (wl - dryL) * mix;
        float orr = dryR + (wr - dryR) * mix;
        const float b = bypassSmooth.getNextValue();
        ol += (dryL - ol) * b;
        orr += (dryR - orr) * b;

        if (outR != nullptr) { outL[i] = ol; outR[i] = orr; }
        else                 { outL[i] = 0.5f * (ol + orr); }

        visChunk[visCount++] = 0.5f * (ol + orr);
        if (visCount == 512) { pushVisualiser (visChunk, visCount); visCount = 0; }
    }

    if (visCount > 0)
        pushVisualiser (visChunk, visCount);
}

//==============================================================================
void AutoMechProcessor::pushVisualiser (const float* data, int n) noexcept
{
    const auto scope = visFifo.write (std::min (n, visFifo.getFreeSpace()));
    if (scope.blockSize1 > 0) std::copy (data, data + scope.blockSize1, visBuffer.data() + scope.startIndex1);
    if (scope.blockSize2 > 0) std::copy (data + scope.blockSize1, data + scope.blockSize1 + scope.blockSize2, visBuffer.data() + scope.startIndex2);
}

int AutoMechProcessor::readVisualiserSamples (float* dest, int maxSamples)
{
    const auto scope = visFifo.read (std::min (maxSamples, visFifo.getNumReady()));
    if (scope.blockSize1 > 0) std::copy (visBuffer.data() + scope.startIndex1, visBuffer.data() + scope.startIndex1 + scope.blockSize1, dest);
    if (scope.blockSize2 > 0) std::copy (visBuffer.data() + scope.startIndex2, visBuffer.data() + scope.startIndex2 + scope.blockSize2, dest + scope.blockSize1);
    return scope.blockSize1 + scope.blockSize2;
}

//==============================================================================
void AutoMechProcessor::startLearn()
{
    learnHistogram.fill (0.0f);
    learnSamplesLeft = (int) (currentSampleRate * 5.0);
    learnReady.store (false);
    learning.store (true);
}

void AutoMechProcessor::finishLearn() noexcept
{
    learning.store (false);

    float total = 0.0f;
    for (auto v : learnHistogram) total += v;
    if (total <= 0.0f)
        return;   // nothing sung: keep the current key

    float best = -2.0f;
    int bestKey = 0, bestScale = scaleMajor;
    for (int k = 0; k < 12; ++k)
    {
        const float maj = correlate (learnHistogram, kMajorProfile, k);
        const float min = correlate (learnHistogram, kMinorProfile, k);
        if (maj > best) { best = maj; bestKey = k; bestScale = scaleMajor; }
        if (min > best) { best = min; bestKey = k; bestScale = scaleMinor; }
    }

    learnKey.store (bestKey);
    learnScale.store (bestScale);
    learnReady.store (true);
}

bool AutoMechProcessor::popLearnResult (int& key, int& scale)
{
    if (! learnReady.exchange (false))
        return false;
    key = learnKey.load();
    scale = learnScale.load();
    return true;
}

//==============================================================================
void AutoMechProcessor::setParam (const juce::String& id, float plainValue)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
}

const juce::StringArray& AutoMechProcessor::factoryPresetNames()
{
    static const juce::StringArray names { "Natural", "Hard Tune", "Robot Voice", "Soft Pop", "Choir Stack" };
    return names;
}

void AutoMechProcessor::applyFactoryPreset (int index)
{
    // Everything back to default first (key / scale / reference are kept).
    for (auto* p : getParameters())
    {
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            const auto id = rp->getParameterID();
            if (id != ids::key && id != ids::scale && id != ids::reference && ! id.startsWith ("noteEn"))
                rp->setValueNotifyingHost (rp->getDefaultValue());
        }
    }

    switch (index)
    {
        case 0: // Natural
            setParam (ids::correction, 75); setParam (ids::speed, 60); setParam (ids::noteTrans, 80);
            setParam (ids::human, 40); setParam (ids::flex, 15);
            break;
        case 1: // Hard Tune
            setParam (ids::correction, 100); setParam (ids::speed, 0); setParam (ids::noteTrans, 0);
            setParam (ids::tighten, 80);
            break;
        case 2: // Robot Voice
            setParam (ids::mechMode, 1); setParam (ids::mechMix, 100); setParam (ids::drive, 15);
            break;
        case 3: // Soft Pop
            setParam (ids::correction, 85); setParam (ids::speed, 25); setParam (ids::noteTrans, 40);
            setParam (ids::human, 20); setParam (ids::vibrato, 12);
            break;
        case 4: // Choir Stack
            setParam (ids::correction, 90); setParam (ids::speed, 20);
            setParam (ids::harmInterval (0), 9);  setParam (ids::harmLevel (0), 60); setParam (ids::harmPan (0), -60);
            setParam (ids::harmInterval (1), 11); setParam (ids::harmLevel (1), 50); setParam (ids::harmPan (1), 60);
            setParam (ids::harmInterval (2), 4);  setParam (ids::harmLevel (2), 40); setParam (ids::harmPan (2), 0);
            setParam (ids::width, 140);
            break;
        default: break;
    }
}

int AutoMechProcessor::getNumPrograms() { return factoryPresetNames().size(); }

void AutoMechProcessor::setCurrentProgram (int index)
{
    if (juce::isPositiveAndBelow (index, getNumPrograms()))
    {
        currentProgram = index;
        applyFactoryPreset (index);
    }
}

const juce::String AutoMechProcessor::getProgramName (int index)
{
    return factoryPresetNames()[index];
}

bool AutoMechProcessor::saveUserPreset (const juce::File& file)
{
    if (auto xml = apvts.copyState().createXml())
        return xml->writeTo (file);
    return false;
}

bool AutoMechProcessor::loadUserPreset (const juce::File& file)
{
    if (auto xml = juce::XmlDocument::parse (file))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            return true;
        }
    }
    return false;
}

void AutoMechProcessor::toggleAB()
{
    abStates[(size_t) abSlot] = apvts.copyState();
    abSlot ^= 1;
    if (abStates[(size_t) abSlot].isValid())
        apvts.replaceState (abStates[(size_t) abSlot].createCopy());
}

//==============================================================================
void AutoMechProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void AutoMechProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorParameter* AutoMechProcessor::getBypassParameter() const
{
    return apvts.getParameter (ids::bypass);
}

juce::AudioProcessorEditor* AutoMechProcessor::createEditor()
{
    return new AutoMechEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AutoMechProcessor();
}
