#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>
#include "GrainFreeze/FormantShifter.h"

// Dedicated "machine" DSP for the MK-ULTRA product direction. Each machine is an
// independent insert on the master bus, gated by its own On flag and blended by
// its own Mix. All process() methods are allocation-free (scratch sized in
// prepare) and bypass cleanly when mix/amount are ~0.
namespace gf
{

// ---- Damage: a full destruction stage, in one allocation-free in-place pass:
// drive + waveshaper -> sample-rate reduction (with jitter) -> bit crush (+dither)
// -> noise -> dropouts -> tone lowpass -> wet/dry. Each stage has its own control.
struct DamageParams
{
    float amount  = 0.0f;   // master drive (0..1 -> 1..19x into the shaper)
    int   clip    = 0;      // 0 Tube, 1 Tape, 2 Hard, 3 Fold, 4 Diode
    float bits    = 16.0f;  // 1..16
    float rate    = 1.0f;   // downsample factor (1 = none .. 64)
    float jitter  = 0.0f;   // 0..1 digital instability (SR wobble + dither)
    float noise   = 0.0f;   // 0..1 added hiss
    float dropout = 0.0f;   // 0..1 random sample dropouts
    float tone    = 1.0f;   // 0 dark .. 1 open (post lowpass)
    float mix     = 0.0f;   // 0..1 wet/dry
};

class DamageEngine
{
public:
    void prepare (double sampleRate, int numChannels)
    {
        sr = sampleRate > 0.0 ? sampleRate : 44100.0;
        const int ch = juce::jmax (1, numChannels);
        hold.assign ((size_t) ch, 0.0f);
        counter.assign ((size_t) ch, 0);
        lp.assign ((size_t) ch, 0.0f);
        dropLeft.assign ((size_t) ch, 0);
        reset();
    }

    void reset()
    {
        std::fill (hold.begin(), hold.end(), 0.0f);
        std::fill (counter.begin(), counter.end(), 0);
        std::fill (lp.begin(), lp.end(), 0.0f);
        std::fill (dropLeft.begin(), dropLeft.end(), 0);
    }

    void process (juce::AudioBuffer<float>& buffer, const DamageParams& p)
    {
        if (p.mix <= 0.001f)
            return;

        const float m      = juce::jlimit (0.0f, 1.0f, p.mix);
        const float drive  = 1.0f + juce::jlimit (0.0f, 1.0f, p.amount) * 18.0f;
        const float comp   = 1.0f / std::sqrt (drive);
        const float levels = std::pow (2.0f, juce::jlimit (1.0f, 16.0f, p.bits));
        const float step   = 2.0f / levels;
        const int   baseN  = juce::jmax (1, (int) std::lround (p.rate));
        const float fc     = 300.0f * std::pow (60.0f, juce::jlimit (0.0f, 1.0f, p.tone)); // 300..18000 Hz
        const float lpCoef = juce::jlimit (0.0f, 1.0f, 1.0f - std::exp (-2.0f * pi * fc / (float) sr));

        const int numCh = buffer.getNumChannels();
        const int n     = buffer.getNumSamples();
        for (int c = 0; c < numCh && c < (int) hold.size(); ++c)
        {
            auto* d  = buffer.getWritePointer (c);
            float h  = hold[(size_t) c];
            int   ct = counter[(size_t) c];
            float y  = lp[(size_t) c];
            int   dl = dropLeft[(size_t) c];
            for (int i = 0; i < n; ++i)
            {
                const float x = d[i];
                float w = shape (p.clip, x * drive) * comp;        // drive + waveshape

                if (ct <= 0)                                        // sample-rate reduction
                {
                    h = w;
                    ct = baseN;
                    if (p.jitter > 0.0f)
                        ct = juce::jmax (1, (int) std::lround (baseN * (1.0f + (rng.nextFloat() * 2.0f - 1.0f) * p.jitter)));
                }
                --ct;
                w = h;

                if (p.jitter > 0.0f)                               // dither
                    w += (rng.nextFloat() * 2.0f - 1.0f) * step * p.jitter * 0.5f;
                w = std::round (w / step) * step;                  // bit crush

                if (p.noise > 0.0f)                                // hiss
                    w += (rng.nextFloat() * 2.0f - 1.0f) * p.noise * 0.2f;

                if (dl > 0) { w = 0.0f; --dl; }                    // dropouts
                else if (p.dropout > 0.0f && rng.nextFloat() < p.dropout * 0.01f)
                {
                    dl = 1 + rng.nextInt (juce::jmax (1, (int) (p.dropout * 220.0f)));
                    w = 0.0f;
                }

                y += lpCoef * (w - y);                             // tone lowpass
                w = y;

                d[i] = x * (1.0f - m) + w * m;
            }
            hold[(size_t) c] = h; counter[(size_t) c] = ct;
            lp[(size_t) c] = y;   dropLeft[(size_t) c] = dl;
        }
    }

private:
    static inline float shape (int clip, float x)
    {
        switch (clip)
        {
            case 1:  return std::tanh (x * 0.8f);                                  // Tape
            case 2:  return juce::jlimit (-1.0f, 1.0f, x);                          // Hard
            case 3:  return std::sin (juce::jlimit (-3.5f, 3.5f, x) * 1.5f);        // Fold
            case 4:  return x >= 0.0f ? std::tanh (x) : std::tanh (x * 0.3f);       // Diode (asym)
            case 0:
            default: return x >= 0.0f ? 1.0f - std::exp (-x) : -1.0f + std::exp (x * 0.7f); // Tube
        }
    }

    static constexpr float pi = juce::MathConstants<float>::pi;
    double sr = 44100.0;
    std::vector<float> hold, lp;
    std::vector<int>   counter, dropLeft;
    juce::Random rng;
};

// ---- Time Breaker: beat-repeat / stutter / reverse glitch. Captures incoming
// audio into a ring; on a periodic clock (sliceLen samples) it may latch the
// most recent slice and replay it (optionally reversed) for a few repeats,
// blended by mix. Allocation-free in process.
class TimeBreakerEngine
{
public:
    void prepare (double sampleRate, int numChannels)
    {
        sr = sampleRate > 0.0 ? sampleRate : 44100.0;
        channels = juce::jmax (1, numChannels);
        const int maxSlice = (int) (sr * 1.2);                    // up to 1.2 s slices
        slice.assign ((size_t) channels, std::vector<float> ((size_t) maxSlice, 0.0f));
        maxSliceLen = maxSlice;
        reset();
    }

    void reset()
    {
        active = false; clock = 0; playPos = 0; repeatsLeft = 0; sliceLen = 0; reversed = false;
        writeHead = 0; captureStart = 0;
        for (auto& s : slice) std::fill (s.begin(), s.end(), 0.0f);
    }

    // sliceSamples: length of a captured slice; clockSamples: how often the clock
    // fires (both already resolved from free ms/Hz or tempo-synced divisions by the
    // caller); chance 0..1: probability a clock tick triggers a repeat; revChance
    // 0..1: probability a triggered slice plays reversed; mix 0..1 wet/dry.
    void process (juce::AudioBuffer<float>& buffer, int sliceSamples, int clockSamples,
                  float chance, float revChance, float mix)
    {
        if (mix <= 0.001f || chance <= 0.001f)
            return;

        const float m       = juce::jlimit (0.0f, 1.0f, mix);
        const int   clockN  = juce::jmax (1, clockSamples);
        const int   wantLen = juce::jlimit (1, maxSliceLen, sliceSamples);
        const int   numCh   = juce::jmin (channels, buffer.getNumChannels());
        const int   n       = buffer.getNumSamples();

        for (int i = 0; i < n; ++i)
        {
            if (! active && --clock <= 0)
            {
                clock = clockN;
                if (rng.nextFloat() < chance)                      // latch a new slice
                {
                    sliceLen     = wantLen;
                    repeatsLeft  = 1 + rng.nextInt (4);
                    reversed     = rng.nextFloat() < revChance;
                    playPos      = 0;
                    captureStart = writeHead - sliceLen;           // most-recent window
                    active       = true;
                }
            }

            for (int c = 0; c < numCh; ++c)
            {
                auto* d  = buffer.getWritePointer (c);
                auto& sl = slice[(size_t) c];
                const float in = d[i];

                if (! active)
                {
                    sl[(size_t) wrap (writeHead)] = in;            // keep filling the ring
                }
                else
                {
                    const long ofs  = reversed ? (sliceLen - 1 - playPos) : playPos;
                    const float wet = sl[(size_t) wrap (captureStart + ofs)];
                    d[i] = in * (1.0f - m) + wet * m;
                }
            }

            if (! active)
                ++writeHead;
            else if (++playPos >= sliceLen)
            {
                playPos = 0;
                if (--repeatsLeft <= 0) { active = false; clock = clockN; }
            }
        }
    }

    int wrap (long pos) const { long r = pos % maxSliceLen; if (r < 0) r += maxSliceLen; return (int) r; }

private:
    double sr = 44100.0;
    int    channels = 2;
    int    maxSliceLen = 1;
    std::vector<std::vector<float>> slice;

    bool active = false, reversed = false;
    int  clock = 0, playPos = 0, repeatsLeft = 0, sliceLen = 0;
    long writeHead = 0, captureStart = 0;
    juce::Random rng;
};

// ---- Pitch / Formant: stereo formant-preserving pitch shifter. The shifter's
// output is delayed by kLatency, so the dry is delayed to match before the
// wet/dry blend (no comb filtering). Up to 2 channels.
class PitchFormantMachine
{
public:
    void prepare (double sampleRate, int numChannels, int maxBlock)
    {
        const int ch = juce::jlimit (1, 2, numChannels);
        for (int c = 0; c < 2; ++c)
        {
            shifter[(size_t) c].prepare (sampleRate);
            dryRing[(size_t) c].assign ((size_t) FormantShifter::kLatency, 0.0f);
        }
        scratch.assign ((size_t) juce::jmax (1, maxBlock), 0.0f);
        ringIdx = 0;
        juce::ignoreUnused (ch);
        reset();
    }

    void reset()
    {
        for (int c = 0; c < 2; ++c)
        {
            shifter[(size_t) c].reset();
            std::fill (dryRing[(size_t) c].begin(), dryRing[(size_t) c].end(), 0.0f);
        }
        ringIdx = 0;
    }

    void process (juce::AudioBuffer<float>& buffer, float semitones, bool formant, float mix)
    {
        const float m = juce::jlimit (0.0f, 1.0f, mix);
        if (m <= 0.001f)
            return;

        const float ratio = juce::jlimit (0.25f, 4.0f, std::pow (2.0f, semitones / 12.0f));
        const int   numCh = juce::jmin (2, buffer.getNumChannels());
        const int   n     = buffer.getNumSamples();
        const int   L     = FormantShifter::kLatency;

        for (int c = 0; c < numCh; ++c)
        {
            auto* d = buffer.getWritePointer (c);

            // latency-compensate the dry: read it out of a kLatency ring while
            // pushing the new input in (per-channel, independent index copy).
            int ri = ringIdx;
            for (int i = 0; i < n; ++i)
            {
                const float in = d[i];
                scratch[(size_t) i] = dryRing[(size_t) c][(size_t) ri]; // delayed dry
                dryRing[(size_t) c][(size_t) ri] = in;
                if (++ri >= L) ri = 0;
            }

            shifter[(size_t) c].setRatio (ratio);
            shifter[(size_t) c].setFormant (formant);
            shifter[(size_t) c].process (d, n);                    // d becomes wet (delayed L)

            for (int i = 0; i < n; ++i)
                d[i] = scratch[(size_t) i] * (1.0f - m) + d[i] * m;
        }

        // advance the shared ring index once for the block (all channels stepped
        // it identically above; commit the final position)
        ringIdx = (ringIdx + n) % L;
    }

private:
    std::array<FormantShifter, 2> shifter;
    std::array<std::vector<float>, 2> dryRing;
    std::vector<float> scratch;
    int ringIdx = 0;
};

} // namespace gf
