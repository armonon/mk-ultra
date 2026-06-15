#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "GrainFreeze/PitchMatch.h"
#include "GrainFreeze/FormantShifter.h"

namespace gf
{
namespace mix
{

enum class RoutingMode
{
    parallel = 0,
    entropyToPrettifier,
    prettifierToEntropy,
    multiband
};

struct Params
{
    bool pluginOn = true;
    bool entropyOn = true;
    bool prettifierOn = true;
    bool limiterOn = true;
    bool mixEqOn = true;
    bool pitchMatchOn = false;
    bool tempoLockOn = false;
    // Master Pitch Lock: when on, it tunes the entire output and overrides
    // Pitch Match. Mode 0 = Chromatic (nearest semitone), 1 = Scale (nearest
    // note in Key+Scale), 2 = Root (lock to the key's root pitch class).
    bool  pitchLockOn = false;
    int   pitchLockMode = 1;
    int   pitchLockKey = 0;     // 0..11 = C..B
    int   pitchLockScale = 1;   // index into the scale table
    float pitchLockAmount = 1.0f;
    bool  pitchLockFormant = false; // preserve formants (phase-vocoder shifter)
    float dryLevel = 0.0f;
    float entropySend = 1.0f;
    float entropyReturn = 1.0f;
    float prettifierSend = 1.0f;
    float prettifierReturn = 1.0f;
    float outputLevel = 1.0f;
    float chaosBeauty = 0.5f;
    float ceilingDb = -0.1f;
    float eqLow = 0.0f;   // dB, low shelf
    float eqMid = 0.0f;   // dB, mid peak
    float eqHigh = 0.0f;  // dB, high shelf
    float eqLoFi = 0.0f;  // 0..1 band-limit amount
    float width = 1.0f;   // stereo width: 0 = mono, 1 = unchanged, 2 = wide
    RoutingMode routing = RoutingMode::parallel;
};

class MixEngine
{
public:
    static void addParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout);

    void prepare (double sampleRate, int samplesPerBlock, int numChannels);
    void reset();

    void processParallel (juce::AudioBuffer<float>& output,
                          const juce::AudioBuffer<float>& dry,
                          const juce::AudioBuffer<float>& entropyOut,
                          const juce::AudioBuffer<float>& prettifierOut,
                          const Params& params);

    // Final master dynamics, applied after everything (incl. Sample Mode) is
    // summed: a one-knob "glue" bus compressor followed by the ceiling limiter.
    void processMasterDynamics (juce::AudioBuffer<float>& output,
                                float glueAmount, float ceilingDb, bool limiterOn);

    void panic();

private:
    void updateEq (const Params& params);

    juce::dsp::Limiter<float> limiter;
    juce::dsp::Compressor<float> glueComp;        // one-knob master "glue"
    juce::SmoothedValue<float> smoothedGlueMakeup { 1.0f };
    juce::SmoothedValue<float> smoothedOutputGain { 1.0f };
    juce::SmoothedValue<float> smoothedBypass { 1.0f };
    juce::AudioBuffer<float> tmp;

    // Mix EQ: stereo IIR bands (low shelf / mid peak / high shelf) plus a lo-fi
    // low-pass for the band-limit character. Coefficients refreshed per block.
    using StereoFilter = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                        juce::dsp::IIR::Coefficients<float>>;
    StereoFilter eqLowShelf, eqMidPeak, eqHighShelf, eqLoFiLP;
    double sampleRateHz = 44100.0;
    float lastEqLow = 1.0e9f, lastEqMid = 1.0e9f, lastEqHigh = 1.0e9f, lastEqLoFi = 1.0e9f;

    // Pitch Match: detect the dry input's fundamental, snap to the nearest
    // semitone, and pitch-shift the wet (entropy+prettifier) sum to that grid.
    gf::PitchDetector pitchDetector;
    gf::PitchShifter pitchShifterL, pitchShifterR;
    juce::SmoothedValue<float> smoothedPitchRatio { 1.0f };
    juce::AudioBuffer<float> wetSum;

    // Master Pitch Lock: a second detector/shifter pair that tunes the whole
    // output buffer to a key/scale, independent of the wet-only Pitch Match.
    gf::PitchDetector lockDetector;
    gf::PitchShifter lockShifterL, lockShifterR;
    gf::FormantShifter formantShifterL, formantShifterR; // formant-preserving path
    juce::SmoothedValue<float> smoothedLockRatio { 1.0f };
};

} // namespace mix
} // namespace gf
