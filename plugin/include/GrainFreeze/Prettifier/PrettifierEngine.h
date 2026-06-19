#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>

namespace gf
{
namespace pretty
{

struct Params
{
    bool enabled = true;
    float inputTrim = 1.0f;
    float outputTrim = 1.0f;

    // MVP machines
    bool echoOn = true;
    float echoTimeMs = 280.0f;
    float echoFeedback = 0.35f;
    float echoMix = 0.25f;

    bool reverbOn = true;
    float reverbSize = 0.65f;
    float reverbDamping = 0.4f;
    float reverbMix = 0.20f;

    bool chorusOn = true;
    float chorusRate = 0.35f;
    float chorusDepth = 0.35f;
    float chorusMix = 0.2f;

    bool beautyOn = true;
    float beautyAmount = 0.25f;
    float beautyAir = 0.3f;
    float beautyWarmth = 0.35f;

    bool polishOn = true;
    float width = 0.2f;
    float air = 0.2f;
    float warmth = 0.2f;
    float harshnessTame = 0.1f;
    float polishMix = 1.0f;

    // Bit-depth crusher (lo-fi). bits = 16 is transparent, lower = crushed.
    bool crushOn = true;
    float crushBits = 16.0f;
    float crushMix = 1.0f;

    // Movement machines (juce::dsp).
    bool  phaserOn  = false;
    float phaserMix = 0.3f;
    bool  flangerOn  = false;
    float flangerMix = 0.3f;

    // DNA controls
    float dnaCharacter = 0.5f;
    float dnaAge = 0.2f;
    float dnaWarmth = 0.5f;
    float dnaWidth = 0.5f;
    float dnaRandomness = 0.0f;
    float dnaAnalog = 0.5f;
    float dnaDigital = 0.5f;
    float dnaSmoothness = 0.5f;
    float dnaMotion = 0.5f;
    float dnaShine = 0.5f;
};

class PrettifierEngine
{
public:
    static void addParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout);
    void prepare (double sampleRate, int samplesPerBlock, int numChannels);
    void reset();
    void process (juce::AudioBuffer<float>& buffer, const Params& params, double hostBpm, bool tempoLock);
    void panic();

private:
    double sr = 44100.0;
    int maxDelaySamples = 1;
    juce::AudioBuffer<float> delayLine;
    int delayWrite = 0;
    juce::dsp::Reverb reverb;
    juce::dsp::Reverb::Parameters reverbParams;
    juce::AudioBuffer<float> wetBuffer;
    std::array<double, 2> chorusPhase { 0.0, 0.0 };

    // Beauty-machine saturation is run oversampled so its tanh drive doesn't
    // alias at high amount. In-place + full-wet, so no dry compensation needed.
    std::unique_ptr<juce::dsp::Oversampling<float>> beautyOs;
    int beautyOsChannels = 0;
    int beautyMaxBlock = 0;

    // Movement machines. The flanger is a juce chorus tuned with a short centre
    // delay + feedback so it sweeps like a flanger rather than a lush chorus.
    juce::dsp::Phaser<float> phaser;
    juce::dsp::Chorus<float> flanger;
};

} // namespace pretty
} // namespace gf
