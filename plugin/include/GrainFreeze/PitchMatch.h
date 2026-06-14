#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <cmath>

namespace gf
{

// Monophonic fundamental-frequency estimator via autocorrelation on a decimated
// mono signal. Designed to run at control rate: push() each block, then call
// estimate(). Returns 0 Hz when the input is too quiet or has no clear pitch
// (so the caller can fall back to "no correction"). Allocation only in prepare().
class PitchDetector
{
public:
    void prepare (double sampleRate)
    {
        decim   = juce::jmax (1, (int) std::round (sampleRate / 8000.0)); // ~8 kHz working rate
        decSr   = sampleRate / (double) decim;
        ring.assign ((size_t) kWin, 0.0f);
        work.assign ((size_t) kWin, 0.0f);
        writePos = 0; decCount = 0; acc = 0.0f;
    }

    void push (const float* mono, int numSamples)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            acc += mono[i];
            if (++decCount >= decim)
            {
                ring[(size_t) writePos] = acc / (float) decim;
                writePos = (writePos + 1) % kWin;
                decCount = 0;
                acc = 0.0f;
            }
        }
    }

    // Returns detected frequency in Hz, or 0 if unvoiced/too quiet.
    float estimate()
    {
        // Linearise the ring (oldest -> newest) and measure energy.
        double energy = 0.0;
        for (int i = 0; i < kWin; ++i)
        {
            const float s = ring[(size_t) ((writePos + i) % kWin)];
            work[(size_t) i] = s;
            energy += (double) s * s;
        }
        if (energy < 1.0e-4) return 0.0f; // effectively silent

        // Lag range for ~70 Hz .. 900 Hz at the decimated rate.
        const int minLag = juce::jmax (2, (int) std::floor (decSr / 900.0));
        const int maxLag = juce::jmin (kWin - 1, (int) std::ceil (decSr / 70.0));

        float bestCorr = 0.0f;
        int   bestLag  = 0;
        for (int lag = minLag; lag <= maxLag; ++lag)
        {
            float corr = 0.0f;
            for (int i = 0; i < kWin - lag; ++i)
                corr += work[(size_t) i] * work[(size_t) (i + lag)];
            if (corr > bestCorr)
            {
                bestCorr = corr;
                bestLag  = lag;
            }
        }

        // Confidence: normalised autocorrelation peak vs. zero-lag energy.
        if (bestLag <= 0 || bestCorr < (float) energy * 0.4f)
            return 0.0f;

        return (float) (decSr / (double) bestLag);
    }

private:
    static constexpr int kWin = 512; // decimated-domain analysis window
    std::vector<float> ring, work;
    int    decim = 6, writePos = 0, decCount = 0;
    double decSr = 8000.0;
    float  acc = 0.0f;
};

// Single-channel time-domain pitch shifter: two delay taps a half-window apart,
// crossfaded, with the delay ramped to produce the pitch ratio. Clean for the
// small (<= ~1 semitone) corrections snap-to-key produces. Allocation only in
// prepare().
class PitchShifter
{
public:
    void prepare (double sampleRate)
    {
        const int len = juce::nextPowerOfTwo ((int) (sampleRate * 0.1)); // 100 ms
        buffer.assign ((size_t) len, 0.0f);
        mask = len - 1;
        writePos = 0;
        phase = 0.0f;
        window = (float) len * 0.5f;
        ratio = 1.0f;
    }

    void setRatio (float r) { ratio = juce::jlimit (0.5f, 2.0f, r); }

    void reset() { std::fill (buffer.begin(), buffer.end(), 0.0f); writePos = 0; phase = 0.0f; }

    // Keep the delay line warm without altering the signal (used when the ratio is
    // ~1.0, so re-engaging the shifter later doesn't read stale samples).
    void prime (const float* samples, int numSamples)
    {
        for (int n = 0; n < numSamples; ++n)
        {
            buffer[(size_t) writePos] = samples[n];
            writePos = (writePos + 1) & mask;
        }
    }

    void process (float* samples, int numSamples)
    {
        const float step = 1.0f - ratio; // delay advance per sample
        for (int n = 0; n < numSamples; ++n)
        {
            buffer[(size_t) writePos] = samples[n];

            // Two read taps half a window apart, triangular crossfade.
            const float p1 = phase;
            float p2 = phase + window;
            if (p2 >= (float) buffer.size()) p2 -= (float) buffer.size();

            const float g1 = std::abs ((window - phase) / window); // 0..1 triangular
            const float g2 = 1.0f - g1;

            samples[n] = readInterp ((float) writePos - p1) * g1
                       + readInterp ((float) writePos - p2) * g2;

            phase += step;
            if (phase < 0.0f)            phase += window;
            if (phase >= window)         phase -= window;

            writePos = (writePos + 1) & mask;
        }
    }

private:
    float readInterp (float delaySamples) const
    {
        float idx = delaySamples;
        while (idx < 0.0f) idx += (float) buffer.size();
        const int i0 = ((int) idx) & mask;
        const int i1 = (i0 + 1) & mask;
        const float f = idx - std::floor (idx);
        return buffer[(size_t) i0] * (1.0f - f) + buffer[(size_t) i1] * f;
    }

    std::vector<float> buffer;
    int   mask = 0, writePos = 0;
    float phase = 0.0f, window = 0.0f, ratio = 1.0f;
};

} // namespace gf
