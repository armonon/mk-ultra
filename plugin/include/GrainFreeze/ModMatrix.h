#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <random>
#include <vector>

namespace gf
{

// Identifies every modulatable parameter. Keep in sync with the APVTS layout.
enum class ParamId
{
    grainSize = 0,
    density,
    pitch,
    spray,
    spread,
    position,
    pitchJitter,
    reverbMix,
    output,
    // Prettifier knobs (same per-knob modulation/lock support as Entropy).
    echoTime,
    echoFeedback,
    echoMix,
    prettyReverbMix,
    chorusRate,
    chorusDepth,
    beautyAmount,
    polishWidth,
    bitCrush,
    numParams
};

inline constexpr int kNumModParams = (int) ParamId::numParams;

// One modulator per parameter: an LFO (rate, depth, shape) plus a sample-and-hold
// "wobble" (rate, depth), plus a routable amount from the single global source.
struct ModSlot
{
    std::atomic<float> lfoRate   { 0.0f };  // Hz; 0 = off
    std::atomic<float> lfoDepth  { 0.0f };  // 0..1 (fraction of param range)
    std::atomic<int>   lfoShape  { 0 };     // 0 sine, 1 triangle, 2 saw, 3 square
    std::atomic<float> shRate    { 0.0f };  // Hz; 0 = off  (sample & hold wobble)
    std::atomic<float> shDepth   { 0.0f };  // 0..1
    std::atomic<float> globalAmt { 0.0f };  // -1..1 routing from the global source

    // Per-knob options.
    std::atomic<bool>  bipolar   { true };  // true: offset swings +/-, false: 0..+
    std::atomic<float> rangeMin  { 0.0f };  // clamp modulated value, 0..1 of range
    std::atomic<float> rangeMax  { 1.0f };
    std::atomic<float> skew      { 0.0f };  // -1..1 curve applied to the offset

    // Runtime phase state (control thread only).
    double lfoPhase = 0.0;
    double shPhase  = 0.0;
    float  shValue  = 0.0f;
};

class ModMatrix
{
public:
    ModMatrix();

    void prepare (double controlRateHz);
    void reset();

    // Advance all modulators by one control-rate tick and compute the combined
    // modulation offset (in normalized -1..1 units) for each parameter.
    void tick();

    // Combined modulation offset for a parameter, normalized to roughly -1..1.
    // Atomic so the UI thread can read it safely while audio writes it.
    float getOffset (ParamId id) const { return offsets[(size_t) id].load (std::memory_order_relaxed); }

    ModSlot& slot (ParamId id) { return slots[(size_t) id]; }

    // Global mod source controls.
    void  setGlobalRate (float hz)   { globalRate.store (hz); }
    void  setGlobalShape (int s)     { globalShape.store (s); }
    void  setGlobalEnabled (bool e)  { globalEnabled.store (e); }
    float getGlobalValue() const     { return globalValue; }

private:
    static float shapeValue (int shape, double phase); // phase in 0..1

    std::array<ModSlot, kNumModParams> slots {};
    std::array<std::atomic<float>, kNumModParams> offsets {};

    std::atomic<float> globalRate  { 0.5f };
    std::atomic<int>   globalShape { 0 };
    std::atomic<bool>  globalEnabled { true };
    double globalPhase = 0.0;
    float  globalValue = 0.0f;

    double controlRate = 100.0; // Hz; how often tick() is called
    std::mt19937 rng { std::random_device{}() };
    std::uniform_real_distribution<float> dist { -1.0f, 1.0f };
};

} // namespace gf
