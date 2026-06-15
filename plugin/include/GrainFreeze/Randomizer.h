#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "GrainFreeze/ModMatrix.h"
#include <array>
#include <random>

namespace gf
{

// String IDs for every parameter, indexed by ParamId. Single source of truth
// shared by the processor, randomizer, and mod matrix.
inline const char* paramIdString (ParamId id)
{
    switch (id)
    {
        case ParamId::grainSize:   return "grainSize";
        case ParamId::density:     return "density";
        case ParamId::pitch:       return "pitch";
        case ParamId::spray:       return "spray";
        case ParamId::spread:      return "spread";
        case ParamId::position:    return "position";
        case ParamId::pitchJitter: return "pitchJitter";
        case ParamId::reverbMix:   return "reverbMix";
        case ParamId::output:      return "output";
        case ParamId::echoTime:        return "echoTimeMs";
        case ParamId::echoFeedback:    return "echoFeedback";
        case ParamId::echoMix:         return "echoMix";
        case ParamId::prettyReverbMix: return "prettyReverbMix";
        case ParamId::chorusRate:      return "chorusRate";
        case ParamId::chorusDepth:     return "chorusDepth";
        case ParamId::beautyAmount:    return "beautyAmount";
        case ParamId::polishWidth:     return "polishWidth";
        case ParamId::bitCrush:        return "crushBits";
        default:                   return "";
    }
}

// "Musical" sub-ranges used by the randomizer so the dice roll lands on
// usable territory instead of the full parameter extremes.
struct MusicalRange { float lo, hi; };

inline MusicalRange musicalRange (ParamId id)
{
    switch (id)
    {
        case ParamId::grainSize:   return { 40.0f, 250.0f };   // ms
        case ParamId::density:     return { 12.0f, 60.0f };    // grains/s
        case ParamId::pitch:       return { -12.0f, 12.0f };   // semis
        case ParamId::spray:       return { 0.0f, 120.0f };    // ms
        case ParamId::spread:      return { 0.2f, 0.9f };
        case ParamId::position:    return { 0.0f, 1.0f };
        case ParamId::pitchJitter: return { 0.0f, 4.0f };      // semis
        case ParamId::reverbMix:   return { 0.0f, 0.6f };
        case ParamId::output:      return { 0.55f, 0.9f };
        case ParamId::echoTime:        return { 80.0f, 600.0f }; // ms
        case ParamId::echoFeedback:    return { 0.1f, 0.6f };
        case ParamId::echoMix:         return { 0.0f, 0.5f };
        case ParamId::prettyReverbMix: return { 0.0f, 0.5f };
        case ParamId::chorusRate:      return { 0.05f, 3.0f };   // Hz
        case ParamId::chorusDepth:     return { 0.0f, 0.6f };
        case ParamId::beautyAmount:    return { 0.0f, 0.6f };
        case ParamId::polishWidth:     return { 0.0f, 0.6f };
        case ParamId::bitCrush:        return { 6.0f, 16.0f }; // bits
        default:                   return { 0.0f, 1.0f };
    }
}

class Randomizer
{
public:
    enum class Mode
    {
        subtle = 0,
        musical,
        glitch,
        ambient,
        horror,
        destroyed,
        cinematic,
        beautiful,
        dream,
        angel,
        vintage
    };

    explicit Randomizer (juce::AudioProcessorValueTreeState& state) : apvts (state) {}

    void setLocked (ParamId id, bool locked) { locks[(size_t) id] = locked; }
    bool isLocked  (ParamId id) const         { return locks[(size_t) id]; }

    // Roll a new preset within musical ranges, skipping locked parameters.
    void randomize()
    {
        randomize (Mode::musical, 1.0f);
    }

    void randomize (Mode mode, float amount)
    {
        randomize (mode, amount, 0, kNumModParams - 1);
    }

    // Randomize only a contiguous span of ParamIds, e.g. just the Entropy knobs
    // (grainSize..output) or just the Prettifier knobs (echoTime..bitCrush), so
    // each tab can roll independently.
    void randomize (Mode mode, float amount, int firstId, int lastId)
    {
        const float t = juce::jlimit (0.0f, 1.0f, amount);
        firstId = juce::jlimit (0, kNumModParams - 1, firstId);
        lastId  = juce::jlimit (0, kNumModParams - 1, lastId);
        for (int i = firstId; i <= lastId; ++i)
        {
            const auto id = (ParamId) i;
            if (locks[(size_t) i]) continue;

            auto r = musicalRange (id);
            const float width = (r.hi - r.lo);
            if (mode == Mode::subtle)       r = { r.lo + width * 0.25f, r.hi - width * 0.25f };
            else if (mode == Mode::glitch)  r = { r.lo, r.hi };
            else if (mode == Mode::ambient) r = { r.lo, r.lo + width * 0.6f };
            else if (mode == Mode::destroyed) r = { r.lo + width * 0.4f, r.hi };
            else if (mode == Mode::beautiful) r = { r.lo + width * 0.2f, r.lo + width * 0.7f };
            else if (mode == Mode::dream || mode == Mode::angel) r = { r.lo + width * 0.15f, r.lo + width * 0.65f };

            const float current = apvts.getRawParameterValue (paramIdString (id))->load();
            const float candidate = r.lo + rand01() * (r.hi - r.lo);
            const float value = juce::jmap (t, current, candidate);

            if (auto* p = apvts.getParameter (paramIdString (id)))
            {
                const auto& range = apvts.getParameterRange (paramIdString (id));
                p->setValueNotifyingHost (range.convertTo0to1 (value));
            }
        }
    }

    void mutate (float amount) { randomize (Mode::musical, amount); }

    // Randomize the Mix-console params (sends/returns/output/blend + EQ) within
    // musical ranges. These aren't ParamIds, so they're handled explicitly.
    void randomizeMix (float amount)
    {
        const float t = juce::jlimit (0.0f, 1.0f, amount);
        struct MR { const char* id; float lo, hi; };
        static const MR mixRanges[] = {
            { "dryLevel",         0.0f,  0.5f },
            { "entropySend",      0.4f,  1.0f },
            { "entropyReturn",    0.4f,  1.0f },
            { "prettifierSend",   0.4f,  1.0f },
            { "prettifierReturn", 0.4f,  1.0f },
            { "mixOutput",        0.8f,  1.2f },
            { "chaosBeauty",      0.2f,  0.8f },
            { "eqLow",           -6.0f,  6.0f },
            { "eqMid",           -5.0f,  5.0f },
            { "eqHigh",          -6.0f,  6.0f },
            { "eqLoFi",           0.0f,  0.4f },
        };
        for (const auto& m : mixRanges)
        {
            if (auto* p = apvts.getParameter (m.id))
            {
                const float current   = apvts.getRawParameterValue (m.id)->load();
                const float candidate = m.lo + rand01() * (m.hi - m.lo);
                const float value     = juce::jmap (t, current, candidate);
                p->setValueNotifyingHost (apvts.getParameterRange (m.id).convertTo0to1 (value));
            }
        }
    }

private:
    float rand01() { return dist (rng); }

    juce::AudioProcessorValueTreeState& apvts;
    std::array<bool, kNumModParams> locks { false };
    std::mt19937 rng { std::random_device{}() };
    std::uniform_real_distribution<float> dist { 0.0f, 1.0f };
};

} // namespace gf
