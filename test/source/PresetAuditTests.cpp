// Every factory preset, mechanically audited:
//  1. every parameter ID it sets actually exists (a typo'd ID silently no-ops)
//  2. it loads
//  3. run on a musical test signal it produces output that is finite, not
//     silent, not clipping past the limiter, and audibly different from the
//     input (a preset that leaves the signal untouched is a dud)
#include <catch2/catch_test_macros.hpp>
#include "ProcessorFixture.h"
#include "GrainFreeze/FactoryPresets.h"
#include <iostream>
#include <iomanip>

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int    kBlock = 512;
    constexpr int    kSeconds = 3;   // presets with long tails need time to bloom
}

TEST_CASE ("Every factory-preset parameter ID exists", "[presets]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;   // per-test JUCE lifetime: torn down before exit()
    auto procOwner = std::make_unique<GrainFreezeProcessor>();
    auto& proc = *procOwner;
    for (auto& fp : gf::factoryPresets())
        for (auto& pp : fp.params)
        {
            INFO ("preset \"" << fp.name << "\" sets \"" << pp.id << "\"");
            CHECK (proc.apvts.getParameter (pp.id) != nullptr);
        }
}

TEST_CASE ("Every factory preset loads, makes sound, and changes the signal", "[presets]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;   // per-test JUCE lifetime: torn down before exit()
    const int total = kSeconds * (int) kSr;

    // Reference: the input itself, so "differs from input" has something to compare to.
    juce::AudioBuffer<float> input (2, total);
    fx::fillMusical (input, kSr, 0);
    const float inRms = fx::rms (input, total / 2);

    std::cout << "[presets] " << std::left << std::setw (30) << "name"
              << std::right << std::setw (9) << "outRMS" << std::setw (8) << "peak" << std::setw (10) << "diff/in" << "\n";

    for (auto& fp : gf::factoryPresets())
    {
        auto procOwner = std::make_unique<GrainFreezeProcessor>();
        auto& proc = *procOwner;
        INFO ("preset: " << fp.name);
        REQUIRE (proc.presets.loadPreset (fp.name));
        proc.prepareToPlay (kSr, kBlock);

        juce::AudioBuffer<float> out (2, total);
        out.clear();   // the block loop leaves a partial tail block unwritten
        juce::MidiBuffer midi;
        for (int pos = 0; pos + kBlock <= total; pos += kBlock)
        {
            juce::AudioBuffer<float> blk (out.getArrayOfWritePointers(), 2, pos, kBlock);
            for (int c = 0; c < 2; ++c) blk.copyFrom (c, 0, input, c, pos, kBlock);
            proc.processBlock (blk, midi);
        }

        // Judge the second half: tails and capture buffers have settled.
        const int from = total / 2;
        const float outRms = fx::rms (out, from);
        const float pk     = fx::peak (out);
        juce::AudioBuffer<float> diff; diff.makeCopyOf (out, true);
        for (int c = 0; c < 2; ++c) diff.addFrom (c, 0, input, c, 0, total, -1.0f);
        const float diffRatio = fx::rms (diff, from) / juce::jmax (1.0e-6f, inRms);

        std::cout << "[presets] " << std::left << std::setw (30) << fp.name
                  << std::right << std::fixed << std::setprecision (4) << std::setw (9) << outRms
                  << std::setw (8) << std::setprecision (2) << pk
                  << std::setw (10) << std::setprecision (3) << diffRatio << "\n";

        CHECK (fx::allFinite (out));
        CHECK (outRms > 1.0e-3f);            // not silent
        CHECK (pk < 1.6f);                   // limiter is on by default; anything past this is a level bug
        CHECK (diffRatio > 0.05f);           // actually does something to the signal
    }
}
