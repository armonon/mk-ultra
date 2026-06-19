#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <memory>
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
    void prepare (double sampleRate, int numChannels, int maxBlock)
    {
        sr = sampleRate > 0.0 ? sampleRate : 44100.0;
        const int ch = juce::jmax (1, numChannels);
        hold.assign ((size_t) ch, 0.0f);
        counter.assign ((size_t) ch, 0);
        lp.assign ((size_t) ch, 0.0f);
        dropLeft.assign ((size_t) ch, 0);

        // The waveshaper (Tube/Tape/Hard/Fold/Diode) throws harmonics above
        // Nyquist at high drive; run it 4x oversampled so they don't fold back
        // as aliasing. The lo-fi stages downstream stay at base rate on purpose.
        osChannels = juce::jlimit (1, 2, ch);
        const int block = juce::jmax (1, maxBlock);
        os = std::make_unique<juce::dsp::Oversampling<float>> (
                 (size_t) osChannels, 2,
                 juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
        os->initProcessing ((size_t) block);
        os->reset();
        dryBuf.setSize (osChannels, block);

        // Delay the dry path by the oversampler latency so the wet/dry blend
        // stays phase-aligned (otherwise it combs at low amounts).
        dryRingLen = juce::jmax (1, (int) std::lround (os->getLatencyInSamples()));
        dryRing.setSize (osChannels, dryRingLen);
        dryRing.clear();
        dryRingPos = 0;
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

        const int n     = buffer.getNumSamples();
        const int numCh = juce::jmin (buffer.getNumChannels(), juce::jmin ((int) hold.size(), osChannels));
        if (numCh <= 0 || n <= 0)
            return;

        // Stash the dry signal for the wet/dry blend at the end.
        for (int c = 0; c < numCh; ++c)
            dryBuf.copyFrom (c, 0, buffer, c, 0, n);

        // ---- Stage 1: drive + waveshaper, 4x oversampled (anti-aliased) ----
        if (os != nullptr && numCh == osChannels && n <= dryBuf.getNumSamples())
        {
            juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numCh, (size_t) n);
            auto up = os->processSamplesUp (block);
            const int upN = (int) up.getNumSamples();
            for (int c = 0; c < numCh; ++c)
            {
                auto* u = up.getChannelPointer ((size_t) c);
                for (int i = 0; i < upN; ++i)
                    u[i] = shape (p.clip, u[i] * drive) * comp;
            }
            os->processSamplesDown (block);
        }
        else // fallback (shouldn't happen once prepared): shape at base rate
        {
            for (int c = 0; c < numCh; ++c)
            {
                auto* d = buffer.getWritePointer (c);
                for (int i = 0; i < n; ++i)
                    d[i] = shape (p.clip, d[i] * drive) * comp;
            }
        }

        // ---- Stage 2: lo-fi grit stays at base rate (the aliasing IS the sound) ----
        for (int c = 0; c < numCh; ++c)
        {
            auto* d  = buffer.getWritePointer (c);
            float h  = hold[(size_t) c];
            int   ct = counter[(size_t) c];
            float y  = lp[(size_t) c];
            int   dl = dropLeft[(size_t) c];
            for (int i = 0; i < n; ++i)
            {
                float w = d[i];                                   // anti-aliased waveshaper output

                if (ct <= 0)                                       // sample-rate reduction
                {
                    h = w;
                    ct = baseN;
                    if (p.jitter > 0.0f)
                        ct = juce::jmax (1, (int) std::lround (baseN * (1.0f + (rng.nextFloat() * 2.0f - 1.0f) * p.jitter)));
                }
                --ct;
                w = h;

                if (p.jitter > 0.0f)                              // dither
                    w += (rng.nextFloat() * 2.0f - 1.0f) * step * p.jitter * 0.5f;
                w = std::round (w / step) * step;                 // bit crush

                if (p.noise > 0.0f)                               // hiss
                    w += (rng.nextFloat() * 2.0f - 1.0f) * p.noise * 0.2f;

                if (dl > 0) { w = 0.0f; --dl; }                   // dropouts
                else if (p.dropout > 0.0f && rng.nextFloat() < p.dropout * 0.01f)
                {
                    dl = 1 + rng.nextInt (juce::jmax (1, (int) (p.dropout * 220.0f)));
                    w = 0.0f;
                }

                y += lpCoef * (w - y);                            // tone lowpass
                d[i] = y;                                         // pre-blend wet
            }
            hold[(size_t) c] = h; counter[(size_t) c] = ct;
            lp[(size_t) c] = y;   dropLeft[(size_t) c] = dl;
        }

        // ---- Stage 3: wet/dry blend, dry delayed to match oversampler latency ----
        int rp = dryRingPos;
        for (int i = 0; i < n; ++i)
        {
            for (int c = 0; c < numCh; ++c)
            {
                const float dd = dryRing.getSample (c, rp);
                dryRing.setSample (c, rp, dryBuf.getSample (c, i));
                auto* d = buffer.getWritePointer (c);
                d[i] = dd * (1.0f - m) + d[i] * m;
            }
            if (++rp >= dryRingLen) rp = 0;
        }
        dryRingPos = rp;
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

    std::unique_ptr<juce::dsp::Oversampling<float>> os; // 4x for the waveshaper only
    juce::AudioBuffer<float> dryBuf;                    // dry copy for the blend
    juce::AudioBuffer<float> dryRing;                   // latency-comp delay line
    int osChannels = 0, dryRingLen = 1, dryRingPos = 0;
};

// ---- Multiband Damage: 2-band Linkwitz-Riley split with an independent
// DamageEngine per band. The 4th-order LR pair sums to flat magnitude when both
// halves are processed unity, so when one band is bypassed the other still
// recombines correctly. This is the architectural lift that makes "destroy only
// the highs, keep the low end clean" a one-knob move.
class MultibandDamage
{
public:
    void prepare (double sampleRate, int numChannels, int maxBlock)
    {
        const int ch = juce::jlimit (1, 2, juce::jmax (1, numChannels));
        const int n  = juce::jmax (1, maxBlock);
        lowBuf.setSize (ch, n);
        highBuf.setSize (ch, n);
        juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) n, (juce::uint32) ch };
        splitter.prepare (spec);
        splitter.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
        setCrossover (800.0f);
        lowBand .prepare (sampleRate, ch, n);
        highBand.prepare (sampleRate, ch, n);
    }

    void reset()
    {
        splitter.reset();
        lowBand.reset(); highBand.reset();
    }

    void setCrossover (float hz)
    {
        splitter.setCutoffFrequency (juce::jlimit (60.0f, 12000.0f, hz));
    }

    // Process with two independent DamageParams. The whole stage bypasses when
    // both mixes are inaudible -- the LR pair preserves the signal, so this is
    // a true bypass (we just return the input untouched).
    void process (juce::AudioBuffer<float>& buffer,
                  const DamageParams& low, const DamageParams& high)
    {
        if (low.mix <= 0.001f && high.mix <= 0.001f)
            return;

        const int n  = buffer.getNumSamples();
        const int ch = juce::jmin (buffer.getNumChannels(), lowBuf.getNumChannels());
        if (n <= 0 || ch <= 0 || n > lowBuf.getNumSamples())
            return;

        // Split: the LinkwitzRileyFilter writes both low + high in one sample call.
        for (int c = 0; c < ch; ++c)
        {
            const auto* src = buffer.getReadPointer (c);
            auto* lo = lowBuf.getWritePointer (c);
            auto* hi = highBuf.getWritePointer (c);
            for (int i = 0; i < n; ++i)
                splitter.processSample (c, src[i], lo[i], hi[i]);
        }

        // Damage each band independently.
        lowBand .process (lowBuf,  low);
        highBand.process (highBuf, high);

        // Sum back. LR4 is magnitude-flat when summed, so reconstruction is exact
        // when both bands are unity.
        for (int c = 0; c < ch; ++c)
        {
            auto* dst = buffer.getWritePointer (c);
            const auto* lo = lowBuf.getReadPointer (c);
            const auto* hi = highBuf.getReadPointer (c);
            for (int i = 0; i < n; ++i)
                dst[i] = lo[i] + hi[i];
        }
    }

private:
    juce::dsp::LinkwitzRileyFilter<float> splitter;
    juce::AudioBuffer<float> lowBuf, highBuf;
    DamageEngine lowBand, highBand;
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
        writeHead = 0; captureStart = 0; gateSmoothed = 0.0f;
        for (auto& s : slice) std::fill (s.begin(), s.end(), 0.0f);
    }

    // sliceSamples: length of a captured slice; clockSamples: how often the clock
    // fires (both already resolved from free ms/Hz or tempo-synced divisions by the
    // caller); chance 0..1: probability a clock tick triggers a repeat; revChance
    // 0..1: probability a triggered slice plays reversed; mix 0..1 wet/dry.
    void process (juce::AudioBuffer<float>& buffer, int sliceSamples, int clockSamples,
                  float chance, float revChance, float mix)
    {
        // Always advance the clock + gate so knob routing works even when the audio
        // effect is bypassed (mix 0). Audio blend only happens when wet + chance > 0.
        const float m       = juce::jlimit (0.0f, 1.0f, mix);
        const bool  doAudio = m > 0.001f && chance > 0.001f;
        const int   clockN  = juce::jmax (1, clockSamples);
        const int   wantLen = juce::jlimit (1, maxSliceLen, sliceSamples);
        const int   numCh   = juce::jmin (channels, buffer.getNumChannels());
        const int   n       = buffer.getNumSamples();
        const float gCoef   = 1.0f - std::exp (-1.0f / juce::jmax (1.0f, (float) (sr * 0.006))); // ~6 ms

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

            if (doAudio)
                for (int c = 0; c < numCh; ++c)
                {
                    auto* d  = buffer.getWritePointer (c);
                    auto& sl = slice[(size_t) c];
                    const float in = d[i];
                    if (! active)
                        sl[(size_t) wrap (writeHead)] = in;        // keep filling the ring
                    else
                    {
                        const long ofs = reversed ? (sliceLen - 1 - playPos) : playPos;
                        d[i] = in * (1.0f - m) + sl[(size_t) wrap (captureStart + ofs)] * m;
                    }
                }

            // Smoothed gate (1 while repeating, else 0) — drives knob routing.
            gateSmoothed += gCoef * ((active ? 1.0f : 0.0f) - gateSmoothed);

            if (! active)
                ++writeHead;
            else if (++playPos >= sliceLen)
            {
                playPos = 0;
                if (--repeatsLeft <= 0) { active = false; clock = clockN; }
            }
        }
    }

    // Tempo-synced rhythmic gate (0..1), for routing Time Breaker onto knobs.
    float gate() const { return gateSmoothed; }

    int wrap (long pos) const { long r = pos % maxSliceLen; if (r < 0) r += maxSliceLen; return (int) r; }

private:
    double sr = 44100.0;
    int    channels = 2;
    int    maxSliceLen = 1;
    std::vector<std::vector<float>> slice;

    bool active = false, reversed = false;
    int  clock = 0, playPos = 0, repeatsLeft = 0, sliceLen = 0;
    long writeHead = 0, captureStart = 0;
    float gateSmoothed = 0.0f;
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
