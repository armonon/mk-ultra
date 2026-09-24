// Bus routing + latency reporting.
//
// 1. The optional Sidechain input exists, is off by default, and when enabled it
//    keys the Ducker without ever leaking into the main output.
// 2. Every latency-adding stage is reported to the host. AIR delays its whole
//    output by its exciter's oversampler latency; before this was reported, a
//    track with AIR on drifted late against everything else.
#include <catch2/catch_test_macros.hpp>
#include "ProcessorFixture.h"

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int    kBlock = 512;

    // Turn the plugin into a straight wire so a test can measure one stage.
    void bypassEverything (GrainFreezeProcessor& p)
    {
        for (auto* id : { "textureGrainOn", "beautySpaceOn", "identityLossOn", "spectralOn",
                          "pitchFormantOn", "timeBreakerOn", "damageOn", "motionMatrixOn",
                          "limiterOn", "duckOn", "sampleMode" })
            fx::setParam (p, id, 0.0f);
    }

    bool enableSidechain (GrainFreezeProcessor& p)
    {
        auto layout = p.getBusesLayout();
        if (layout.inputBuses.size() < 2)
            return false;
        layout.inputBuses.getReference (1) = juce::AudioChannelSet::stereo();
        return p.setBusesLayout (layout);
    }
}

TEST_CASE ("The Sidechain input bus exists and is off by default", "[routing]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    auto procOwner = std::make_unique<GrainFreezeProcessor>();
    auto& proc = *procOwner;

    auto* side = proc.getBus (true, 1);
    REQUIRE (side != nullptr);
    CHECK (side->getName() == "Sidechain");
    CHECK_FALSE (side->isEnabled());              // a plain insert stays 2-in / 2-out
    CHECK (proc.getMainBusNumInputChannels() == 2);
    CHECK (enableSidechain (proc));
    CHECK (proc.getBus (true, 1)->isEnabled());
    CHECK (proc.getMainBusNumInputChannels() == 2);   // main bus is untouched by it
}

TEST_CASE ("An external key ducks the wet and never leaks into the output", "[routing]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    constexpr int kBlocks = 40;

    // One run: main input = musical signal, sidechain = `keyLevel` noise bursts.
    // Returns the output RMS over the second half (after the ducker has settled).
    auto run = [] (float keyLevel, bool silentMain) -> juce::AudioBuffer<float>
    {
        auto procOwner = std::make_unique<GrainFreezeProcessor>();
        auto& proc = *procOwner;
        proc.setDeterministicSeed (99u);
        REQUIRE (enableSidechain (proc));
        fx::setParam (proc, "duckOn",        1.0f);
        fx::setParam (proc, "duckSource",    1.0f);    // 1 = Sidechain
        fx::setParam (proc, "duckAmount",    1.0f);
        fx::setParam (proc, "duckThreshold", 0.02f);
        fx::setParam (proc, "duckAttack",    1.0f);
        fx::setParam (proc, "duckRelease",   20.0f);
        fx::setParam (proc, "dryLevel",      0.0f);    // judge the wet path alone
        fx::setParam (proc, "dryWet",        1.0f);
        proc.prepareToPlay (kSr, kBlock);

        juce::AudioBuffer<float> out (2, kBlocks * kBlock);
        out.clear();
        juce::AudioBuffer<float> work (4, kBlock);     // 2 main + 2 sidechain
        juce::MidiBuffer midi;
        for (int b = 0; b < kBlocks; ++b)
        {
            work.clear();
            if (! silentMain)
            {
                juce::AudioBuffer<float> mainIn (work.getArrayOfWritePointers(), 2, kBlock);
                fx::fillMusical (mainIn, kSr, b * kBlock);
            }
            for (int c = 2; c < 4; ++c)
                for (int i = 0; i < kBlock; ++i)
                    work.setSample (c, i, keyLevel);   // steady-state key
            proc.processBlock (work, midi);
            for (int c = 0; c < 2; ++c)
                out.copyFrom (c, b * kBlock, work, c, 0, kBlock);
        }
        return out;
    };

    const auto quietKey = run (0.0f, false);
    const auto loudKey  = run (0.9f, false);
    const int  half     = quietKey.getNumSamples() / 2;
    const float openRms   = fx::rms (quietKey, half);
    const float duckedRms = fx::rms (loudKey,  half);
    INFO ("wet with no key " << openRms << ", wet with a full-scale key " << duckedRms);
    CHECK (fx::allFinite (loudKey));
    CHECK (openRms > 1.0e-3f);                  // there is a wet path to duck
    CHECK (duckedRms < openRms * 0.5f);         // the external key really ducks it

    // The key signal itself must never reach the output: silent main in, loud key.
    const auto keyOnly = run (0.9f, true);
    CHECK (fx::peak (keyOnly) < 1.0e-3f);
}

TEST_CASE ("Latency reporting covers AIR, not just the formant shifter", "[routing]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    auto procOwner = std::make_unique<GrainFreezeProcessor>();
    auto& proc = *procOwner;
    bypassEverything (proc);
    fx::setParam (proc, "airOn", 0.0f);
    proc.prepareToPlay (kSr, kBlock);
    CHECK (proc.getLatencySamples() == 0);

    fx::setParam (proc, "airOn", 1.0f);
    proc.prepareToPlay (kSr, kBlock);
    const int reported = proc.getLatencySamples();
    const int airLatency = proc.airEngine.getLatencySamples();
    INFO ("AIR reports " << airLatency << " samples, host is told " << reported);
    CHECK (airLatency > 0);
    CHECK (reported == airLatency);

    // And the reported figure is the real one: a low sine (well under the
    // crossover) comes out delayed by that many samples. On top of it sits the
    // LR4 crossover's group delay -- a few samples at 100 Hz / 2.5 kHz, frequency
    // dependent, and not something any plugin reports as latency.
    fx::setParam (proc, "airMix",       0.0f);   // dry top, so AIR is pure delay
    fx::setParam (proc, "airSquelchOn", 0.0f);
    fx::setParam (proc, "airExciterOn", 0.0f);
    fx::setParam (proc, "airShelfOn",   0.0f);
    fx::setParam (proc, "airPhaserOn",  0.0f);
    fx::setParam (proc, "airDelayOn",   0.0f);
    proc.prepareToPlay (kSr, kBlock);

    constexpr int kN = kBlock * 8;
    juce::AudioBuffer<float> in (2, kN), out (2, kN);
    in.clear(); out.clear();
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < kN; ++i)
            in.setSample (c, i, 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 100.0 * i / kSr));

    juce::MidiBuffer midi;
    for (int pos = 0; pos + kBlock <= kN; pos += kBlock)
    {
        juce::AudioBuffer<float> blk (out.getArrayOfWritePointers(), 2, pos, kBlock);
        for (int c = 0; c < 2; ++c) blk.copyFrom (c, 0, in, c, pos, kBlock);
        proc.processBlock (blk, midi);
    }

    // Best-correlating lag over the settled second half.
    int bestLag = -1; double best = -1.0;
    const int from = kN / 2, len = kN / 4;
    for (int lag = 0; lag <= 64; ++lag)
    {
        double acc = 0.0;
        for (int i = 0; i < len; ++i)
            acc += (double) in.getSample (0, from + i) * out.getSample (0, from + i + lag);
        if (acc > best) { best = acc; bestLag = lag; }
    }
    INFO ("measured delay " << bestLag << " samples vs reported " << reported);
    CHECK (bestLag >= reported);            // we are not over-reporting
    CHECK (bestLag - reported <= 6);        // ...and not under-reporting by a stage
}
