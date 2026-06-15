#include <catch2/catch_test_macros.hpp>
#include "GrainFreeze/Prettifier/PrettifierEngine.h"
#include <cmath>

TEST_CASE ("PrettifierEngine stays finite and bounded", "[prettifier][safety]")
{
    gf::pretty::PrettifierEngine p;
    p.prepare (48000.0, 512, 2);

    juce::AudioBuffer<float> b (2, 512);
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < 512; ++i)
            b.setSample (c, i, 0.4f * std::sin ((float) i * 0.02f));

    gf::pretty::Params params;
    params.enabled = true;
    params.echoOn = true;
    params.reverbOn = true;
    params.chorusOn = true;
    params.beautyOn = true;
    params.polishOn = true;
    p.process (b, params, 120.0, true);

    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float v = b.getSample (c, i);
            REQUIRE (std::isfinite (v));
            REQUIRE (std::abs (v) < 8.0f);
        }
}
