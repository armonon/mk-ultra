#include "GrainFreeze/Mix/MixEngine.h"
#include <cmath>
#include <vector>

namespace
{
    // Nearest in-tune MIDI note for the Pitch Lock. mode 0 = chromatic (nearest
    // semitone), 1 = nearest note of key+scale, 2 = nearest root pitch class.
    float snapMidi (float midi, int key, int scaleIdx, int mode)
    {
        if (mode == 0)
            return std::round (midi);

        if (mode == 2)
        {
            const float rel = midi - (float) key;
            return (float) key + 12.0f * std::round (rel / 12.0f);
        }

        static const std::vector<std::vector<int>> scales = {
            { 0, 2, 4, 5, 7, 9, 11 },         // Major
            { 0, 2, 3, 5, 7, 8, 10 },         // Minor
            { 0, 2, 3, 5, 7, 8, 11 },         // Harmonic Minor
            { 0, 2, 3, 5, 7, 9, 11 },         // Melodic Minor
            { 0, 2, 3, 5, 7, 9, 10 },         // Dorian
            { 0, 1, 3, 5, 7, 8, 10 },         // Phrygian
            { 0, 2, 4, 6, 7, 9, 11 },         // Lydian
            { 0, 2, 4, 5, 7, 9, 10 },         // Mixolydian
            { 0, 1, 3, 5, 6, 8, 10 },         // Locrian
            { 0, 2, 4, 7, 9 },                // Major Pentatonic
            { 0, 3, 5, 7, 10 },               // Minor Pentatonic
            { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } // Chromatic
        };
        const auto& s = scales[(size_t) juce::jlimit (0, (int) scales.size() - 1, scaleIdx)];

        float best = std::round (midi), bestDist = 1.0e9f;
        const int baseOct = (int) std::floor ((midi - (float) key) / 12.0f);
        for (int oct = baseOct - 1; oct <= baseOct + 1; ++oct)
            for (int iv : s)
            {
                const float cand = (float) key + (float) (oct * 12 + iv);
                const float d = std::abs (cand - midi);
                if (d < bestDist) { bestDist = d; best = cand; }
            }
        return best;
    }
}

namespace gf
{
namespace mix
{

void MixEngine::addParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout)
{
    using namespace juce;
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "pluginOn", 1 }, "Plugin On", true));
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "entropyOn", 1 }, "Texture / Grain On", true));
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "prettifierOn", 1 }, "Beauty & Space On", true));
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "mixEqOn", 1 }, "Mix EQ On", true));
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "pitchMatchOn", 1 }, "Pitch Match On", false));
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "tempoLockOn", 1 }, "Tempo Lock On", false));
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "limiterOn", 1 }, "Limiter On", true));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "dryLevel", 1 }, "Dry Level", NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "entropySend", 1 }, "Texture / Grain Send", NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "entropyReturn", 1 }, "Texture / Grain Return", NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "prettifierSend", 1 }, "Beauty & Space Send", NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "prettifierReturn", 1 }, "Beauty & Space Return", NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "mixOutput", 1 }, "Output Level", NormalisableRange<float> (0.0f, 2.0f, 0.001f), 1.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "chaosBeauty", 1 }, "Chaos Beauty", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "routingMode", 1 }, "Routing Mode",
                                                         StringArray { "Parallel", "Texture / Grain -> Beauty & Space", "Beauty & Space -> Texture / Grain", "Multiband" }, 0));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "ceilingDb", 1 }, "Ceiling",
                                                        NormalisableRange<float> (-20.0f, 0.0f, 0.01f), -0.1f));
    // Mix EQ: 3-band tone shaping + a lo-fi band-limit, all gated by mixEqOn.
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "eqLow", 1 },  "EQ Low",  NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "eqMid", 1 },  "EQ Mid",  NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "eqHigh", 1 }, "EQ High", NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "eqLoFi", 1 }, "EQ Lo-Fi", NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "mixWidth", 1 }, "Stereo Width", NormalisableRange<float> (0.0f, 2.0f, 0.01f), 1.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "mixGlue", 1 }, "Master Glue", NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));

    // Master Pitch Lock: tunes the whole output to a key/scale and overrides
    // Pitch Match. Key/Scale lists mirror the project-wide tuning vocabulary.
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "pitchLockOn", 1 }, "Pitch Lock On", false));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "pitchLockMode", 1 }, "Pitch Lock Mode",
                                                         StringArray { "Chromatic", "Scale", "Root" }, 1));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "pitchLockKey", 1 }, "Pitch Lock Key",
                                                         StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "pitchLockScale", 1 }, "Pitch Lock Scale",
                                                         StringArray { "Major", "Minor", "Harmonic Minor", "Melodic Minor", "Dorian", "Phrygian", "Lydian",
                                                                       "Mixolydian", "Locrian", "Major Pentatonic", "Minor Pentatonic", "Chromatic" }, 1));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "pitchLockAmount", 1 }, "Pitch Lock Amount",
                                                        NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f));
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "pitchLockFormant", 1 }, "Pitch Lock Formant", false));
}

void MixEngine::prepare (double sampleRate, int samplesPerBlock, int numChannels)
{
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, (juce::uint32) numChannels };
    limiter.prepare (spec);
    limiter.setRelease (60.0f);
    limiter.setThreshold (-0.1f);

    // Master glue: musical bus-comp timing; threshold/ratio/makeup follow the knob.
    glueComp.prepare (spec);
    glueComp.setAttack (30.0f);
    glueComp.setRelease (140.0f);
    glueComp.setThreshold (-12.0f);
    glueComp.setRatio (2.0f);
    smoothedGlueMakeup.reset (sampleRate, 0.05);
    smoothedGlueMakeup.setCurrentAndTargetValue (1.0f);
    smoothedOutputGain.reset (sampleRate, 0.02);
    smoothedOutputGain.setCurrentAndTargetValue (1.0f);
    smoothedBypass.reset (sampleRate, 0.02);
    smoothedBypass.setCurrentAndTargetValue (1.0f);
    tmp.setSize (numChannels, samplesPerBlock);

    sampleRateHz = sampleRate;
    for (auto* f : { &eqLowShelf, &eqMidPeak, &eqHighShelf, &eqLoFiLP })
        f->prepare (spec);
    lastEqLow = lastEqMid = lastEqHigh = lastEqLoFi = 1.0e9f; // force first update

    pitchDetector.prepare (sampleRate);
    pitchShifterL.prepare (sampleRate);
    pitchShifterR.prepare (sampleRate);
    smoothedPitchRatio.reset (sampleRate, 0.05);
    smoothedPitchRatio.setCurrentAndTargetValue (1.0f);
    wetSum.setSize (numChannels, samplesPerBlock);

    lockDetector.prepare (sampleRate);
    lockShifterL.prepare (sampleRate);
    lockShifterR.prepare (sampleRate);
    formantShifterL.prepare (sampleRate);
    formantShifterR.prepare (sampleRate);
    smoothedLockRatio.reset (sampleRate, 0.05);
    smoothedLockRatio.setCurrentAndTargetValue (1.0f);
}

void MixEngine::updateEq (const Params& params)
{
    using Coeffs = juce::dsp::IIR::Coefficients<float>;
    auto changed = [] (float a, float b)
    {
        return std::abs (a - b) > 1.0e-6f;
    };

    if (changed (params.eqLow, lastEqLow))
    {
        *eqLowShelf.state = *Coeffs::makeLowShelf (sampleRateHz, 150.0f, 0.7f, juce::Decibels::decibelsToGain (params.eqLow));
        lastEqLow = params.eqLow;
    }
    if (changed (params.eqMid, lastEqMid))
    {
        *eqMidPeak.state = *Coeffs::makePeakFilter (sampleRateHz, 1000.0f, 0.8f, juce::Decibels::decibelsToGain (params.eqMid));
        lastEqMid = params.eqMid;
    }
    if (changed (params.eqHigh, lastEqHigh))
    {
        *eqHighShelf.state = *Coeffs::makeHighShelf (sampleRateHz, 5000.0f, 0.7f, juce::Decibels::decibelsToGain (params.eqHigh));
        lastEqHigh = params.eqHigh;
    }
    if (changed (params.eqLoFi, lastEqLoFi))
    {
        // 0 = transparent (cutoff above audible), 1 = heavily band-limited (~1.2 kHz).
        const float cutoff = juce::jmap (params.eqLoFi, 0.0f, 1.0f, 18000.0f, 1200.0f);
        *eqLoFiLP.state = *Coeffs::makeLowPass (sampleRateHz, juce::jmin (cutoff, (float) sampleRateHz * 0.45f));
        lastEqLoFi = params.eqLoFi;
    }
}

void MixEngine::reset()
{
    limiter.reset();
    glueComp.reset();
    for (auto* f : { &eqLowShelf, &eqMidPeak, &eqHighShelf, &eqLoFiLP })
        f->reset();
    pitchShifterL.reset();
    pitchShifterR.reset();
    lockShifterL.reset();
    lockShifterR.reset();
    formantShifterL.reset();
    formantShifterR.reset();
}

void MixEngine::processParallel (juce::AudioBuffer<float>& output,
                                 const juce::AudioBuffer<float>& dry,
                                 const juce::AudioBuffer<float>& entropyOut,
                                 const juce::AudioBuffer<float>& prettifierOut,
                                 const Params& params)
{
    const int channels = output.getNumChannels();
    const int samples = output.getNumSamples();
    output.clear();

    const float chaosWeight = juce::jlimit (0.0f, 1.0f, 1.0f - params.chaosBeauty) * params.entropyReturn;
    const float beautyWeight = juce::jlimit (0.0f, 1.0f, params.chaosBeauty) * params.prettifierReturn;
    const float dryWeight = params.dryLevel;

    // Dry is summed straight through (never pitch-shifted).
    for (int c = 0; c < channels; ++c)
    {
        const int srcDry = juce::jmin (c, dry.getNumChannels() - 1);
        output.addFrom (c, 0, dry, srcDry, 0, samples, dryWeight);
    }

    // Build the wet sum (entropy + prettifier) into wetSum so Pitch Match can
    // shift it as one signal without touching the dry.
    const int wetChans = juce::jmin (channels, wetSum.getNumChannels());
    for (int c = 0; c < wetChans; ++c)
    {
        wetSum.clear (c, 0, samples);
        const int srcEnt = juce::jmin (c, entropyOut.getNumChannels() - 1);
        const int srcPre = juce::jmin (c, prettifierOut.getNumChannels() - 1);
        if (params.entropyOn)
            wetSum.addFrom (c, 0, entropyOut, srcEnt, 0, samples, chaosWeight);
        if (params.prettifierOn)
            wetSum.addFrom (c, 0, prettifierOut, srcPre, 0, samples, beautyWeight);
    }

    if (params.pitchMatchOn && ! params.pitchLockOn && wetChans > 0)
    {
        // Detect the dry fundamental, snap to the nearest semitone, and shift the
        // wet by the input's detune-to-grid amount (in tune -> ratio 1, no change).
        pitchDetector.push (dry.getReadPointer (0), samples);
        const float f = pitchDetector.estimate();
        float targetRatio = 1.0f;
        if (f > 0.0f)
        {
            const float midi    = 69.0f + 12.0f * std::log2 (f / 440.0f);
            const float snapped = std::round (midi);
            targetRatio = std::pow (2.0f, (snapped - midi) / 12.0f);
        }
        smoothedPitchRatio.setTargetValue (targetRatio);
        const float r = smoothedPitchRatio.skip (samples);
        const bool shifting = std::abs (r - 1.0f) > 0.001f; // skip the comb at unity
        pitchShifterL.setRatio (r);
        pitchShifterR.setRatio (r);
        if (shifting)
        {
            pitchShifterL.process (wetSum.getWritePointer (0), samples);
            if (wetChans > 1) pitchShifterR.process (wetSum.getWritePointer (1), samples);
        }
        else
        {
            pitchShifterL.prime (wetSum.getReadPointer (0), samples);
            if (wetChans > 1) pitchShifterR.prime (wetSum.getReadPointer (1), samples);
        }
    }

    for (int c = 0; c < wetChans; ++c)
        output.addFrom (c, 0, wetSum, c, 0, samples);

    smoothedOutputGain.setTargetValue (params.outputLevel);
    smoothedBypass.setTargetValue (params.pluginOn ? 1.0f : 0.0f);
    for (int i = 0; i < samples; ++i)
    {
        const float g = smoothedOutputGain.getNextValue();
        const float b = smoothedBypass.getNextValue();
        for (int c = 0; c < channels; ++c)
            output.setSample (c, i, output.getSample (c, i) * g * b);
    }

    // Master Pitch Lock overrides Pitch Match: detect the whole output's pitch
    // and tune it onto the chosen key/scale grid (before EQ + limiter).
    if (params.pitchLockOn && channels > 0)
    {
        lockDetector.push (output.getReadPointer (0), samples);
        const float f = lockDetector.estimate();
        float targetRatio = 1.0f;
        if (f > 0.0f)
        {
            const float midi    = 69.0f + 12.0f * std::log2 (f / 440.0f);
            const float snapped = snapMidi (midi, params.pitchLockKey, params.pitchLockScale, params.pitchLockMode);
            const float semis   = (snapped - midi) * juce::jlimit (0.0f, 1.0f, params.pitchLockAmount);
            targetRatio = std::pow (2.0f, semis / 12.0f);
        }
        smoothedLockRatio.setTargetValue (targetRatio);
        const float r = smoothedLockRatio.skip (samples);
        if (params.pitchLockFormant)
        {
            // Phase-vocoder path: preserves formants, always running (adds latency).
            formantShifterL.setFormant (true);
            formantShifterR.setFormant (true);
            formantShifterL.setRatio (r);
            formantShifterR.setRatio (r);
            formantShifterL.process (output.getWritePointer (0), samples);
            if (channels > 1) formantShifterR.process (output.getWritePointer (1), samples);
        }
        else
        {
            const bool shifting = std::abs (r - 1.0f) > 0.001f;
            lockShifterL.setRatio (r);
            lockShifterR.setRatio (r);
            if (shifting)
            {
                lockShifterL.process (output.getWritePointer (0), samples);
                if (channels > 1) lockShifterR.process (output.getWritePointer (1), samples);
            }
            else
            {
                lockShifterL.prime (output.getReadPointer (0), samples);
                if (channels > 1) lockShifterR.prime (output.getReadPointer (1), samples);
            }
        }
    }

    if (params.mixEqOn)
    {
        updateEq (params);
        juce::dsp::AudioBlock<float> block (output);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        eqLowShelf.process (ctx);
        eqMidPeak.process (ctx);
        eqHighShelf.process (ctx);
        if (params.eqLoFi > 0.001f)
            eqLoFiLP.process (ctx);
    }

    // Stereo width via mid/side: scale the side signal (0 = mono, 2 = wide).
    if (channels > 1 && std::abs (params.width - 1.0f) > 0.001f)
    {
        const float w = juce::jlimit (0.0f, 2.0f, params.width);
        float* l = output.getWritePointer (0);
        float* r = output.getWritePointer (1);
        for (int i = 0; i < samples; ++i)
        {
            const float mid  = 0.5f * (l[i] + r[i]);
            const float side = 0.5f * (l[i] - r[i]) * w;
            l[i] = mid + side;
            r[i] = mid - side;
        }
    }

    // Limiter + glue are deferred to processMasterDynamics so they also catch
    // Sample Mode playback, which the processor sums in after this returns.
}

void MixEngine::processMasterDynamics (juce::AudioBuffer<float>& output,
                                       float glueAmount, float ceilingDb, bool limiterOn)
{
    juce::dsp::AudioBlock<float> block (output);
    juce::dsp::ProcessContextReplacing<float> ctx (block);

    // One-knob glue: more amount = lower threshold, higher ratio, more makeup.
    const float amt = juce::jlimit (0.0f, 1.0f, glueAmount);
    if (amt > 0.001f)
    {
        glueComp.setThreshold (juce::jmap (amt, 0.0f, 1.0f, -4.0f, -22.0f));
        glueComp.setRatio     (juce::jmap (amt, 0.0f, 1.0f, 1.5f, 4.0f));
        glueComp.process (ctx);

        const float makeup = juce::Decibels::decibelsToGain (juce::jmap (amt, 0.0f, 1.0f, 0.0f, 6.0f));
        smoothedGlueMakeup.setTargetValue (makeup);
        const int n = output.getNumSamples();
        for (int i = 0; i < n; ++i)
        {
            const float g = smoothedGlueMakeup.getNextValue();
            for (int c = 0; c < output.getNumChannels(); ++c)
                output.setSample (c, i, output.getSample (c, i) * g);
        }
    }
    else
    {
        smoothedGlueMakeup.setCurrentAndTargetValue (1.0f);
    }

    if (limiterOn)
    {
        limiter.setThreshold (ceilingDb);
        limiter.process (ctx);
    }
}

void MixEngine::panic()
{
    if (tmp.getNumSamples() > 0)
        tmp.clear();
}

} // namespace mix
} // namespace gf
