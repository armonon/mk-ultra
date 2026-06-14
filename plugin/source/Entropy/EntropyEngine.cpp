#include "GrainFreeze/Entropy/EntropyEngine.h"

namespace
{
namespace pid
{
constexpr auto frozen      = "frozen";
constexpr auto grainSize   = "grainSize";
constexpr auto density     = "density";
constexpr auto pitch       = "pitch";
constexpr auto spray       = "spray";
constexpr auto spread      = "spread";
constexpr auto position    = "position";
constexpr auto pitchJitter = "pitchJitter";
constexpr auto reverbMix   = "reverbMix";
constexpr auto output      = "output";
}
}

namespace gf
{
namespace entropy
{

void EntropyEngine::addParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout)
{
    using namespace juce;
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { pid::frozen, 1 },      "Freeze", false));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::grainSize, 1 },   "Grain Size", NormalisableRange<float> (10.0f, 400.0f, 1.0f), 120.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::density, 1 },     "Density", NormalisableRange<float> (2.0f, 80.0f, 1.0f), 28.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::pitch, 1 },       "Pitch", NormalisableRange<float> (-24.0f, 24.0f, 1.0f), 0.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::spray, 1 },       "Spray", NormalisableRange<float> (0.0f, 300.0f, 1.0f), 30.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::spread, 1 },      "Stereo Spread", NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.4f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::position, 1 },    "Position", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::pitchJitter, 1 }, "Pitch Jitter", NormalisableRange<float> (0.0f, 12.0f, 0.1f), 0.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::reverbMix, 1 },   "Reverb Mix", NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.25f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { pid::output, 1 },      "Output", NormalisableRange<float> (0.0f, 1.5f, 0.01f), 0.75f));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "satOn", 1 }, "Saturation On", true));
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "satType", 1 }, "Saturation Type",
        StringArray { "Tube", "Tape", "Hard" }, 0));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "satDrive", 1 }, "Saturation Drive",
        NormalisableRange<float> (1.0f, 24.0f, 0.01f, 0.5f), 1.0f));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "satMix", 1 }, "Saturation Mix",
        NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { "specFreeze", 1 }, "Spectral Freeze", false));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "specMix", 1 }, "Spectral Mix",
        NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));
    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "specShimmer", 1 }, "Spectral Shimmer",
        NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.2f));

    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "midiEnable", 1 }, "MIDI Enable", false));
    layout.add (std::make_unique<AudioParameterInt>   (ParameterID { "midiRoot", 1 },   "MIDI Root", 0, 127, 60));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "glideTime", 1 },  "Glide", NormalisableRange<float> (0.0f, 500.0f, 1.0f), 0.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "velToAmp", 1 },   "Vel->Amp", NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));
}

void EntropyEngine::prepare (double sampleRate, int samplesPerBlock, int numChannels)
{
    granular.prepare (sampleRate, samplesPerBlock, numChannels);

    reverb.reset();
    reverbParams.roomSize = 0.7f;
    reverbParams.damping = 0.4f;
    reverbParams.width = 1.0f;
    reverbParams.wetLevel = 1.0f;
    reverbParams.dryLevel = 0.0f;
    reverb.setParameters (reverbParams);

    wetBuffer.setSize (numChannels, samplesPerBlock);
    saturator.prepare (sampleRate, numChannels);
    spectral.prepare (sampleRate, numChannels);
}

void EntropyEngine::reset()
{
    granular.reset();
    spectral.reset();
    reverb.reset();
}

void EntropyEngine::pushInput (const juce::AudioBuffer<float>& input)
{
    granular.pushInput (input);
}

void EntropyEngine::process (juce::AudioBuffer<float>& buffer, const Params& params)
{
    granular.setFrozen (params.frozen);
    granular.setGrainSizeMs (params.grainSize);
    granular.setDensity (params.density);
    granular.setPitchSemis (params.pitch);
    granular.setNoteOffsetSemis (params.noteOffset);
    granular.setSprayMs (params.spray);
    granular.setSpread (params.spread);
    granular.setPosition (params.position);
    granular.setPitchJitter (params.pitchJitter);
    granular.setOutputGain (params.output);
    granular.setVelocity (params.velocity);
    granular.setVelocityToAmp (params.velToAmp);
    granular.process (buffer);

    spectral.setFrozen (params.spectralFreeze);
    spectral.setMix (params.spectralMix);
    spectral.setShimmer (params.spectralShimmer);
    spectral.process (buffer);

    if (params.reverbMix > 0.001f)
    {
        const int ch = buffer.getNumChannels();
        const int n = buffer.getNumSamples();
        wetBuffer.setSize (ch, n, false, false, true);
        for (int c = 0; c < ch; ++c)
            wetBuffer.copyFrom (c, 0, buffer, c, 0, n);

        juce::dsp::AudioBlock<float> block (wetBuffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        reverb.process (ctx);

        for (int c = 0; c < ch; ++c)
        {
            buffer.applyGain (c, 0, n, 1.0f - params.reverbMix);
            buffer.addFrom (c, 0, wetBuffer, c, 0, n, params.reverbMix);
        }
    }

    if (params.satOn)
    {
        saturator.setType (params.satType);
        saturator.setDrive (params.satDrive);
        saturator.setMix (params.satMix);
        saturator.process (buffer);
    }
}

} // namespace entropy
} // namespace gf
