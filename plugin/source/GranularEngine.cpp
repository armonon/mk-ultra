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
    // 4-point cubic Hermite (Catmull-Rom) interpolation with wraparound.
    //
    // Linear interpolation is cheap but acts as a low-pass whose cutoff depends
    // on the fractional phase -- so pitched-up grains lose air and pitched-down
    // grains alias. Hermite uses the two neighbours on each side to fit a cubic
    // through the sample points, which drops the interpolation error by roughly
    // an order of magnitude in the top octave. This is the single biggest
    // audible quality factor in a granular engine.
    while (pos < 0)           pos += captureLen;
    while (pos >= captureLen) pos -= captureLen;

    const int   i1 = (int) pos;
    const float f  = (float) (pos - (double) i1);

    const int i0 = (i1 - 1 + captureLen) % captureLen;
    const int i2 = (i1 + 1) % captureLen;
    const int i3 = (i1 + 2) % captureLen;

    const auto* d = capture.getReadPointer (channel);
    const float y0 = d[i0], y1 = d[i1], y2 = d[i2], y3 = d[i3];

    // Catmull-Rom coefficients.
    const float c0 = y1;
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);

    return ((c3 * f + c2) * f + c1) * f + c0;
}

void GranularEngine::spawnGrain()
{
    // Find a free voice.
    Grain* slot = nullptr;
    int activeCount = 0;
    const int activeLimit = maxActiveGrains.load();
    for (auto& g : grains)
    {
        if (g.active)
            ++activeCount;
        else if (slot == nullptr)
            slot = &g;
    }
    if (activeCount >= activeLimit) return;
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
    // Polyphonic mode: pick a random held note's offset for this grain so a
    // chord becomes a polyphonic granular cloud. Falls through to noteOffset if
    // no notes are held or poly is off.
    // With MPE enabled the chosen voice also contributes pressure -> amplitude
    // and timbre -> grain-size scaling, so per-note expression actually moves
    // the cloud per voice.
    float noteOff = noteOffset.load();
    float voicePress = 1.0f, voiceTimb = 0.0f;
    if (polyOn.load (std::memory_order_relaxed))
    {
        const int n = activeNoteCount.load (std::memory_order_relaxed);
        if (n > 0)
        {
            const int pick = juce::jlimit (0, n - 1, (int) (rand01() * n));
            noteOff   = activeNotes  [(size_t) pick].load (std::memory_order_relaxed);
            voicePress = voicePressure[(size_t) pick].load (std::memory_order_relaxed);
            voiceTimb  = voiceTimbre  [(size_t) pick].load (std::memory_order_relaxed);
        }
    }
    const float semis  = pitchSemis.load() + noteOff + jit;
    const double rate   = std::pow (2.0, semis / 12.0);
    // Timbre (-1..+1) gives ±35% grain-size shift when MPE is on -- a useful but
    // not extreme range, so users can sweep CC74 without making grains absurd.
    const float sizeScale = mpeOn.load (std::memory_order_relaxed) ? (1.0f + voiceTimb * 0.35f) : 1.0f;
    const int    lenSm  = juce::jmax (4, (int) ((grainSizeMs.load() / 1000.0) * sr * sizeScale));

    slot->active      = true;
    slot->readPos     = anchor + jitter;
    slot->rate        = rate;
    slot->lengthSamps = lenSm;
    slot->samplesLeft = lenSm;
    slot->amp         = 0.6f + 0.4f * rand01();
    const float vToA  = velocityToAmp.load();
    const float vel   = velocity.load();
    slot->amp        *= (1.0f - vToA) + vToA * vel;
    // MPE pressure: scale grain amplitude by [0.3 .. 1.5] across the voice's
    // pressure range so harder presses = louder grains. Off when MPE is off.
    if (mpeOn.load (std::memory_order_relaxed))
        slot->amp *= 0.3f + 1.2f * voicePress;

    const float sp = spread.load();
    slot->pan = juce::jlimit (0.0f, 1.0f, 0.5f + (rand01() - 0.5f) * sp);
    // Resolve the equal-power pan gains ONCE here instead of per sample.
    const float panAngle = slot->pan * juce::MathConstants<float>::halfPi;
    slot->panL = std::cos (panAngle);
    slot->panR = std::sin (panAngle);
    // Latch the window shape so a grain keeps a consistent envelope even if the
    // user sweeps the Shape knob mid-grain.
    slot->skew = grainSkew.load (std::memory_order_relaxed);
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

            // Window index from grain progress, warped by the grain's shape.
            float prog = 1.0f - ((float) g.samplesLeft / (float) g.lengthSamps);
            if (g.skew != 1.0f)
            {
                // Rational bias curve: p / (p + s(1-p)). Maps 0->0 and 1->1, and
                // slides the window peak earlier (s<1, percussive) or later
                // (s>1, swelling). One mul/add/div -- no pow() in the inner loop.
                const float den = prog + g.skew * (1.0f - prog);
                prog = den > 1.0e-6f ? prog / den : prog;
            }
            const int   wIdx = juce::jlimit (0, kWindowPoints - 1,
                                             (int) (prog * (kWindowPoints - 1)));
            const float win  = window[(size_t) wIdx] * g.amp;

            const float sL = readInterpolated (0, g.readPos) * win;
            const float sR = readInterpolated (channels > 1 ? 1 : 0, g.readPos) * win;

            // Equal-power pan, resolved at spawn (see spawnGrain).
            left  += sL * g.panL;
            right += sR * g.panR;

            g.readPos += g.rate;
            if (--g.samplesLeft <= 0) g.active = false;
        }

        if (outCh > 0) output.setSample (0, i, left  * gain);
        if (outCh > 1) output.setSample (1, i, right * gain);
    }
}

} // namespace gf
