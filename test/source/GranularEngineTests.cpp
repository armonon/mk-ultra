#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "GrainFreeze/GranularEngine.h"
#include "GrainFreeze/ModMatrix.h"
#include "GrainFreeze/Saturator.h"
#include "GrainFreeze/SpectralFreeze.h"

namespace
{
    constexpr double kSampleRate = 44100.0;
    constexpr int    kBlock      = 512;
    constexpr int    kChannels   = 2;

    juce::AudioBuffer<float> makeToneBlock (int channels, int numSamples, float freq, double sr)
    {
        juce::AudioBuffer<float> b (channels, numSamples);
        for (int c = 0; c < channels; ++c)
            for (int i = 0; i < numSamples; ++i)
                b.setSample (c, i, 0.5f * std::sin (2.0 * juce::MathConstants<double>::pi * freq * i / sr));
        return b;
    }

    float blockRMS (const juce::AudioBuffer<float>& b)
    {
        double acc = 0.0;
        for (int c = 0; c < b.getNumChannels(); ++c)
            for (int i = 0; i < b.getNumSamples(); ++i)
                acc += (double) b.getSample (c, i) * b.getSample (c, i);
        return (float) std::sqrt (acc / (b.getNumChannels() * b.getNumSamples()));
    }
}

TEST_CASE ("Engine prepares and reports ready", "[engine]")
{
    gf::GranularEngine engine;
    REQUIRE_FALSE (engine.isPrepared());
    engine.prepare (kSampleRate, kBlock, kChannels);
    REQUIRE (engine.isPrepared());
}

TEST_CASE ("Output is silent with no input fed", "[engine]")
{
    gf::GranularEngine engine;
    engine.prepare (kSampleRate, kBlock, kChannels);
    engine.setFrozen (true); // force grain emission even without dragging

    juce::AudioBuffer<float> out (kChannels, kBlock);
    engine.process (out);

    // With an empty capture buffer, grains read silence -> output stays ~0.
    REQUIRE (blockRMS (out) < 1.0e-6f);
}

TEST_CASE ("Frozen engine produces sound from captured material", "[engine][freeze]")
{
    gf::GranularEngine engine;
    engine.prepare (kSampleRate, kBlock, kChannels);
    engine.setDensity (60.0f);
    engine.setGrainSizeMs (100.0f);
    engine.setPosition (0.0f);

    // Feed several blocks of a tone so the capture buffer fills.
    for (int n = 0; n < 20; ++n)
    {
        auto tone = makeToneBlock (kChannels, kBlock, 220.0f, kSampleRate);
        engine.pushInput (tone);
        juce::AudioBuffer<float> scratch (kChannels, kBlock);
        engine.process (scratch);
    }

    // Freeze and render: grains should now read the held tone and make sound.
    engine.setFrozen (true);

    float maxRMS = 0.0f;
    for (int n = 0; n < 10; ++n)
    {
        juce::AudioBuffer<float> out (kChannels, kBlock);
        engine.process (out);
        maxRMS = juce::jmax (maxRMS, blockRMS (out));
    }
    REQUIRE (maxRMS > 1.0e-4f);
}

TEST_CASE ("Output stays finite and bounded", "[engine][safety]")
{
    gf::GranularEngine engine;
    engine.prepare (kSampleRate, kBlock, kChannels);
    engine.setDensity (80.0f);
    engine.setPitchSemis (12.0f);
    engine.setSprayMs (300.0f);
    engine.setOutputGain (1.5f);

    for (int n = 0; n < 50; ++n)
    {
        auto tone = makeToneBlock (kChannels, kBlock, 440.0f, kSampleRate);
        engine.pushInput (tone);
        juce::AudioBuffer<float> out (kChannels, kBlock);
        engine.process (out);

        for (int c = 0; c < out.getNumChannels(); ++c)
            for (int i = 0; i < out.getNumSamples(); ++i)
            {
                const float s = out.getSample (c, i);
                REQUIRE (std::isfinite (s));
                REQUIRE (std::abs (s) < 8.0f); // generous ceiling; catches runaway feedback
            }
    }
}

// ---- Modulation matrix tests ----

TEST_CASE ("ModMatrix is silent when no modulators are active", "[mod]")
{
    gf::ModMatrix m;
    m.prepare (100.0);
    for (int i = 0; i < 200; ++i) m.tick();
    for (int p = 0; p < gf::kNumModParams; ++p)
        REQUIRE (std::abs (m.getOffset ((gf::ParamId) p)) < 1.0e-6f);
}

TEST_CASE ("LFO produces bounded oscillating offset", "[mod][lfo]")
{
    gf::ModMatrix m;
    m.prepare (100.0);
    auto& s = m.slot (gf::ParamId::pitch);
    s.lfoRate.store (2.0f);
    s.lfoDepth.store (1.0f);
    s.lfoShape.store (0); // sine

    float minV = 1.0f, maxV = -1.0f;
    for (int i = 0; i < 100; ++i) // 1 second at 100 Hz => 2 full cycles
    {
        m.tick();
        const float v = m.getOffset (gf::ParamId::pitch);
        minV = juce::jmin (minV, v);
        maxV = juce::jmax (maxV, v);
        REQUIRE (std::abs (v) <= 1.0f + 1.0e-5f);
    }
    REQUIRE (maxV > 0.5f);   // swung clearly positive
    REQUIRE (minV < -0.5f);  // and clearly negative
}

// ---- Saturator tests ----

TEST_CASE ("Saturator is transparent when mix is zero", "[sat]")
{
    gf::Saturator s;
    s.prepare (44100.0, 2, 512);
    s.setDrive (12.0f);
    s.setMix (0.0f);

    juce::AudioBuffer<float> b (2, 256);
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < 256; ++i)
            b.setSample (c, i, 0.4f * std::sin (i * 0.1f));

    juce::AudioBuffer<float> ref (b);
    s.process (b);

    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < 256; ++i)
            REQUIRE (std::abs (b.getSample (c, i) - ref.getSample (c, i)) < 1.0e-6f);
}

TEST_CASE ("Saturator stays finite and bounded under heavy drive", "[sat][safety]")
{
    for (int type = 0; type <= 2; ++type)
    {
        gf::Saturator s;
        s.prepare (44100.0, 2, 512);
        s.setType (type);
        s.setDrive (24.0f);
        s.setMix (1.0f);

        juce::AudioBuffer<float> b (2, 512);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 512; ++i)
                b.setSample (c, i, 0.9f * std::sin (i * 0.05f));

        s.process (b);

        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 512; ++i)
            {
                const float v = b.getSample (c, i);
                REQUIRE (std::isfinite (v));
                REQUIRE (std::abs (v) < 4.0f);
            }
    }
}

// ---- Spectral freeze tests ----

TEST_CASE ("SpectralFreeze passes dry when not frozen", "[spectral]")
{
    gf::SpectralFreeze sp;
    sp.prepare (44100.0, 2);
    sp.setMix (1.0f);
    sp.setFrozen (false);

    juce::AudioBuffer<float> b (2, 1024);
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < 1024; ++i)
            b.setSample (c, i, 0.3f * std::sin (i * 0.07f));
    juce::AudioBuffer<float> ref (b);

    sp.process (b);
    // Not frozen: buffer should be unchanged (analysis only, no blend).
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < 1024; ++i)
            REQUIRE (std::abs (b.getSample (c, i) - ref.getSample (c, i)) < 1.0e-6f);
}

TEST_CASE ("SpectralFreeze output stays finite when frozen", "[spectral][safety]")
{
    gf::SpectralFreeze sp;
    sp.prepare (44100.0, 2);
    sp.setMix (1.0f);
    sp.setShimmer (0.8f);

    // Prime with several blocks of tone while analyzing.
    for (int n = 0; n < 8; ++n)
    {
        juce::AudioBuffer<float> b (2, 512);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 512; ++i)
                b.setSample (c, i, 0.4f * std::sin ((n * 512 + i) * 0.05f));
        sp.process (b);
    }

    sp.setFrozen (true);
    for (int n = 0; n < 16; ++n)
    {
        juce::AudioBuffer<float> b (2, 512);
        b.clear();
        sp.process (b);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 512; ++i)
            {
                const float v = b.getSample (c, i);
                REQUIRE (std::isfinite (v));
                REQUIRE (std::abs (v) < 8.0f);
            }
    }
}
