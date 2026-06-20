#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>

namespace gf
{

class MidiNoteController
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        glide.reset (sampleRate, 0.0);
        glide.setCurrentAndTargetValue (0.0f);
    }

    void reset()
    {
        count = 0;
        glide.setCurrentAndTargetValue (0.0f);
        velocity.store (0.0f, std::memory_order_relaxed);
        gateOpen.store (false, std::memory_order_relaxed);
    }

    void setRoot (int n)       { root.store (juce::jlimit (0, 127, n), std::memory_order_relaxed); }
    void setGlideMs (float ms) { glideMs.store (juce::jmax (0.0f, ms), std::memory_order_relaxed); }
    void updateGlideTime()     { glide.reset (sr, glideMs.load (std::memory_order_relaxed) / 1000.0); }

    void handleMessage (const juce::MidiMessage& m)
    {
        if (m.isNoteOn())
            pushNote (m.getNoteNumber(), m.getFloatVelocity());
        else if (m.isNoteOff())
            removeNote (m.getNoteNumber());
        else if (m.isAllNotesOff() || m.isAllSoundOff())
            count = 0;

        updateTarget();
    }

    float nextOffsetSemis() { return glide.getNextValue(); }
    bool  isGateOpen() const { return gateOpen.load (std::memory_order_relaxed); }
    float getVelocity() const { return velocity.load (std::memory_order_relaxed); }

    // Copies up to `maxNotes` of the active note semitone-offsets into `out` and
    // returns the number written. Used by polyphonic granular mode so a held
    // chord can drive grain pitches across all the held notes.
    int copyActiveOffsets (float* out, int maxNotes) const
    {
        const int r = root.load (std::memory_order_relaxed);
        const int n = juce::jmin (count, maxNotes);
        for (int i = 0; i < n; ++i)
            out[i] = (float) (stack[(size_t) i] - r);
        return n;
    }

private:
    void pushNote (int note, float vel)
    {
        removeNote (note);
        if (count < (int) stack.size())
            stack[(size_t) count++] = note;
        velocity.store (vel, std::memory_order_relaxed);
        gateOpen.store (true, std::memory_order_relaxed);
    }

    void removeNote (int note)
    {
        for (int i = 0; i < count; ++i)
        {
            if (stack[(size_t) i] == note)
            {
                for (int j = i; j < count - 1; ++j)
                    stack[(size_t) j] = stack[(size_t) (j + 1)];
                --count;
                break;
            }
        }

        if (count == 0)
            gateOpen.store (false, std::memory_order_relaxed);
    }

    void updateTarget()
    {
        if (count > 0)
            glide.setTargetValue ((float) (stack[(size_t) (count - 1)] - root.load (std::memory_order_relaxed)));
        else
            glide.setTargetValue (0.0f);
    }

    double sr = 44100.0;
    std::array<int, 16> stack {};
    int count = 0;
    juce::SmoothedValue<float> glide { 0.0f };
    std::atomic<int> root { 60 };
    std::atomic<float> glideMs { 0.0f };
    std::atomic<float> velocity { 0.0f };
    std::atomic<bool> gateOpen { false };
};

} // namespace gf
