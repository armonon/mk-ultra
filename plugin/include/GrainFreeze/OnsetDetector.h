#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

namespace gf
{

// Finds the hits in the incoming audio and reports where they land inside the
// block, so grains can be spawned ON the transients instead of from a
// free-running clock. That is the difference between a granular that smears a
// drum loop and one that stays inside its groove.
//
// The method is a fast envelope against a slow one: a hit is a moment where the
// signal jumps well above its own recent average, with a refractory period so a
// single hit fires once rather than smearing into a burst.
class OnsetDetector
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate > 0.0 ? sampleRate : 44100.0;
        fastUp   = coeffFor (0.5);
        fastDown = coeffFor (30.0);
        slowUp   = coeffFor (60.0);
        slowDown = coeffFor (220.0);
        peakDown = coeffFor (2000.0);
        reset();
    }

    void reset()
    {
        fast = slow = peak = 0.0f;
        sinceLast = 1 << 20;
        armed = true;
    }

    // 0 = only clear hits, 1 = almost anything with an edge on it.
    void setSensitivity (float s) { sensitivity = juce::jlimit (0.0f, 1.0f, s); }

    // Minimum gap between hits. Shorter lets fast rolls through; longer keeps
    // one hit from firing twice.
    void setRefractoryMs (float ms) { refractoryMs = juce::jlimit (5.0f, 200.0f, ms); }

    // Fills `offsets` with the sample positions of hits inside this block and
    // returns how many there were.
    int process (const juce::AudioBuffer<float>& in, int* offsets, int maxOffsets)
    {
        const int n = in.getNumSamples();
        const int ch = in.getNumChannels();
        if (n <= 0 || ch <= 0 || maxOffsets <= 0)
            return 0;

        // Two conditions. A hit has to beat its own recent average (so it is an
        // edge, not just loud), and it has to be big enough relative to the
        // loudest thing heard lately (so sensitivity means "how quiet a hit
        // counts" -- ghost notes in, or only the ones on the beat).
        const float ratio = juce::jmap (sensitivity, 0.0f, 1.0f, 2.6f, 1.12f);
        const float gate = juce::jmap (sensitivity, 0.0f, 1.0f, 0.35f, 0.02f);
        const float floorLevel = juce::jmap (sensitivity, 0.0f, 1.0f, 0.02f, 0.002f);
        const int refractory = (int) (refractoryMs * 0.001f * sr);

        int found = 0;
        for (int i = 0; i < n; ++i)
        {
            float mono = 0.0f;
            for (int c = 0; c < ch; ++c)
                mono = juce::jmax (mono, std::abs (in.getSample (c, i)));

            fast += (mono > fast ? fastUp : fastDown) * (mono - fast);
            slow += (mono > slow ? slowUp : slowDown) * (mono - slow);
            peak = mono > peak ? mono : peak + peakDown * (mono - peak);

            ++sinceLast;
            const bool over = fast > floorLevel
                           && fast > slow * ratio
                           && fast > peak * gate;
            if (over && armed && sinceLast >= refractory)
            {
                if (found < maxOffsets)
                    offsets[found++] = i;
                sinceLast = 0;
                armed = false;
            }
            else if (! over && fast < slow * (ratio * 0.75f))
            {
                armed = true;   // re-arm once the signal has fallen back
            }
        }
        return found;
    }

private:
    float coeffFor (double ms) const
    {
        return (float) (1.0 - std::exp (-1.0 / juce::jmax (1.0, ms * 0.001 * sr)));
    }

    double sr = 44100.0;
    float fast = 0.0f, slow = 0.0f, peak = 0.0f;
    float fastUp = 0.1f, fastDown = 0.01f, slowUp = 0.01f, slowDown = 0.002f, peakDown = 0.0001f;
    float sensitivity = 0.5f;
    float refractoryMs = 40.0f;
    int   sinceLast = 1 << 20;
    bool  armed = true;
};

} // namespace gf
