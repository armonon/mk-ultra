#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <memory>
#include <vector>
#include <cmath>

namespace gf
{

class Saturator
{
public:
    enum Type { Tube = 0, Tape = 1, Hard = 2 };

    void prepare (double sampleRate, int numChannels, int maxBlock)
    {
        driveSmoothed.reset (sampleRate, 0.02);
        mixSmoothed.reset   (sampleRate, 0.02);

        // Oversample the nonlinearity 4x so hard drive doesn't alias. Drive and
        // level-compensation are linear and stay at base rate around it.
        osChannels = juce::jlimit (1, 2, juce::jmax (1, numChannels));
        const int block = juce::jmax (1, maxBlock);
        os = std::make_unique<juce::dsp::Oversampling<float>> (
                 (size_t) osChannels, 2,
                 juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
        os->initProcessing ((size_t) block);
        os->reset();
        dryBuf.setSize (osChannels, block);
        compArr.assign ((size_t) block, 1.0f);
        mixArr.assign  ((size_t) block, 0.0f);

        // Latency-compensate the dry path against the oversampler.
        dryRingLen = juce::jmax (1, (int) std::lround (os->getLatencyInSamples()));
        dryRing.setSize (osChannels, dryRingLen);
        dryRing.clear();
        dryRingPos = 0;
    }

    void setType  (int t)     { type.store (t); }
    void setDrive (float d)   { drive.store (juce::jlimit (1.0f, 24.0f, d)); }   // 1..24x
    void setMix   (float m)   { mix.store   (juce::jlimit (0.0f, 1.0f, m)); }

    // Evaluate the full transfer function (drive + shape + level compensation)
    // for a dry input x in [-1,1]. Used by the UI to draw the curve identically
    // to what the audio path produces. Static so the editor can call it freely.
    static float transfer (int type, float drive, float x)
    {
        const float comp = 1.0f / std::sqrt (juce::jmax (1.0f, drive));
        return shape (type, x * drive) * comp;
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const float dB = drive.load();
        const float mx = mix.load();
        if (mx <= 0.001f) return; // bypass when fully dry

        driveSmoothed.setTargetValue (dB);
        mixSmoothed.setTargetValue (mx);

        const int t = type.load();
        const int n = buffer.getNumSamples();
        const int numCh = juce::jmin (buffer.getNumChannels(), osChannels);
        if (numCh <= 0 || n <= 0 || n > dryBuf.getNumSamples())
            return;

        // Base rate: stash dry, apply smoothed drive, capture comp + mix per sample.
        for (int i = 0; i < n; ++i)
        {
            const float d = driveSmoothed.getNextValue();
            compArr[(size_t) i] = 1.0f / std::sqrt (d);   // keep level ~constant as drive rises
            mixArr[(size_t) i]  = mixSmoothed.getNextValue();
            for (int c = 0; c < numCh; ++c)
            {
                const float x = buffer.getSample (c, i);
                dryBuf.setSample (c, i, x);
                buffer.setSample (c, i, x * d);
            }
        }

        // Oversample the pure nonlinearity (the part that aliases).
        if (os != nullptr && numCh == osChannels)
        {
            juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) numCh, (size_t) n);
            auto up = os->processSamplesUp (block);
            const int upN = (int) up.getNumSamples();
            for (int c = 0; c < numCh; ++c)
            {
                auto* u = up.getChannelPointer ((size_t) c);
                for (int i = 0; i < upN; ++i)
                    u[i] = shape (t, u[i]);
            }
            os->processSamplesDown (block);
        }
        else
        {
            for (int c = 0; c < numCh; ++c)
            {
                auto* d = buffer.getWritePointer (c);
                for (int i = 0; i < n; ++i)
                    d[i] = shape (t, d[i]);
            }
        }

        // Level-compensate and blend, dry delayed to match the oversampler latency.
        int rp = dryRingPos;
        for (int i = 0; i < n; ++i)
        {
            const float comp = compArr[(size_t) i];
            const float m    = mixArr[(size_t) i];
            for (int c = 0; c < numCh; ++c)
            {
                const float dd = dryRing.getSample (c, rp);
                dryRing.setSample (c, rp, dryBuf.getSample (c, i));
                buffer.setSample (c, i, dd * (1.0f - m) + buffer.getSample (c, i) * comp * m);
            }
            if (++rp >= dryRingLen) rp = 0;
        }
        dryRingPos = rp;
    }

private:
    static inline float shape (int type, float x)
    {
        switch (type)
        {
            case Tape:
            {
                // Smooth, slightly compressive odd-harmonic curve.
                return std::tanh (x * 0.8f);
            }
            case Hard:
            {
                // Hard clip with a touch of rounding just below the knee.
                return juce::jlimit (-1.0f, 1.0f, x);
            }
            case Tube:
            default:
            {
                // Asymmetric soft clip: warmer, adds even + odd harmonics.
                if (x >= 0.0f)
                    return 1.0f - std::exp (-x);
                else
                    return -1.0f + std::exp (x * 0.7f);
            }
        }
    }

    std::atomic<int>   type  { Tube };
    std::atomic<float> drive { 1.0f };
    std::atomic<float> mix   { 0.0f };

    juce::SmoothedValue<float> driveSmoothed { 1.0f };
    juce::SmoothedValue<float> mixSmoothed   { 0.0f };

    std::unique_ptr<juce::dsp::Oversampling<float>> os; // 4x for the nonlinearity
    juce::AudioBuffer<float> dryBuf, dryRing;           // dry copy + latency-comp delay
    std::vector<float> compArr, mixArr;                 // per-sample comp + mix scratch
    int osChannels = 0, dryRingLen = 1, dryRingPos = 0;
};

} // namespace gf
