#pragma once
// Shared helpers for tests that instantiate the full GrainFreezeProcessor.
#include <juce_events/juce_events.h>
#include "GrainFreeze/PluginProcessor.h"
#include <memory>
#include <cmath>
#include <random>

namespace fx
{
    // NOTE: tests that build a processor declare a local
    // juce::ScopedJuceInitialiser_GUI. A function-static one is destroyed
    // during exit() after other statics, and DeletedAtShutdown double-frees.

    inline void setParam (GrainFreezeProcessor& p, const juce::String& id, float realValue)
    {
        auto* param = p.apvts.getParameter (id);
        REQUIRE (param != nullptr);
        param->setValueNotifyingHost (param->getNormalisableRange().convertTo0to1 (realValue));
    }

    // A "real" test signal: a minor triad with a little noise, so granular /
    // spectral / reverb stages have harmonic material and transients to chew on.
    inline void fillMusical (juce::AudioBuffer<float>& b, double sr, int startSample, unsigned seed = 3)
    {
        std::mt19937 rng (seed);
        std::uniform_real_distribution<float> d (-1.0f, 1.0f);
        const double f[] = { 220.0, 261.63, 329.63 };
        for (int c = 0; c < b.getNumChannels(); ++c)
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                const double t = (double) (startSample + i) / sr;
                float v = 0.0f;
                for (double ff : f) v += (float) std::sin (2.0 * juce::MathConstants<double>::pi * ff * t);
                // a soft 2 Hz amplitude pulse so envelope followers see movement
                const float env = 0.6f + 0.4f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 2.0 * t);
                b.setSample (c, i, (v / 3.0f) * 0.4f * env + d (rng) * 0.02f);
            }
    }

    inline float rms (const juce::AudioBuffer<float>& b, int from = 0)
    {
        double a = 0.0; int n = 0;
        for (int c = 0; c < b.getNumChannels(); ++c)
            for (int i = from; i < b.getNumSamples(); ++i) { const double v = b.getSample (c, i); a += v * v; ++n; }
        return (float) std::sqrt (a / (double) juce::jmax (1, n));
    }
    inline float peak (const juce::AudioBuffer<float>& b)
    {
        float m = 0.0f;
        for (int c = 0; c < b.getNumChannels(); ++c) m = juce::jmax (m, b.getMagnitude (c, 0, b.getNumSamples()));
        return m;
    }
    inline bool allFinite (const juce::AudioBuffer<float>& b)
    {
        for (int c = 0; c < b.getNumChannels(); ++c)
            for (int i = 0; i < b.getNumSamples(); ++i)
                if (! std::isfinite (b.getSample (c, i))) return false;
        return true;
    }
}
