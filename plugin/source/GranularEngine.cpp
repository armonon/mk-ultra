#include "GrainFreeze/GranularEngine.h"
#include <cmath>

namespace gf
{

void GranularEngine::prepare (double sampleRate, int /*maxBlockSize*/, int numChannels)
{
    sr       = sampleRate;
    channels = juce::jmax (1, numChannels);

    captureLen = (int) std::ceil (captureSeconds * sr);
    capture.setSize (channels, captureLen);
    capture.clear();
    captureWrite = 0;

    // Precompute a Hann window. Every grain multiplies its output by this curve,
    // which is what prevents clicks at grain boundaries.
    for (int i = 0; i < kWindowPoints; ++i)
    {
        const double phase = (double) i / (double) (kWindowPoints - 1);
        window[(size_t) i] = (float) (0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * phase));
    }

    for (auto& g : grains) g.active = false;
    grainClock = 0.0;
    prepared   = true;
}

void GranularEngine::reset()
{
    capture.clear();
    captureWrite = 0;
    grainClock   = 0.0;
    wasFrozen    = false;
    for (auto& g : grains) g.active = false;
}

void GranularEngine::pushInput (const juce::AudioBuffer<float>& input)
{
    if (! prepared) return;
    if (frozen.load()) return; // hold the buffer while frozen

    const int n = input.getNumSamples();
    const int ch = juce::jmin (channels, input.getNumChannels());

    for (int i = 0; i < n; ++i)
    {
        const int w = (captureWrite + i) % captureLen;
        for (int c = 0; c < channels; ++c)
        {
            const int srcCh = juce::jmin (c, ch - 1);
            capture.setSample (c, w, input.getSample (srcCh, i));
        }
    }
    captureWrite = (captureWrite + n) % captureLen;
}

inline float GranularEngine::readInterpolated (int channel, double pos) const
{
    // Linear interpolation with wraparound into the circular buffer.
    while (pos < 0)          pos += captureLen;
    while (pos >= captureLen) pos -= captureLen;

    const int   i0 = (int) pos;
    const int   i1 = (i0 + 1) % captureLen;
    const float f  = (float) (pos - (double) i0);
    const float a  = capture.getSample (channel, i0);
    const float b  = capture.getSample (channel, i1);
    return a + (b - a) * f;
}

void GranularEngine::spawnGrain()
{
    // Find a free voice.
    Grain* slot = nullptr;
    for (auto& g : grains)
        if (! g.active) { slot = &g; break; }
    if (slot == nullptr) return; // pool exhausted; drop the grain

    const bool  isFrozen = frozen.load();
    const float pos01    = position.load();

    // Anchor: when frozen, read around the locked anchor; otherwise trail the
    // write head so we read the most recent material.
    double anchor = isFrozen
        ? (double) frozenAnchor
        : (double) ((captureWrite - (int) (0.05 * sr) + captureLen) % captureLen);

    // Position offsets a window across the captured material.
    anchor += pos01 * captureLen;

    // Spray randomizes the read offset for texture.
    const double spraySamps = (sprayMs.load() / 1000.0) * sr;
    const double jitter     = (rand01() * 2.0 - 1.0) * spraySamps;

    const float jit    = (rand01() * 2.0f - 1.0f) * pitchJitter.load();
    const float semis  = pitchSemis.load() + noteOffset.load() + jit;
    const double rate   = std::pow (2.0, semis / 12.0);
    const int    lenSm  = juce::jmax (4, (int) ((grainSizeMs.load() / 1000.0) * sr));

    slot->active      = true;
    slot->readPos     = anchor + jitter;
    slot->rate        = rate;
    slot->lengthSamps = lenSm;
    slot->samplesLeft = lenSm;
    slot->amp         = 0.6f + 0.4f * rand01();
    const float vToA  = velocityToAmp.load();
    const float vel   = velocity.load();
    slot->amp        *= (1.0f - vToA) + vToA * vel;

    const float sp = spread.load();
    slot->pan = 0.5f + (rand01() - 0.5f) * sp;
}

void GranularEngine::process (juce::AudioBuffer<float>& output)
{
    output.clear();
    if (! prepared) return;

    // Latch the freeze anchor at the moment freezing engages.
    const bool nowFrozen = frozen.load();
    if (nowFrozen && ! wasFrozen)
        frozenAnchor = (captureWrite - (int) (0.05 * sr) + captureLen) % captureLen;
    wasFrozen = nowFrozen;

    const int   n          = output.getNumSamples();
    const double secsPer   = 1.0 / sr;
    const double spawnRate = juce::jmax (1.0f, density.load());
    const float  gain      = outputGain.load();
    const int    outCh     = output.getNumChannels();

    for (int i = 0; i < n; ++i)
    {
        // Sample-accurate grain spawning: accumulate time, spawn when due.
        grainClock += spawnRate * secsPer;
        while (grainClock >= 1.0)
        {
            spawnGrain();
            grainClock -= 1.0;
        }

        float left = 0.0f, right = 0.0f;

        for (auto& g : grains)
        {
            if (! g.active) continue;

            // Window index from grain progress.
            const float prog = 1.0f - ((float) g.samplesLeft / (float) g.lengthSamps);
            const int   wIdx = juce::jlimit (0, kWindowPoints - 1,
                                             (int) (prog * (kWindowPoints - 1)));
            const float win  = window[(size_t) wIdx] * g.amp;

            const float sL = readInterpolated (0, g.readPos) * win;
            const float sR = readInterpolated (channels > 1 ? 1 : 0, g.readPos) * win;

            // Equal-power-ish pan.
            const float pl = std::cos (g.pan * juce::MathConstants<float>::halfPi);
            const float pr = std::sin (g.pan * juce::MathConstants<float>::halfPi);
            left  += sL * pl;
            right += sR * pr;

            g.readPos += g.rate;
            if (--g.samplesLeft <= 0) g.active = false;
        }

        if (outCh > 0) output.setSample (0, i, left  * gain);
        if (outCh > 1) output.setSample (1, i, right * gain);
    }
}

} // namespace gf
