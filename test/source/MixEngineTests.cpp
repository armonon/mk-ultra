#include <catch2/catch_test_macros.hpp>
#include "GrainFreeze/Mix/MixEngine.h"
#include <cmath>

TEST_CASE ("MixEngine output stays finite and bounded", "[mix][safety]")
{
    gf::mix::MixEngine mix;
    mix.prepare (44100.0, 256, 2);

    juce::AudioBuffer<float> dry (2, 256);
    juce::AudioBuffer<float> ent (2, 256);
    juce::AudioBuffer<float> pre (2, 256);
    juce::AudioBuffer<float> out (2, 256);
    for (int c = 0; c < 2; ++c)
    {
        for (int i = 0; i < 256; ++i)
        {
            const float x = 0.3f * std::sin ((float) i * 0.03f);
            dry.setSample (c, i, x);
            ent.setSample (c, i, x * 1.2f);
            pre.setSample (c, i, x * 0.7f);
        }
    }

    gf::mix::Params params;
    params.pluginOn = true;
    params.entropyOn = true;
    params.prettifierOn = true;
    params.limiterOn = true;
    params.dryLevel = 0.5f;
    params.chaosBeauty = 0.5f;
    params.outputLevel = 1.2f;
    params.ceilingDb = -0.5f;
    mix.processParallel (out, dry, ent, pre, params);

    for (int c = 0; c < out.getNumChannels(); ++c)
        for (int i = 0; i < out.getNumSamples(); ++i)
        {
            const float v = out.getSample (c, i);
            REQUIRE (std::isfinite (v));
            REQUIRE (std::abs (v) < 4.0f);
        }
}
