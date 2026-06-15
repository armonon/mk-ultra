#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <cmath>

namespace gf
{

// Sample Mode: continuously records a rolling window of audio from both the live
// input and the processed output. On Freeze it snapshots the most recent N
// seconds of the chosen source into a sample buffer that then loops forever and
// can be retriggered / transposed from the keyboard (variable-rate playback with
// linear interpolation). All capture/playback happens on the audio thread; the
// UI only flips an atomic "freeze requested" flag and reads ready() for display.
class SampleFreezeEngine
{
public:
    static constexpr float kMinSeconds = 1.0f;
    static constexpr float kMaxSeconds = 10.0f;

    void prepare (double sampleRate, int numChannels)
    {
        sr = sampleRate > 0.0 ? sampleRate : 44100.0;
        channels = juce::jmax (1, numChannels);
        const int ringLen = (int) std::ceil (sr * kMaxSeconds) + 8;
        inputRing.setSize (channels, ringLen);
        outputRing.setSize (channels, ringLen);
        sampleBuf.setSize (channels, ringLen);
        inputRing.clear();
        outputRing.clear();
        sampleBuf.clear();
        inWrite = outWrite = 0;
        sampleLen = 0;
        readPos = 0.0;
        hasSample.store (false, std::memory_order_release);
    }

    // Record one block of the live input / processed output into the rings.
    void pushInput  (const juce::AudioBuffer<float>& b) { writeRing (inputRing,  inWrite,  b); }
    void pushOutput (const juce::AudioBuffer<float>& b) { writeRing (outputRing, outWrite, b); }

    // Snapshot the most recent windowSeconds of the chosen source (0 = input,
    // 1 = output) into the looping sample buffer. Audio-thread only.
    void freeze (float windowSeconds, int source)
    {
        const int ringLen = inputRing.getNumSamples();
        if (ringLen <= 1) return;

        const float secs = juce::jlimit (kMinSeconds, kMaxSeconds, windowSeconds);
        const int want = juce::jlimit (1, ringLen - 1, (int) std::round (sr * secs));

        const auto& ring = (source == 1) ? outputRing : inputRing;
        const int   write = (source == 1) ? outWrite  : inWrite;

        for (int ch = 0; ch < channels; ++ch)
        {
            const float* src = ring.getReadPointer (ch);
            float*       dst = sampleBuf.getWritePointer (ch);
            int r = (write - want + ringLen) % ringLen;
            for (int i = 0; i < want; ++i)
            {
                dst[i] = src[r];
                if (++r >= ringLen) r = 0;
            }
        }

        // Short equal-power-ish fades at the loop seam to avoid a click.
        const int fade = juce::jmin (want / 8, (int) (sr * 0.01));
        for (int ch = 0; ch < channels; ++ch)
        {
            float* d = sampleBuf.getWritePointer (ch);
            for (int i = 0; i < fade; ++i)
            {
                const float g = (float) i / (float) fade;
                d[i]            *= g;
                d[want - 1 - i] *= g;
            }
        }

        sampleLen = want;
        readPos = 0.0;
        hasSample.store (true, std::memory_order_release);
    }

    bool ready() const { return hasSample.load (std::memory_order_acquire) && sampleLen > 1; }

    // Mix looped playback of the frozen sample into out. rate = playback speed
    // multiplier (1 = original pitch), level = output gain.
    void render (juce::AudioBuffer<float>& out, float rate, float level)
    {
        if (! ready()) return;
        const int n     = out.getNumSamples();
        const int outCh = out.getNumChannels();
        const int len   = sampleLen;
        const double inc = juce::jlimit (0.0625, 8.0, (double) rate);
        double pos = readPos;
        for (int i = 0; i < n; ++i)
        {
            const int    i0   = (int) pos;
            const double frac = pos - (double) i0;
            const int    i1   = (i0 + 1) % len;
            for (int ch = 0; ch < outCh; ++ch)
            {
                const float* s = sampleBuf.getReadPointer (juce::jmin (ch, channels - 1));
                const float v = (float) (s[i0] * (1.0 - frac) + s[i1] * frac);
                out.addSample (ch, i, v * level);
            }
            pos += inc;
            while (pos >= len) pos -= len;
        }
        readPos = pos;
    }

    void clear()
    {
        hasSample.store (false, std::memory_order_release);
        sampleLen = 0;
        readPos = 0.0;
    }

private:
    void writeRing (juce::AudioBuffer<float>& ring, int& write, const juce::AudioBuffer<float>& b)
    {
        const int ringLen = ring.getNumSamples();
        if (ringLen == 0) return;
        const int n   = b.getNumSamples();
        const int bch = b.getNumChannels();
        for (int ch = 0; ch < channels; ++ch)
        {
            const float* src = b.getReadPointer (juce::jmin (ch, bch - 1));
            float*       dst = ring.getWritePointer (ch);
            int w = write;
            for (int i = 0; i < n; ++i)
            {
                dst[w] = src[i];
                if (++w >= ringLen) w = 0;
            }
        }
        write = (write + n) % ringLen;
    }

    double sr = 44100.0;
    int channels = 2;
    juce::AudioBuffer<float> inputRing, outputRing, sampleBuf;
    int inWrite = 0, outWrite = 0;
    int sampleLen = 0;
    double readPos = 0.0;
    std::atomic<bool> hasSample { false };
};

} // namespace gf
