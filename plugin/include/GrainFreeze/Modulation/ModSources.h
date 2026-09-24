#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <random>

namespace gf
{

// Extra modulation sources for the Mod Matrix: a second LFO (free or tempo
// synced), a tempo-synced step sequencer and a random sample & hold. They are
// plain generators -- no audio, no parameters of their own -- advanced once per
// audio block and read as -1..+1 signals, the same contract as the Global LFO
// and the Time Breaker gate the matrix already routes.
class ModSources
{
public:
    static constexpr int kMaxSteps = 16;

    // Note divisions in beats, shared with the Time Breaker / sync dropdowns:
    // 1/1, 1/2, 1/4, 1/8, 1/8T, 1/16, 1/16T, 1/32.
    static constexpr float kDivBeats[8] = { 4.0f, 2.0f, 1.0f, 0.5f, 1.0f / 3.0f, 0.25f, 1.0f / 6.0f, 0.125f };

    struct Params
    {
        float lfoRate     = 0.5f;   // Hz, used when lfoDivision == 0 ("Free")
        int   lfoShape    = 0;      // 0 sine, 1 triangle, 2 saw, 3 square
        int   lfoDivision = 0;      // 0 = free, 1..8 = kDivBeats[index - 1]
        int   stepDivision = 5;     // index into kDivBeats (5 = 1/16)
        int   stepLength   = 8;     // active steps, 1..kMaxSteps
        float stepSmooth   = 0.0f;  // 0 = hard steps, 1 = ~250 ms glide
        const float* steps = nullptr;   // kMaxSteps values in -1..+1
        float randomRate   = 4.0f;  // new random value this many times a second
    };

    void prepare (double sampleRate)
    {
        sr = sampleRate > 0.0 ? sampleRate : 44100.0;
        reset();
    }

    void reset()
    {
        lfoPhase = stepPhase = randomPhase = 0.0;
        stepPos = 0;
        lfoValue = stepValue = 0.0f;
        randomValue = 0.0f;
    }

    // Deterministic random source for tests.
    void setSeed (unsigned seed) { rng.seed (seed); }

    void advance (int numSamples, double bpm, const Params& p)
    {
        if (numSamples <= 0)
            return;
        const double dt = (double) numSamples / sr;
        const double beatsPerSecond = juce::jmax (20.0, bpm) / 60.0;

        // ---- LFO 2.
        const double hz = p.lfoDivision > 0
                              ? beatsPerSecond / (double) kDivBeats[juce::jlimit (0, 7, p.lfoDivision - 1)]
                              : (double) juce::jmax (0.0f, p.lfoRate);
        lfoPhase += hz * dt;
        lfoPhase -= std::floor (lfoPhase);
        lfoValue = shapeValue (p.lfoShape, lfoPhase);

        // ---- Step sequencer: one step per note division, wrapping at stepLength.
        const int length = juce::jlimit (1, kMaxSteps, p.stepLength);
        const double stepHz = beatsPerSecond / (double) kDivBeats[juce::jlimit (0, 7, p.stepDivision)];
        stepPhase += stepHz * dt;
        while (stepPhase >= 1.0)
        {
            stepPhase -= 1.0;
            stepPos = (stepPos + 1) % length;
        }
        if (stepPos >= length)
            stepPos = 0;
        const float targetStep = p.steps != nullptr
                                     ? juce::jlimit (-1.0f, 1.0f, p.steps[stepPos]) : 0.0f;
        const float smooth = juce::jlimit (0.0f, 1.0f, p.stepSmooth);
        if (smooth <= 0.001f)
            stepValue = targetStep;
        else
        {
            // One-pole glide, 5 ms .. 255 ms of time constant.
            const double tau = 0.005 + (double) smooth * 0.25;
            const float a = (float) (1.0 - std::exp (-dt / tau));
            stepValue += (targetStep - stepValue) * a;
        }

        // ---- Random sample & hold.
        randomPhase += (double) juce::jmax (0.0f, p.randomRate) * dt;
        while (randomPhase >= 1.0)
        {
            randomPhase -= 1.0;
            randomValue = dist (rng);
        }
    }

    float lfo()    const { return lfoValue; }
    float step()   const { return stepValue; }
    float random() const { return randomValue; }
    // Which step is playing, so the UI can light it.
    int   currentStep() const { return stepPos; }

private:
    static float shapeValue (int shape, double phase)
    {
        switch (juce::jlimit (0, 3, shape))
        {
            case 1:  return (float) (4.0 * std::abs (phase - 0.5) - 1.0);   // triangle
            case 2:  return (float) (2.0 * phase - 1.0);                    // saw
            case 3:  return phase < 0.5 ? 1.0f : -1.0f;                     // square
            default: return (float) std::sin (phase * juce::MathConstants<double>::twoPi);
        }
    }

    double sr = 44100.0;
    double lfoPhase = 0.0, stepPhase = 0.0, randomPhase = 0.0;
    int    stepPos = 0;
    float  lfoValue = 0.0f, stepValue = 0.0f, randomValue = 0.0f;

    std::mt19937 rng { 0xC0FFEEu };
    std::uniform_real_distribution<float> dist { -1.0f, 1.0f };
};

} // namespace gf
