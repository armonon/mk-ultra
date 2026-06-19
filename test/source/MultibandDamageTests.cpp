// Behavioral guards for MultibandDamage: clean bypass when both bands are off,
// near-perfect reconstruction when both bands are unity (LR4 sums to flat
// magnitude), and verifiable low-band preservation when only the high band is
// driven hard ("destroy only the highs").
#include <catch2/catch_test_macros.hpp>
#include "GrainFreeze/TransformRack.h"

#include <cmath>
#include <vector>

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

    float rms (const juce::AudioBuffer<float>& b)
    {
        double a = 0.0;
        const int n = b.getNumChannels() * b.getNumSamples();
        for (int c = 0; c < b.getNumChannels(); ++c)
            for (int i = 0; i < b.getNumSamples(); ++i)
                a += (double) b.getSample (c, i) * b.getSample (c, i);
        return (float) std::sqrt (a / (double) juce::jmax (1, n));
    }
}

TEST_CASE ("MultibandDamage bypasses cleanly when both mixes are zero", "[multiband]")
{
    gf::MultibandDamage mb; mb.prepare (kSr, 1, kN);
    juce::AudioBuffer<float> buf (1, kN); fillSine (buf, 2000.0, 0.5f);
    juce::AudioBuffer<float> orig; orig.makeCopyOf (buf, true);

    gf::DamageParams zero; // mix defaults to 0
    mb.process (buf, zero, zero);

    for (int i = 0; i < kN; ++i)
        REQUIRE (buf.getSample (0, i) == orig.getSample (0, i));   // bit-exact bypass
}

TEST_CASE ("MultibandDamage clean-pass matches single-band DamageEngine energy", "[multiband]")
{
    // Both DamageEngines have the same oversampler latency + level-compensation
    // ramp-up, so the split shouldn't add audible loss vs. a single-band engine
    // at the same settings. If the split itself were broken (phase-flipped band,
    // wrong sum, etc.), multiband RMS would diverge from the single-band reference.
    juce::AudioBuffer<float> bufA (1, kN); fillSine (bufA, 2000.0, 0.5f);
    juce::AudioBuffer<float> bufB; bufB.makeCopyOf (bufA, true);

    gf::DamageParams pass;
    pass.amount = 0.0f; pass.bits = 16.0f; pass.rate = 1.0f; pass.tone = 1.0f; pass.mix = 1.0f;

    gf::DamageEngine single; single.prepare (kSr, 1, kN);
    single.process (bufA, pass);

    gf::MultibandDamage mb; mb.prepare (kSr, 1, kN);
    mb.setCrossover (800.0f);
    mb.process (bufB, pass, pass);

    const float rSingle = rms (bufA);
    const float rMulti  = rms (bufB);
    INFO ("RMS single-band = " << rSingle << ", multiband = " << rMulti);
    REQUIRE (rSingle > 0.0f);
    CHECK (std::abs (rMulti - rSingle) / rSingle < 0.10f);
}

TEST_CASE ("MultibandDamage isolates bands: high drive doesn't change the low energy", "[multiband]")
{
    // Pure low-frequency tone (180 Hz). Driving the HIGH band hard at a 1.5 kHz
    // split should not move the low-band RMS in any meaningful way.
    gf::MultibandDamage mb; mb.prepare (kSr, 1, kN);
    mb.setCrossover (1500.0f);

    juce::AudioBuffer<float> base (1, kN); fillSine (base, 180.0, 0.5f);
    juce::AudioBuffer<float> a; a.makeCopyOf (base, true);
    juce::AudioBuffer<float> b; b.makeCopyOf (base, true);

    gf::DamageParams off;  off.amount = 0.0f; off.mix = 0.0f; off.bits = 16.0f; off.rate = 1.0f; off.tone = 1.0f;
    gf::DamageParams hard; hard.amount = 1.0f; hard.mix = 1.0f; hard.clip = 2 /*Hard*/; hard.bits = 16.0f; hard.rate = 1.0f; hard.tone = 1.0f;

    mb.process (a, off,  off);    // both bypassed -> equals input
    mb.process (b, off,  hard);   // only high band hammered

    const float rA = rms (a);
    const float rB = rms (b);
    INFO ("RMS bypass = " << rA << ", RMS high-only-damage = " << rB);
    REQUIRE (rA > 0.0f);
    CHECK (std::abs (rB - rA) / rA < 0.10f);   // <10% drift on a pure low tone
}
