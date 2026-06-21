#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "GrainFreeze/GranularEngine.h"
#include "GrainFreeze/Saturator.h"
#include "GrainFreeze/SpectralFreeze.h"

namespace gf
{
namespace entropy
{

struct Params
{
    bool frozen = false;
    float grainSize = 120.0f;
    float density = 28.0f;
    float pitch = 0.0f;
    float noteOffset = 0.0f;
    float spray = 30.0f;
    float spread = 0.4f;
    float position = 0.5f;
    float pitchJitter = 0.0f;
    float output = 0.75f;
    int maxGrains = 32;
    float velocity = 1.0f;
    float velToAmp = 0.0f;

    bool spectralFreeze = false;
    float spectralMix = 0.0f;
    float spectralShimmer = 0.2f;

    float reverbMix = 0.25f;
    bool satOn = true;
    int satType = 0;
    float satDrive = 1.0f;
    float satMix = 0.0f;
};

class EntropyEngine
{
public:
    static void addParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout);

    void prepare (double sampleRate, int samplesPerBlock, int numChannels);
    void reset();
    void pushInput (const juce::AudioBuffer<float>& input);
    void process (juce::AudioBuffer<float>& buffer, const Params& params);

    // Pass-throughs to the underlying GranularEngine for polyphonic granular mode.
    void setPolyOn      (bool b)                       { granular.setPolyOn (b); }
    void setActiveNotes (const float* notes, int n)    { granular.setActiveNotes (notes, n); }
    void setMpeOn       (bool b)                       { granular.setMpeOn (b); }
    void setActiveVoices (const gf::GranularEngine::VoicePush* v, int n) { granular.setActiveVoices (v, n); }
    int  copyGrainSnapshot (gf::GranularEngine::GrainSnapshot* out, int maxOut) const
    { return granular.copyGrainSnapshot (out, maxOut); }

private:
    gf::GranularEngine granular;
    gf::SpectralFreeze spectral;
    gf::Saturator saturator;
    juce::dsp::Reverb reverb;
    juce::dsp::Reverb::Parameters reverbParams;
    juce::AudioBuffer<float> wetBuffer;
};

} // namespace entropy
} // namespace gf
