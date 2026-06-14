#include "GrainFreeze/SpectralFreeze.h"
#include <algorithm>
#include <cmath>

namespace gf
{

SpectralFreeze::SpectralFreeze() = default;

void SpectralFreeze::prepare (double sampleRate, int numChannels)
{
    sr       = sampleRate;
    channels = juce::jmax (1, numChannels);

    inFifo.assign (kFftSize, 0.0f);
    outAccum.assign (kFftSize, 0.0f);
    fftData.assign (2 * kFftSize, 0.0f);

    const int bins = kFftSize / 2 + 1;
    frozenMag.assign (bins, 0.0f);
    phase.assign (bins, 0.0f);

    window.assign (kFftSize, 0.0f);
    for (int i = 0; i < kFftSize; ++i)
        window[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi
                                                     * (float) i / (float) (kFftSize - 1));

    fifoFill   = 0;
    hopCounter = 0;
    haveFrozen = false;
}

void SpectralFreeze::reset()
{
    std::fill (inFifo.begin(), inFifo.end(), 0.0f);
    std::fill (outAccum.begin(), outAccum.end(), 0.0f);
    fifoFill   = 0;
    hopCounter = 0;
    haveFrozen = false;
}

void SpectralFreeze::processFrame()
{
    const int bins = kFftSize / 2 + 1;

    // Linearize the circular input window: oldest sample is at fifoFill.
    for (int i = 0; i < kFftSize; ++i)
    {
        const int src = (fifoFill + i) % kFftSize;
        fftData[(size_t) i] = inFifo[(size_t) src] * window[(size_t) i];
    }
    std::fill (fftData.begin() + kFftSize, fftData.end(), 0.0f);

    if (! frozen.load())
    {
        // ANALYSIS: real FFT, store magnitude spectrum.
        fft.performRealOnlyForwardTransform (fftData.data());
        for (int b = 0; b < bins; ++b)
        {
            const float re = fftData[(size_t) (2 * b)];
            const float im = fftData[(size_t) (2 * b + 1)];
            frozenMag[(size_t) b] = std::sqrt (re * re + im * im);
        }
        haveFrozen = true;
        return; // not frozen: dry path carries the sound, no output written
    }

    if (! haveFrozen) return;

    // RESYNTHESIS: rebuild spectrum from held magnitude + evolving phase.
    const float shim = shimmer.load();
    for (int b = 0; b < bins; ++b)
    {
        if (shim > 0.0f && (b % 2 == 0))
            phase[(size_t) b] += dist (rng) * shim * 0.05f;

        const float mag = frozenMag[(size_t) b];
        fftData[(size_t) (2 * b)]     = mag * std::cos (phase[(size_t) b]);
        fftData[(size_t) (2 * b + 1)] = mag * std::sin (phase[(size_t) b]);
    }

    fft.performRealOnlyInverseTransform (fftData.data());

    // Window again and overlap-add circularly, aligned to the same window start.
    for (int i = 0; i < kFftSize; ++i)
    {
        const int dst = (fifoFill + i) % kFftSize;
        outAccum[(size_t) dst] += fftData[(size_t) i] * window[(size_t) i];
    }
}

void SpectralFreeze::process (juce::AudioBuffer<float>& buffer)
{
    const float m = mix.load();
    if (m <= 0.001f && ! frozen.load()) return;

    const int n = buffer.getNumSamples();
    const float oaGain = 2.0f / 3.0f; // Hann analysis+synthesis at 75% overlap

    for (int i = 0; i < n; ++i)
    {
        // Mono sum into the sliding analysis window at the current write index.
        float mono = 0.0f;
        for (int c = 0; c < buffer.getNumChannels(); ++c)
            mono += buffer.getSample (c, i);
        mono /= (float) juce::jmax (1, buffer.getNumChannels());

        inFifo[(size_t) fifoFill] = mono;
        const float wet = outAccum[(size_t) fifoFill] * oaGain;
        outAccum[(size_t) fifoFill] = 0.0f; // consumed

        ++fifoFill;
        if (fifoFill >= kFftSize)
            fifoFill = 0;

        // Every hop samples, run an analysis/synthesis frame over the window.
        if (++hopCounter >= kHop)
        {
            hopCounter = 0;
            processFrame();
        }

        if (frozen.load())
        {
            for (int c = 0; c < buffer.getNumChannels(); ++c)
            {
                const float dry = buffer.getSample (c, i);
                const float w = (c == 1) ? -wet : wet; // slight stereo decorrelation
                buffer.setSample (c, i, dry * (1.0f - m) + w * m);
            }
        }
    }
}

} // namespace gf
