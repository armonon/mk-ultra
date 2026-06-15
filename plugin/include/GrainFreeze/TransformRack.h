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

// ---- Damage: drive -> sample-rate reduction -> bit crush, in one in-place pass.
// amount 0..1 scales severity; mix 0..1 wet/dry. No external deps so it can run
// per-sample without scratch buffers.
class DamageEngine
{
public:
    void prepare (double sr, int numChannels)
    {
        juce::ignoreUnused (sr);
        hold.assign ((size_t) juce::jmax (1, numChannels), 0.0f);
        counter.assign ((size_t) juce::jmax (1, numChannels), 0);
        reset();
    }

    void reset()
    {
        std::fill (hold.begin(), hold.end(), 0.0f);
        std::fill (counter.begin(), counter.end(), 0);
    }

    void process (juce::AudioBuffer<float>& buffer, float amount, float mix)
    {
        if (mix <= 0.001f || amount <= 0.001f)
            return;

        const float a       = juce::jlimit (0.0f, 1.0f, amount);
        const float m        = juce::jlimit (0.0f, 1.0f, mix);
        const float drive    = 1.0f + a * 18.0f;                 // 1..19x
        const float comp     = 1.0f / std::sqrt (drive);          // level compensation
        const int   downN    = 1 + (int) (a * a * 40.0f);         // sample-and-hold stride
        const float bits     = 16.0f - a * 14.0f;                 // 16..2 bits
        const float levels   = std::pow (2.0f, juce::jmax (1.0f, bits));
        const float step     = 2.0f / levels;

        const int numCh = buffer.getNumChannels();
        const int n     = buffer.getNumSamples();
        for (int c = 0; c < numCh && c < (int) hold.size(); ++c)
        {
            auto* d = buffer.getWritePointer (c);
            float  h  = hold[(size_t) c];
            int    ct = counter[(size_t) c];
            for (int i = 0; i < n; ++i)
            {
                const float x = d[i];
                if (ct <= 0) { h = x; ct = downN; }   // sample-and-hold downsample
                --ct;
                float wet = std::tanh (h * drive) * comp;          // saturate
                wet = std::round (wet / step) * step;              // bit crush
                d[i] = x * (1.0f - m) + wet * m;
            }
            hold[(size_t) c]    = h;
            counter[(size_t) c] = ct;
        }
    }

private:
    std::vector<float> hold;
    std::vector<int>   counter;
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

    // sliceMs: length of a captured slice; rateHz: how often the clock fires;
    // chance 0..1: probability a clock tick triggers a repeat; revChance 0..1:
    // probability a triggered slice plays reversed; mix 0..1 wet/dry.
    void process (juce::AudioBuffer<float>& buffer, float sliceMs, float rateHz,
                  float chance, float revChance, float mix)
    {
        if (mix <= 0.001f || chance <= 0.001f)
            return;

        const float m       = juce::jlimit (0.0f, 1.0f, mix);
        const int   clockN  = juce::jmax (1, (int) (sr / juce::jmax (0.25f, rateHz)));
        const int   wantLen = juce::jlimit (1, maxSliceLen, (int) (sr * sliceMs * 0.001f));
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
