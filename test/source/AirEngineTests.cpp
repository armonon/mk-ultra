// Behavioural guards for AirEngine: bit-exact bypass when off, low band
// preserved regardless of what the toys do to the top, top band actually
// affected, energy preserved through the LR split at mix=0, finite at extremes.
#include <catch2/catch_test_macros.hpp>
#include "GrainFreeze/AirEngine.h"

#include <cmath>
#include <random>

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int    kN  = 2048;

    void fillSine (juce::AudioBuffer<float>& b, double f, float amp)
    {
        for (int c = 0; c < b.getNumChannels(); ++c)
            for (int i = 0; i < b.getNumSamples(); ++i)
                b.setSample (c, i, (float) std::sin (2.0 * juce::MathConstants<double>::pi * f * i / kSr) * amp);
    }
    void fillNoise (juce::AudioBuffer<float>& b, float amp, unsigned seed = 7)
    {
        std::mt19937 rng (seed);
        std::uniform_real_distribution<float> d (-1.0f, 1.0f);
        for (int c = 0; c < b.getNumChannels(); ++c)
            for (int i = 0; i < b.getNumSamples(); ++i)
                b.setSample (c, i, d (rng) * amp);
    }
    float rms (const juce::AudioBuffer<float>& b, int from = 0)
    {
        double a = 0.0; int n = 0;
        for (int c = 0; c < b.getNumChannels(); ++c)
            for (int i = from; i < b.getNumSamples(); ++i) { a += (double) b.getSample (c, i) * b.getSample (c, i); ++n; }
        return (float) std::sqrt (a / (double) juce::jmax (1, n));
    }
    bool allFinite (const juce::AudioBuffer<float>& b)
    {
        for (int c = 0; c < b.getNumChannels(); ++c)
            for (int i = 0; i < b.getNumSamples(); ++i)
                if (! std::isfinite (b.getSample (c, i))) return false;
        return true;
    }
    // Everything on, hard.
    gf::AirEngine::Params everything()
    {
        gf::AirEngine::Params p;
        p.on = true; p.crossoverHz = 2500.0f; p.mix = 1.0f;
        p.squelchOn = true; p.squelchMode = 1; p.squelchHz = 5000.0f; p.squelchRes = 1.0f; p.squelchEnv = 1.0f;
        p.exciterOn = true; p.exciterDrive = 1.0f; p.exciterMix = 1.0f;
        p.dynShelfOn = true; p.dynShelfHz = 8000.0f; p.dynShelfAmount = 1.0f; p.dynShelfThreshold = 0.0f;
        p.phaserOn = true; p.phaserRate = 2.0f; p.phaserDepth = 1.0f; p.phaserMix = 1.0f;
        p.delayOn = true; p.delayMs = 40.0f; p.delayFeedback = 0.9f; p.delayMix = 1.0f;
        return p;
    }
}

TEST_CASE ("AirEngine is bit-exact bypass when off", "[air]")
{
    gf::AirEngine a; a.prepare (kSr, 2, kN);
    juce::AudioBuffer<float> buf (2, kN); fillNoise (buf, 0.5f);
    juce::AudioBuffer<float> orig; orig.makeCopyOf (buf, true);
    auto p = everything(); p.on = false;
    a.process (buf, p);
    for (int c = 0; c < 2; ++c) for (int i = 0; i < kN; ++i)
        REQUIRE (buf.getSample (c, i) == orig.getSample (c, i));
}

TEST_CASE ("AirEngine preserves the LOW band no matter what the toys do", "[air]")
{
    // 150 Hz tone is ~4 octaves under a 2.5 kHz LR4 crossover: the top band
    // holds essentially nothing, so hammering it must leave the output alone.
    gf::AirEngine a; a.prepare (kSr, 2, 512);
    juce::AudioBuffer<float> in (2, kN); fillSine (in, 150.0, 0.5f);
    juce::AudioBuffer<float> out; out.makeCopyOf (in, true);
    const auto p = everything();
    for (int off = 0; off < kN; off += 512)
    {
        juce::AudioBuffer<float> blk (out.getArrayOfWritePointers(), 2, off, 512);
        a.process (blk, p);
    }
    // Skip the first block (ring + filter settling), then compare energy.
    const float rIn = rms (in, 512), rOut = rms (out, 512);
    INFO ("low-band RMS in=" << rIn << " out=" << rOut);
    REQUIRE (allFinite (out));
    CHECK (std::abs (rOut - rIn) / rIn < 0.05f);   // <5% -- toys touched only the (empty) top
}

TEST_CASE ("AirEngine actually changes the TOP band", "[air]")
{
    gf::AirEngine a; a.prepare (kSr, 2, 512);
    juce::AudioBuffer<float> in (2, kN); fillSine (in, 8000.0, 0.4f);
    juce::AudioBuffer<float> out; out.makeCopyOf (in, true);
    gf::AirEngine::Params p; p.on = true; p.mix = 1.0f;
    p.exciterOn = true; p.exciterDrive = 1.0f; p.exciterMix = 1.0f;   // hard drive on the top
    for (int off = 0; off < kN; off += 512)
    {
        juce::AudioBuffer<float> blk (out.getArrayOfWritePointers(), 2, off, 512);
        a.process (blk, p);
    }
    REQUIRE (allFinite (out));
    // Sample-level difference must be substantial (not just a phase nudge).
    double diff = 0.0;
    for (int i = 512; i < kN; ++i) diff += std::abs (out.getSample (0, i) - in.getSample (0, i));
    diff /= (kN - 512);
    INFO ("mean |out-in| on 8 kHz tone = " << diff);
    CHECK (diff > 0.02);
}

TEST_CASE ("AirEngine at mix=0 with toys off preserves energy (LR split + ring are transparent)", "[air]")
{
    gf::AirEngine a; a.prepare (kSr, 2, 512);
    juce::AudioBuffer<float> in (2, kN); fillNoise (in, 0.5f, 11);
    juce::AudioBuffer<float> out; out.makeCopyOf (in, true);
    gf::AirEngine::Params p; p.on = true; p.mix = 0.0f;   // dry top + clean low
    for (int off = 0; off < kN; off += 512)
    {
        juce::AudioBuffer<float> blk (out.getArrayOfWritePointers(), 2, off, 512);
        a.process (blk, p);
    }
    // LR4 low+high sums to an allpass: phase shifts, magnitude flat -> RMS equal.
    const float rIn = rms (in, 512), rOut = rms (out, 512);
    INFO ("mix=0 RMS in=" << rIn << " out=" << rOut);
    REQUIRE (allFinite (out));
    CHECK (std::abs (rOut - rIn) / rIn < 0.03f);
}

TEST_CASE ("AirEngine stays finite with everything cranked for many blocks", "[air]")
{
    gf::AirEngine a; a.prepare (kSr, 2, 256);
    const auto p = everything();
    for (int blk = 0; blk < 200; ++blk)
    {
        juce::AudioBuffer<float> b (2, 256); fillNoise (b, 0.9f, (unsigned) blk);
        a.process (b, p);
        REQUIRE (allFinite (b));
        // Squelch at res=1 + delay fb=0.9 + exciter must not run away.
        REQUIRE (rms (b) < 4.0f);
    }
}
