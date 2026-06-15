#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <random>

namespace gf
{

// A single grain voice. Reads from the capture buffer with a Hann window
// applied, at a (fractional) read position and a resample ratio for pitch.
struct Grain
{
    bool   active      = false;
    double readPos     = 0.0;   // fractional sample index into the capture buffer
    double rate        = 1.0;   // resample ratio (pitch); 1.0 = original pitch
    int    samplesLeft = 0;     // remaining output samples for this grain
    int    lengthSamps = 1;     // total grain length in output samples
    float  pan         = 0.5f;  // 0 = left, 1 = right
    float  amp         = 1.0f;  // per-grain amplitude
};

class GranularEngine
{
public:
    GranularEngine() = default;

    void prepare (double sampleRate, int maxBlockSize, int numChannels);
    void reset();

    // Push incoming audio into the circular capture buffer. When not frozen,
    // grains read from the most recent material; when frozen, the buffer is
    // held and grains read from the frozen window.
    void pushInput (const juce::AudioBuffer<float>& input);

    // Render grain cloud into the output buffer (replaces contents).
    void process (juce::AudioBuffer<float>& output);

    // ---- Parameters (thread-safe atomics, set from the message thread) ----
    void setFrozen      (bool b)        { frozen.store (b); }
    void setGrainSizeMs (float ms)      { grainSizeMs.store (ms); }
    void setDensity     (float perSec)  { density.store (perSec); }
    void setPitchSemis  (float st)      { pitchSemis.store (st); }
    void setNoteOffsetSemis (float st)  { noteOffset.store (st); }
    void setSprayMs     (float ms)      { sprayMs.store (ms); }
    void setSpread      (float n)       { spread.store (juce::jlimit (0.0f, 1.0f, n)); }
    void setPosition    (float n)       { position.store (juce::jlimit (0.0f, 1.0f, n)); }
    void setPitchJitter (float st)      { pitchJitter.store (st); }
    void setOutputGain  (float n)       { outputGain.store (n); }
    void setVelocity    (float n)       { velocity.store (juce::jlimit (0.0f, 1.0f, n)); }
    void setVelocityToAmp (float n)     { velocityToAmp.store (juce::jlimit (0.0f, 1.0f, n)); }

    bool isPrepared() const { return prepared; }

private:
    void spawnGrain();
    inline float readInterpolated (int channel, double pos) const;

    static constexpr int kMaxGrains    = 64;
    static constexpr int kWindowPoints = 2048;       // Hann lookup table size
    double captureSeconds              = 4.0;        // length of capture buffer

    std::array<Grain, kMaxGrains> grains {};
    std::array<float, kWindowPoints> window {};      // precomputed Hann window

    juce::AudioBuffer<float> capture;                // circular capture buffer
    int   captureWrite = 0;                          // write head
    int   captureLen   = 0;                          // length in samples
    bool  prepared     = false;

    double sr        = 44100.0;
    int    channels  = 2;

    double grainClock = 0.0;                         // accumulator for grain spawning

    std::mt19937 rng { std::random_device{}() };
    std::uniform_real_distribution<float> dist { 0.0f, 1.0f };
    inline float rand01() { return dist (rng); }

    // Parameters
    std::atomic<bool>  frozen      { false };
    std::atomic<float> grainSizeMs { 120.0f };
    std::atomic<float> density     { 28.0f };
    std::atomic<float> pitchSemis  { 0.0f };
    std::atomic<float> noteOffset  { 0.0f };
    std::atomic<float> sprayMs     { 30.0f };
    std::atomic<float> spread      { 0.4f };
    std::atomic<float> position    { 0.5f };
    std::atomic<float> pitchJitter { 0.0f };
    std::atomic<float> outputGain  { 0.75f };
    std::atomic<float> velocity    { 1.0f };
    std::atomic<float> velocityToAmp { 0.0f };

    // Frozen snapshot: the read window is locked to where freeze engaged.
    int  frozenAnchor = 0;
    bool wasFrozen    = false; // edge-detect for latching the anchor (per-instance)
};

} // namespace gf
