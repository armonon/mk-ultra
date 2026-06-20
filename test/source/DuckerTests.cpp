// Behavioral guards for SidechainDucker: clean bypass at amount=0, real
// attenuation when a hot trigger is driving a steady target, no NaNs at extremes.
#include <catch2/catch_test_macros.hpp>
#include "GrainFreeze/TransformRack.h"

#include <cmath>

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
        for (int c = 0; c < b.getNumChannels(); ++c)
            for (int i = 0; i < b.getNumSamples(); ++i)
                a += (double) b.getSample (c, i) * b.getSample (c, i);
        return (float) std::sqrt (a / (double) juce::jmax (1, b.getNumChannels() * b.getNumSamples()));
    }
}

TEST_CASE ("Ducker is bit-exact bypass at amount=0", "[ducker]")
{
    gf::SidechainDucker d; d.prepare (kSr, 1, kN);
    juce::AudioBuffer<float> target (1, kN); fillSine (target, 500.0, 0.5f);
    juce::AudioBuffer<float> orig; orig.makeCopyOf (target, true);
    juce::AudioBuffer<float> trigger (1, kN); fillSine (trigger, 200.0, 0.9f);
    d.process (target, trigger, 0.0f, 0.0f);
    for (int i = 0; i < kN; ++i)
        REQUIRE (target.getSample (0, i) == orig.getSample (0, i));
}

TEST_CASE ("Ducker reduces target RMS when trigger is hot", "[ducker]")
{
    gf::SidechainDucker d; d.prepare (kSr, 1, kN);
    d.setAttackMs (1.0f); d.setReleaseMs (50.0f);

    juce::AudioBuffer<float> target (1, kN);  fillSine (target,  1000.0, 0.5f);
    juce::AudioBuffer<float> trigger (1, kN); fillSine (trigger,  200.0, 0.9f);
    juce::AudioBuffer<float> orig; orig.makeCopyOf (target, true);

    d.process (target, trigger, 1.0f /*full duck*/, 0.0f /*no threshold*/);

    const float rBefore = rms (orig);
    const float rAfter  = rms (target);
    INFO ("RMS before = " << rBefore << ", after = " << rAfter);
    REQUIRE (rBefore > 0.0f);
    CHECK (rAfter < rBefore * 0.5f);   // hot trigger -> at least 50% attenuation
}

TEST_CASE ("Ducker leaves target alone when trigger is silent", "[ducker]")
{
    gf::SidechainDucker d; d.prepare (kSr, 1, kN);
    juce::AudioBuffer<float> target (1, kN);  fillSine (target, 1000.0, 0.5f);
    juce::AudioBuffer<float> trigger (1, kN); trigger.clear();   // silence
    juce::AudioBuffer<float> orig; orig.makeCopyOf (target, true);

    d.process (target, trigger, 1.0f /*full duck*/, 0.0f);

    const float rBefore = rms (orig);
    const float rAfter  = rms (target);
    CHECK (std::abs (rAfter - rBefore) / rBefore < 0.02f);   // <2% drift on silent trigger
}

TEST_CASE ("Ducker stays finite under hot trigger + extreme settings", "[ducker]")
{
    gf::SidechainDucker d; d.prepare (kSr, 2, 512);
    d.setAttackMs (0.1f); d.setReleaseMs (10.0f);
    for (int blk = 0; blk < 50; ++blk)
    {
        juce::AudioBuffer<float> tgt (2, 512); fillSine (tgt, 1000.0, 0.5f);
        juce::AudioBuffer<float> trg (2, 512); fillSine (trg,  200.0, 0.95f);
        d.process (tgt, trg, 1.0f, 0.0f);
        for (int c = 0; c < 2; ++c) for (int i = 0; i < 512; ++i)
            REQUIRE (std::isfinite (tgt.getSample (c, i)));
    }
}
