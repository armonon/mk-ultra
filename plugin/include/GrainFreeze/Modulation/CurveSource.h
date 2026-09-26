#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <cmath>

namespace gf
{

// A drawable, tempo-locked modulation curve -- the shape you draw is the
// modulation. Nodes carry an x (0..1 across the loop), a y (-1..+1) and a
// tension for the segment that leaves them, so a handful of points can describe
// anything from a hard gate to a slow swell.
//
// The shape is edited on the message thread and read on the audio thread, so it
// is published by writing into the spare half of a double buffer and flipping an
// index -- no locks, no allocation, and the audio thread always sees a complete
// shape rather than a half-edited one.
class CurveSource
{
public:
    static constexpr int kMaxNodes = 32;

    struct Node
    {
        float x = 0.0f;       // 0..1 across the loop
        float y = 0.0f;       // -1..+1
        float tension = 0.0f; // -1 (ease out) .. 0 (linear) .. +1 (ease in)
    };

    struct Shape
    {
        int  count = 2;
        std::array<Node, kMaxNodes> nodes {};

        float valueAt (float phase) const noexcept
        {
            const int n = juce::jlimit (2, kMaxNodes, count);
            phase = phase - std::floor (phase);

            // Find the segment containing `phase`. The list is kept sorted by x.
            int i = 0;
            while (i < n - 1 && nodes[(size_t) (i + 1)].x <= phase)
                ++i;
            const auto& a = nodes[(size_t) i];
            const auto& b = nodes[(size_t) juce::jmin (i + 1, n - 1)];

            const float span = b.x - a.x;
            if (span <= 1.0e-6f)
                return a.y;

            float t = juce::jlimit (0.0f, 1.0f, (phase - a.x) / span);
            // Tension bends the segment without moving its endpoints.
            const float k = juce::jlimit (-0.98f, 0.98f, a.tension);
            if (std::abs (k) > 0.001f)
                t = k > 0.0f ? std::pow (t, 1.0f + k * 3.0f)
                             : 1.0f - std::pow (1.0f - t, 1.0f - k * 3.0f);
            return a.y + (b.y - a.y) * t;
        }
    };

    CurveSource()
    {
        // A usable default: a falling ramp, i.e. the classic sidechain duck.
        Shape s;
        s.count = 3;
        s.nodes[0] = { 0.0f,  1.0f, 0.4f };
        s.nodes[1] = { 0.55f, -1.0f, 0.0f };
        s.nodes[2] = { 1.0f, -1.0f, 0.0f };
        setShape (s);
        buffers[1] = buffers[0];
    }

    void prepare (double sampleRate)
    {
        sr = sampleRate > 0.0 ? sampleRate : 44100.0;
        phase = 0.0;
    }

    // ---- Message thread ----------------------------------------------------
    void setShape (const Shape& s)
    {
        const int next = 1 - live.load (std::memory_order_relaxed);
        buffers[(size_t) next] = s;
        live.store (next, std::memory_order_release);
    }
    Shape getShape() const { return buffers[(size_t) live.load (std::memory_order_acquire)]; }

    // ---- Audio thread ------------------------------------------------------
    // `bars` is the loop length; when the host gives a musical position the loop
    // is locked to the timeline so the curve lines up with the grid on every
    // playback, instead of drifting from wherever playback started.
    void advance (int numSamples, double bpm, float bars, bool havePpq, double ppqPosition)
    {
        const double beats = juce::jmax (0.125, (double) bars * 4.0);
        if (havePpq)
        {
            phase = std::fmod (ppqPosition / beats, 1.0);
            if (phase < 0.0) phase += 1.0;
        }
        else
        {
            const double beatsPerSecond = juce::jmax (20.0, bpm) / 60.0;
            phase += (beatsPerSecond / beats) * ((double) numSamples / sr);
            phase -= std::floor (phase);
        }
        const auto& s = buffers[(size_t) live.load (std::memory_order_acquire)];
        current = s.valueAt ((float) phase);
    }

    float value() const noexcept { return current; }
    float getPhase() const noexcept { return (float) phase; }

    // ---- Serialisation: the shape is patch data, so it rides in the state
    // tree as one compact string rather than as ninety-odd parameters.
    juce::String toString() const
    {
        const auto s = getShape();
        juce::String out;
        out << s.count;
        for (int i = 0; i < s.count; ++i)
            out << ',' << juce::String (s.nodes[(size_t) i].x, 4)
                << ',' << juce::String (s.nodes[(size_t) i].y, 4)
                << ',' << juce::String (s.nodes[(size_t) i].tension, 3);
        return out;
    }

    bool fromString (const juce::String& in)
    {
        juce::StringArray parts;
        parts.addTokens (in, ",", "");
        if (parts.size() < 1)
            return false;
        Shape s;
        s.count = juce::jlimit (2, kMaxNodes, parts[0].getIntValue());
        if (parts.size() < 1 + s.count * 3)
            return false;
        for (int i = 0; i < s.count; ++i)
        {
            s.nodes[(size_t) i].x       = juce::jlimit (0.0f, 1.0f, parts[1 + i * 3].getFloatValue());
            s.nodes[(size_t) i].y       = juce::jlimit (-1.0f, 1.0f, parts[2 + i * 3].getFloatValue());
            s.nodes[(size_t) i].tension = juce::jlimit (-1.0f, 1.0f, parts[3 + i * 3].getFloatValue());
        }
        // Endpoints are fixed and the list must be sorted, or valueAt() breaks.
        s.nodes[0].x = 0.0f;
        s.nodes[(size_t) (s.count - 1)].x = 1.0f;
        for (int i = 1; i < s.count; ++i)
            s.nodes[(size_t) i].x = juce::jmax (s.nodes[(size_t) i].x, s.nodes[(size_t) (i - 1)].x);
        setShape (s);
        return true;
    }

private:
    std::array<Shape, 2> buffers {};
    std::atomic<int> live { 0 };
    double sr = 44100.0;
    double phase = 0.0;
    float  current = 0.0f;
};

} // namespace gf
