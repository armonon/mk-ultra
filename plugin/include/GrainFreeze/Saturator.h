#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <cmath>

namespace gf
{

class Saturator
{
public:
    enum Type { Tube = 0, Tape = 1, Hard = 2 };

    void prepare (double sampleRate, int numChannels)
    {
        juce::ignoreUnused (sampleRate, numChannels);
        driveSmoothed.reset (sampleRate, 0.02);
        mixSmoothed.reset   (sampleRate, 0.02);
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

        for (int i = 0; i < n; ++i)
        {
            const float d = driveSmoothed.getNextValue();
            const float m = mixSmoothed.getNextValue();
            // Compensation keeps perceived level roughly constant as drive rises.
            const float comp = 1.0f / std::sqrt (d);

            for (int c = 0; c < buffer.getNumChannels(); ++c)
            {
                const float x = buffer.getSample (c, i);
                const float wet = shape (t, x * d) * comp;
                buffer.setSample (c, i, x * (1.0f - m) + wet * m);
            }
        }
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
};

} // namespace gf
