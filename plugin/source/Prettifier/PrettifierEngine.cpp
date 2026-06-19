#include "GrainFreeze/Prettifier/PrettifierEngine.h"

#include <algorithm>
#include <cmath>

namespace gf
{
namespace pretty
{

void PrettifierEngine::addParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout)
{
    using namespace juce;
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "prettifierEnabled", 1 }, "Beauty & Space Enabled", true));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "prettifierInTrim", 1 }, "Beauty & Space In", NormalisableRange<float> (0.0f, 2.0f, 0.001f), 1.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "prettifierOutTrim", 1 }, "Beauty & Space Out", NormalisableRange<float> (0.0f, 2.0f, 0.001f), 1.0f));

    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "echoOn", 1 }, "Echo On", true));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "echoTimeMs", 1 }, "Echo Time", NormalisableRange<float> (1.0f, 8000.0f, 1.0f), 280.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "echoFeedback", 1 }, "Echo Feedback", NormalisableRange<float> (0.0f, 0.98f, 0.001f), 0.35f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "echoMix", 1 }, "Echo Mix", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.25f));

    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "prettyReverbOn", 1 }, "Reverb On", true));
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "reverbOn", 1 }, "Reverb On", true));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "prettyReverbSize", 1 }, "Reverb Size", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.65f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "prettyReverbDamping", 1 }, "Reverb Damping", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.4f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "prettyReverbMix", 1 }, "Reverb Mix", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.2f));

    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "chorusOn", 1 }, "Chorus On", true));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "chorusRate", 1 }, "Chorus Rate", NormalisableRange<float> (0.01f, 10.0f, 0.001f), 0.35f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "chorusDepth", 1 }, "Chorus Depth", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.35f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "chorusMix", 1 }, "Chorus Mix", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.2f));

    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "beautyOn", 1 }, "Beauty On", true));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "beautyAmount", 1 }, "More Expensive", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.25f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "beautyAir", 1 }, "Beauty Air", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.3f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "beautyWarmth", 1 }, "Beauty Warmth", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.35f));

    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "polishOn", 1 }, "Polish On", true));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "polishWidth", 1 }, "Polish Width", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.2f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "polishAir", 1 }, "Polish Air", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.2f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "polishWarmth", 1 }, "Polish Warmth", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.2f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "polishHarshnessTame", 1 }, "Harshness Tame", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.1f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "polishMix", 1 }, "Polish Mix", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));

    // Bit-depth crusher: 16 bits = transparent, lower = lo-fi crush.
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "crushOn", 1 }, "Crush On", true));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "crushBits", 1 }, "Bit Crush", NormalisableRange<float> (1.0f, 24.0f, 0.01f), 16.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "crushMix", 1 }, "Crush Mix", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));

    // DNA controls.
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "dnaCharacter", 1 }, "DNA Character", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "dnaAge", 1 }, "DNA Age", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.2f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "dnaWarmth", 1 }, "DNA Warmth", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "dnaWidth", 1 }, "DNA Width", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "dnaRandomness", 1 }, "DNA Randomness", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "dnaAnalog", 1 }, "DNA Analog", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "dnaDigital", 1 }, "DNA Digital", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "dnaSmoothness", 1 }, "DNA Smoothness", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "dnaMotion", 1 }, "DNA Motion", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "dnaShine", 1 }, "DNA Shine", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));

    // Movement + shimmer machines. Phaser/Flanger are implemented; Harmony/Dream/
    // Angel are scaffolded for the shimmer batch. The five redundant placeholders
    // (Texture, Bitcrush, Sample Rate, Space, Spectral) were cut as they duplicate
    // existing machines (granular, Damage lo-fi, Grain Space, the Spectral machine).
    static constexpr const char* machineIds[] = {
        "phaser", "flanger", "harmony", "dream", "angel"
    };
    static constexpr const char* machineNames[] = {
        "Phaser", "Flanger", "Harmony", "Dream", "Angel"
    };
    for (size_t i = 0; i < std::size (machineIds); ++i)
    {
        juce::String onId = juce::String (machineIds[i]) + "On";
        juce::String mixId = juce::String (machineIds[i]) + "Mix";
        layout.add (std::make_unique<AudioParameterBool> (juce::ParameterID { onId, 1 }, juce::String (machineNames[i]) + " On", false));
        layout.add (std::make_unique<AudioParameterFloat> (juce::ParameterID { mixId, 1 }, juce::String (machineNames[i]) + " Mix",
                                                           juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));
    }
}

void PrettifierEngine::prepare (double sampleRate, int samplesPerBlock, int numChannels)
{
    sr = sampleRate;
    maxDelaySamples = (int) std::ceil (sampleRate * 8.2);
    delayLine.setSize (numChannels, maxDelaySamples);
    delayLine.clear();
    delayWrite = 0;

    reverb.reset();
    reverbParams.roomSize = 0.65f;
    reverbParams.damping = 0.4f;
    reverbParams.width = 1.0f;
    reverbParams.wetLevel = 1.0f;
    reverbParams.dryLevel = 0.0f;
    reverb.setParameters (reverbParams);
    wetBuffer.setSize (numChannels, samplesPerBlock);

    beautyOsChannels = juce::jlimit (1, 2, juce::jmax (1, numChannels));
    beautyMaxBlock   = juce::jmax (1, samplesPerBlock);
    beautyOs = std::make_unique<juce::dsp::Oversampling<float>> (
                   (size_t) beautyOsChannels, 2,
                   juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
    beautyOs->initProcessing ((size_t) beautyMaxBlock);
    beautyOs->reset();

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) juce::jmax (1, samplesPerBlock),
                                  (juce::uint32) juce::jmax (1, numChannels) };
    phaser.prepare (spec);
    phaser.setRate (0.4f);
    phaser.setDepth (0.55f);
    phaser.setCentreFrequency (520.0f);
    phaser.setFeedback (0.45f);

    flanger.prepare (spec);
    flanger.setRate (0.28f);
    flanger.setDepth (0.6f);
    flanger.setCentreDelay (3.5f);   // short -> flanger character (vs lush chorus)
    flanger.setFeedback (0.55f);
}

void PrettifierEngine::reset()
{
    delayLine.clear();
    delayWrite = 0;
    reverb.reset();
    chorusPhase = { 0.0, 0.0 };
    phaser.reset();
    flanger.reset();
}

void PrettifierEngine::process (juce::AudioBuffer<float>& buffer, const Params& params, double hostBpm, bool tempoLock)
{
    if (! params.enabled)
        return;

    const int channels = buffer.getNumChannels();
    const int samples = buffer.getNumSamples();
    for (int c = 0; c < channels; ++c)
        buffer.applyGain (c, 0, samples, params.inputTrim);

    // Echo machine.
    if (params.echoOn)
    {
        const float beatMs = hostBpm > 0.0 ? (float) (60000.0 / hostBpm) : 500.0f;
        const float timeMs = tempoLock ? beatMs * 0.5f : params.echoTimeMs;
        const int delaySamples = juce::jlimit (1, maxDelaySamples - 1, (int) std::round (timeMs * 0.001f * (float) sr));
        for (int i = 0; i < samples; ++i)
        {
            const int read = (delayWrite - delaySamples + maxDelaySamples) % maxDelaySamples;
            for (int c = 0; c < channels; ++c)
            {
                const int src = juce::jmin (c, delayLine.getNumChannels() - 1);
                const float x = buffer.getSample (c, i);
                const float delayed = delayLine.getSample (src, read);
                const float y = x + delayed * params.echoMix;
                buffer.setSample (c, i, y);
                delayLine.setSample (src, delayWrite, x + delayed * params.echoFeedback);
            }
            delayWrite = (delayWrite + 1) % maxDelaySamples;
        }
    }

    // Reverb machine.
    if (params.reverbOn && params.reverbMix > 0.001f)
    {
        wetBuffer.makeCopyOf (buffer, true);
        reverbParams.roomSize = params.reverbSize;
        reverbParams.damping = params.reverbDamping;
        reverb.setParameters (reverbParams);
        juce::dsp::AudioBlock<float> block (wetBuffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        reverb.process (ctx);
        for (int c = 0; c < channels; ++c)
        {
            buffer.applyGain (c, 0, samples, 1.0f - params.reverbMix);
            buffer.addFrom (c, 0, wetBuffer, c, 0, samples, params.reverbMix);
        }
    }

    // Chorus machine (micro delay modulation).
    if (params.chorusOn && params.chorusMix > 0.001f)
    {
        for (int c = 0; c < channels; ++c)
        {
            const double phaseInc = (2.0 * juce::MathConstants<double>::pi * params.chorusRate) / sr;
            double phase = chorusPhase[(size_t) juce::jmin (c, 1)];
            for (int i = 0; i < samples; ++i)
            {
                const float modMs = 4.0f + 8.0f * params.chorusDepth * (0.5f + 0.5f * (float) std::sin (phase));
                const int modSamples = juce::jlimit (1, maxDelaySamples - 1, (int) std::round (modMs * 0.001f * (float) sr));
                const int read = (delayWrite - modSamples + maxDelaySamples) % maxDelaySamples;
                const float delayed = delayLine.getSample (juce::jmin (c, delayLine.getNumChannels() - 1), read);
                const float x = buffer.getSample (c, i);
                buffer.setSample (c, i, x * (1.0f - params.chorusMix) + delayed * params.chorusMix);
                phase += phaseInc;
            }
            chorusPhase[(size_t) juce::jmin (c, 1)] = phase;
        }
    }

    // Phaser machine (sweeping allpass notches).
    if (params.phaserOn && params.phaserMix > 0.001f)
    {
        phaser.setMix (params.phaserMix);
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        phaser.process (ctx);
    }

    // Flanger machine (short modulated delay + feedback).
    if (params.flangerOn && params.flangerMix > 0.001f)
    {
        flanger.setMix (params.flangerMix);
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        flanger.process (ctx);
    }

    // Beauty machine: mild saturating tilt + air lift.
    if (params.beautyOn)
    {
        const float amt   = params.beautyAmount;
        const float lift  = 1.0f + params.beautyAir * 0.25f;
        const float warm  = 1.0f - params.beautyWarmth * 0.2f;
        const float drive = 1.0f + amt * 2.2f;

        if (beautyOs != nullptr && channels == beautyOsChannels && samples <= beautyMaxBlock)
        {
            // Oversample the tanh 4x so its harmonics don't fold back as aliasing;
            // lift/warm are linear gains and stay at base rate after downsampling.
            juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), (size_t) channels, (size_t) samples);
            auto up = beautyOs->processSamplesUp (block);
            const int upN = (int) up.getNumSamples();
            for (int c = 0; c < channels; ++c)
            {
                auto* u = up.getChannelPointer ((size_t) c);
                for (int i = 0; i < upN; ++i)
                    u[i] = std::tanh (u[i] * drive);
            }
            beautyOs->processSamplesDown (block);
            for (int c = 0; c < channels; ++c)
            {
                auto* d = buffer.getWritePointer (c);
                for (int i = 0; i < samples; ++i)
                    d[i] *= lift * warm;
            }
        }
        else
        {
            for (int c = 0; c < channels; ++c)
                for (int i = 0; i < samples; ++i)
                {
                    const float v = std::tanh (buffer.getSample (c, i) * drive);
                    buffer.setSample (c, i, v * lift * warm);
                }
        }
    }

    // Width / tone polish.
    if (params.polishOn && channels > 1)
    {
        const float width = juce::jlimit (0.0f, 1.5f, 1.0f + params.width * 0.8f + (params.dnaWidth - 0.5f) * 0.4f);
        const float tame = juce::jlimit (0.0f, 1.0f, params.harshnessTame);
        const float mix = juce::jlimit (0.0f, 1.0f, params.polishMix);
        for (int i = 0; i < samples; ++i)
        {
            const float l = buffer.getSample (0, i);
            const float r = buffer.getSample (1, i);
            const float m = 0.5f * (l + r);
            float s = 0.5f * (l - r) * width;
            s *= (1.0f - tame * 0.2f);
            const float wetL = m + s;
            const float wetR = m - s;
            buffer.setSample (0, i, l * (1.0f - mix) + wetL * mix);
            buffer.setSample (1, i, r * (1.0f - mix) + wetR * mix);
        }
    }

    // Bit-depth crusher: quantize to 2^bits levels for a lo-fi/digital edge.
    if (params.crushOn && params.crushBits < 15.99f && params.crushMix > 0.001f)
    {
        const float bits   = juce::jlimit (1.0f, 24.0f, params.crushBits);
        const float levels = std::pow (2.0f, bits);
        const float step   = 2.0f / (levels - 1.0f);   // signal assumed in [-1, 1]
        const float mix    = juce::jlimit (0.0f, 1.0f, params.crushMix);
        for (int c = 0; c < channels; ++c)
        {
            auto* data = buffer.getWritePointer (c);
            for (int i = 0; i < samples; ++i)
            {
                const float x = data[i];
                const float q = std::round (x / step) * step;
                data[i] = x * (1.0f - mix) + q * mix;
            }
        }
    }

    for (int c = 0; c < channels; ++c)
        buffer.applyGain (c, 0, samples, params.outputTrim);
}

void PrettifierEngine::panic()
{
    delayLine.clear();
    delayWrite = 0;
    reverb.reset();
}

} // namespace pretty
} // namespace gf
