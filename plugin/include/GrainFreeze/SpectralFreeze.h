#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <vector>
#include <random>

namespace gf
{

// FFT-based spectral freeze. While capturing, it runs a windowed STFT and keeps
// the most recent magnitude spectrum. When frozen, it resynthesizes a sustained
// tone from that held magnitude with per-bin randomized/evolving phase, giving a
// smooth glassy pad rather than a looped grain. Uses overlap-add with a Hann
// window at 4x overlap. Mono-summed analysis, stereo decorrelated output.
class SpectralFreeze
{
public:
    SpectralFreeze();

    void prepare (double sampleRate, int numChannels);
    void reset();

    void setFrozen (bool b)        { frozen.store (b); }
    void setMix    (float m)       { mix.store (juce::jlimit (0.0f, 1.0f, m)); }
    void setShimmer (float s)      { shimmer.store (juce::jlimit (0.0f, 1.0f, s)); } // phase evolution rate

    bool isFrozen() const          { return frozen.load(); }

    // Process in place: blends the spectral-freeze signal into buffer by mix.
    void process (juce::AudioBuffer<float>& buffer);

    static constexpr int kOrder = 11;            // FFT size = 2048
    static constexpr int kFftSize = 1 << kOrder; // 2048
    static constexpr int kHop = kFftSize / 4;    // 75% overlap

private:
    void processFrame();

    juce::dsp::FFT fft { kOrder };

    double sr = 44100.0;
    int    channels = 2;

    std::atomic<bool>  frozen  { false };
    std::atomic<float> mix     { 0.0f };
    std::atomic<float> shimmer { 0.2f };

    // Sliding input/output buffers for overlap-add.
    std::vector<float> inFifo;     // circular window of kFftSize samples
    std::vector<float> outAccum;   // circular overlap-add accumulator
    int   fifoFill = 0;            // current circular write/read index
    int   hopCounter = 0;          // counts samples between frames

    std::vector<float> window;                 // Hann
    std::vector<float> frozenMag;              // held magnitude spectrum (kFftSize/2+1)
    std::vector<float> phase;                  // running phase per bin
    std::vector<float> fftData;                // 2*kFftSize for JUCE realfft

    bool haveFrozen = false;

    std::mt19937 rng { 0xC0FFEE };
    std::uniform_real_distribution<float> dist { -juce::MathConstants<float>::pi,
                                                  juce::MathConstants<float>::pi };
};

} // namespace gf
