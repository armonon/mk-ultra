#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "GrainFreeze/Saturator.h"

namespace gf
{

// ---- AIR: a parallel high-band "toy box" ------------------------------------
//
// Splits the master bus at a crossover, leaves the low band pristine, and runs
// the HIGH band through a chain of tonal-layering tools, then blends the
// processed top back against the dry top:
//
//     high -> [Squelch] -> [Exciter] -> [Dyn Shelf] -> [Phaser] -> [Delay]
//
//  Squelch    resonant TPT state-variable filter whose cutoff can ride the
//             band's own envelope (auto-wah). High resonance adds a pitched
//             ring at the cutoff -- the most direct way to "layer tonality".
//  Exciter    the existing 4x-oversampled Saturator on the high band only:
//             generates harmonics above the crossover (aural-exciter style).
//  Dyn Shelf  a high shelf whose gain follows the band envelope. Negative =
//             tames spiky highs (de-ess), positive = lifts air when there's
//             energy to lift.
//  Phaser     juce::dsp::Phaser confined to the top: moving notches without
//             smearing the low end.
//  Delay      short feedback delay on the top only: tuned echoes / comb tone.
//
// The Saturator's oversampler delays the wet chain by a few samples; the low
// band and the dry-top copy are delayed by the same integer amount so the
// recombination stays phase-coherent through the crossover. Allocation-free in
// process().
class AirEngine
{
public:
    struct Params
    {
        bool  on = false;
        float crossoverHz = 2500.0f;
        float mix = 0.5f;               // processed-top vs dry-top (low is always dry)

        bool  squelchOn   = false;
        int   squelchMode = 1;          // 0 LP, 1 BP, 2 HP
        float squelchHz   = 4000.0f;
        float squelchRes  = 0.5f;       // 0..1 -> Q 0.5..14 (self-oscillates near the top)
        float squelchEnv  = 0.0f;       // -1..+1: envelope -> cutoff, in octaves x4

        bool  exciterOn    = false;
        float exciterDrive = 0.3f;      // 0..1 -> 1..12x
        float exciterMix   = 0.5f;

        bool  dynShelfOn        = false;
        float dynShelfHz        = 8000.0f;
        float dynShelfAmount    = 0.0f; // -1 (tame) .. +1 (lift), up to +/-12 dB
        float dynShelfThreshold = 0.2f;

        bool  phaserOn    = false;
        float phaserRate  = 0.3f;       // Hz
        float phaserDepth = 0.6f;
        float phaserMix   = 0.5f;

        bool  delayOn       = false;
        float delayMs       = 120.0f;
        float delayFeedback = 0.35f;
        float delayMix      = 0.4f;
    };

    void prepare (double sampleRate, int numChannels, int maxBlock)
    {
        sr = sampleRate > 0.0 ? sampleRate : 44100.0;
        channels = juce::jlimit (1, 2, juce::jmax (1, numChannels));
        const int n = juce::jmax (1, maxBlock);

        juce::dsp::ProcessSpec spec { sr, (juce::uint32) n, (juce::uint32) channels };

        lowBuf.setSize (channels, n);
        highBuf.setSize (channels, n);
        highDry.setSize (channels, n);

        splitter.prepare (spec);
        splitter.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
        splitter.setCutoffFrequency (2500.0f);

        svf.prepare (spec);
        svf.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
        svf.setCutoffFrequency (4000.0f);
        svf.setResonance (1.0f);

        bandEnv.prepare (spec);
        bandEnv.setLevelCalculationType (juce::dsp::BallisticsFilterLevelCalculationType::peak);
        bandEnv.setAttackTime (5.0f);
        bandEnv.setReleaseTime (80.0f);

        exciter.prepare (sr, channels, n);
        latency = exciter.getLatencySamples();

        // Integer rings that delay everything NOT going through the exciter's
        // oversampler by the same amount, so low + dryTop + wetTop line up.
        ringLen = juce::jmax (1, latency);
        lowRing.setSize (channels, ringLen);  lowRing.clear();
        dryRing.setSize (channels, ringLen);  dryRing.clear();
        ringPos = 0;

        for (int c = 0; c < 2; ++c)
        {
            shelf[c].reset();
            shelf[c].coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, 8000.0, 0.707f, 1.0f);
        }
        shelfGainDb.reset (sr, 0.02);
        shelfGainDb.setCurrentAndTargetValue (0.0f);
        lastShelfHz = 8000.0f; lastShelfDb = 0.0f;

        phaser.prepare (spec);
        phaser.setCentreFrequency (3000.0f);
        phaser.setFeedback (0.35f);

        delay.prepare (spec);
        delay.setMaximumDelayInSamples ((int) (sr * 1.5) + 1);
        delay.reset();
        delaySamples.reset (sr, 0.05);
        delaySamples.setCurrentAndTargetValue (0.12f * (float) sr);
    }

    void reset()
    {
        splitter.reset(); svf.reset(); bandEnv.reset(); phaser.reset();
        for (auto& s : shelf) s.reset();
        delay.reset();
        lowRing.clear(); dryRing.clear(); ringPos = 0;
    }

    int getLatencySamples() const { return latency; }

    void process (juce::AudioBuffer<float>& buffer, const Params& p)
    {
        if (! p.on) return;
        const int n  = buffer.getNumSamples();
        const int ch = juce::jmin (buffer.getNumChannels(), channels);
        if (n <= 0 || ch <= 0 || n > lowBuf.getNumSamples()) return;

        // ---- 1. Split. The LR pair writes low + high in one call.
        splitter.setCutoffFrequency (juce::jlimit (200.0f, 12000.0f, p.crossoverHz));
        for (int c = 0; c < ch; ++c)
        {
            const auto* src = buffer.getReadPointer (c);
            auto* lo = lowBuf.getWritePointer (c);
            auto* hi = highBuf.getWritePointer (c);
            for (int i = 0; i < n; ++i)
                splitter.processSample (c, src[i], lo[i], hi[i]);
        }
        for (int c = 0; c < ch; ++c)
            highDry.copyFrom (c, 0, highBuf, c, 0, n);

        // Block-level envelope of the top band, shared by squelch + shelf.
        float env = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            float mx = 0.0f;
            for (int c = 0; c < ch; ++c) mx = juce::jmax (mx, std::abs (highBuf.getSample (c, i)));
            env = bandEnv.processSample (0, mx);
        }
        env = juce::jlimit (0.0f, 1.0f, env);

        // ---- 2. Squelch: resonant SVF, cutoff optionally riding the envelope.
        if (p.squelchOn)
        {
            using T = juce::dsp::StateVariableTPTFilterType;
            svf.setType (p.squelchMode == 0 ? T::lowpass : p.squelchMode == 2 ? T::highpass : T::bandpass);
            const float octaves = p.squelchEnv * 4.0f * env;          // +/- 4 oct at full env
            const float hz = juce::jlimit (40.0f, (float) (sr * 0.45), p.squelchHz * std::pow (2.0f, octaves));
            svf.setCutoffFrequency (hz);
            svf.setResonance (0.5f + juce::jlimit (0.0f, 1.0f, p.squelchRes) * 13.5f);
            for (int c = 0; c < ch; ++c)
            {
                auto* d = highBuf.getWritePointer (c);
                for (int i = 0; i < n; ++i)
                    d[i] = svf.processSample (c, d[i]);
            }
            // Resonance can push well past unity; keep it civil.
            highBuf.applyGain (1.0f / (1.0f + juce::jlimit (0.0f, 1.0f, p.squelchRes) * 1.5f));
        }

        // ---- 3. Exciter: oversampled saturator on the top only.
        // Always run so the oversampler latency is constant; mix=0 when off.
        exciter.setType (0);                                  // tube-ish
        exciter.setDrive (1.0f + juce::jlimit (0.0f, 1.0f, p.exciterDrive) * 11.0f);
        exciter.setMix   (p.exciterOn ? juce::jlimit (0.0f, 1.0f, p.exciterMix) : 0.0f);
        exciter.process (highBuf);

        // ---- 4. Dynamic shelf: gain follows the envelope above a threshold.
        if (p.dynShelfOn)
        {
            const float thr  = juce::jlimit (0.0f, 0.95f, p.dynShelfThreshold);
            const float over = juce::jmax (0.0f, env - thr) / (1.0f - thr);
            shelfGainDb.setTargetValue (juce::jlimit (-1.0f, 1.0f, p.dynShelfAmount) * 12.0f * over);
        }
        else
            shelfGainDb.setTargetValue (0.0f);
        {
            const float db = shelfGainDb.skip (n);
            const float hz = juce::jlimit (1000.0f, (float) (sr * 0.45), p.dynShelfHz);
            if (std::abs (db - lastShelfDb) > 0.05f || std::abs (hz - lastShelfHz) > 1.0f)
            {
                auto coeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf (
                                  sr, hz, 0.707f, juce::Decibels::decibelsToGain (db));
                for (int c = 0; c < ch; ++c) shelf[c].coefficients = coeffs;
                lastShelfDb = db; lastShelfHz = hz;
            }
            if (std::abs (lastShelfDb) > 0.01f)
                for (int c = 0; c < ch; ++c)
                {
                    auto* d = highBuf.getWritePointer (c);
                    for (int i = 0; i < n; ++i) d[i] = shelf[c].processSample (d[i]);
                }
        }

        // ---- 5. Phaser on the top.
        if (p.phaserOn && p.phaserMix > 0.001f)
        {
            phaser.setRate  (juce::jlimit (0.02f, 10.0f, p.phaserRate));
            phaser.setDepth (juce::jlimit (0.0f, 1.0f, p.phaserDepth));
            phaser.setMix   (juce::jlimit (0.0f, 1.0f, p.phaserMix));
            juce::dsp::AudioBlock<float> blk (highBuf.getArrayOfWritePointers(), (size_t) ch, (size_t) n);
            juce::dsp::ProcessContextReplacing<float> ctx (blk);
            phaser.process (ctx);
        }

        // ---- 6. Feedback delay on the top.
        if (p.delayOn && p.delayMix > 0.001f)
        {
            delaySamples.setTargetValue (juce::jlimit (1.0f, (float) (sr * 1.4), p.delayMs * 0.001f * (float) sr));
            const float fb = juce::jlimit (0.0f, 0.95f, p.delayFeedback);
            const float mx = juce::jlimit (0.0f, 1.0f, p.delayMix);
            for (int i = 0; i < n; ++i)
            {
                delay.setDelay (delaySamples.getNextValue());
                for (int c = 0; c < ch; ++c)
                {
                    auto* d = highBuf.getWritePointer (c);
                    const float y = delay.popSample (c);
                    delay.pushSample (c, d[i] + y * fb);
                    d[i] = d[i] * (1.0f - mx) + y * mx;
                }
            }
        }

        // ---- 7. Recombine: delayed low + blend(delayed dryTop, wetTop).
        const float mix = juce::jlimit (0.0f, 1.0f, p.mix);
        int rp = ringPos;
        for (int i = 0; i < n; ++i)
        {
            for (int c = 0; c < ch; ++c)
            {
                auto* lr = lowRing.getWritePointer (c);
                auto* dr = dryRing.getWritePointer (c);
                const float loDelayed  = lr[rp];
                const float dryDelayed = dr[rp];
                lr[rp] = lowBuf.getSample (c, i);
                dr[rp] = highDry.getSample (c, i);
                const float wet = highBuf.getSample (c, i);
                buffer.setSample (c, i, loDelayed + dryDelayed * (1.0f - mix) + wet * mix);
            }
            if (++rp >= ringLen) rp = 0;
        }
        ringPos = rp;
    }

private:
    double sr = 44100.0;
    int channels = 2, latency = 0, ringLen = 1, ringPos = 0;

    juce::dsp::LinkwitzRileyFilter<float> splitter;
    juce::AudioBuffer<float> lowBuf, highBuf, highDry, lowRing, dryRing;

    juce::dsp::StateVariableTPTFilter<float> svf;
    juce::dsp::BallisticsFilter<float> bandEnv;
    Saturator exciter;

    juce::dsp::IIR::Filter<float> shelf[2];
    juce::SmoothedValue<float> shelfGainDb;
    float lastShelfHz = 8000.0f, lastShelfDb = 0.0f;

    juce::dsp::Phaser<float> phaser;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delay;
    juce::SmoothedValue<float> delaySamples;
};

} // namespace gf
