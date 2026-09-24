// Macro sweep audit. The one-page design bets that seven macros finish most
// sounds -- so each macro must (a) do something and (b) do it EVENLY across its
// travel. For each macro we render the default preset at 11 positions with a
// fixed grain seed, measure how much the output changes per step, and report
// the cumulative-change curve. A macro that's dead for its first half or that
// does everything in its last 20% is a broken macro.
#include <catch2/catch_test_macros.hpp>
#include "ProcessorFixture.h"
#include <iostream>
#include <iomanip>
#include <vector>

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int    kBlock = 512;
    constexpr int    kSeconds = 3;
    constexpr int    kSteps = 10;

    juce::AudioBuffer<float> render (const juce::String& macroId, float value, const juce::AudioBuffer<float>& input)
    {
        auto procOwner = std::make_unique<GrainFreezeProcessor>();
        auto& proc = *procOwner;
        proc.presets.loadDefaultPatch();
        proc.setDeterministicSeed (1234u);
        fx::setParam (proc, macroId, value);
        proc.prepareToPlay (kSr, kBlock);
        const int total = input.getNumSamples();
        juce::AudioBuffer<float> out (2, total);
        out.clear();   // the block loop leaves a partial tail block unwritten
        juce::MidiBuffer midi;
        for (int pos = 0; pos + kBlock <= total; pos += kBlock)
        {
            juce::AudioBuffer<float> blk (out.getArrayOfWritePointers(), 2, pos, kBlock);
            for (int c = 0; c < 2; ++c) blk.copyFrom (c, 0, input, c, pos, kBlock);
            proc.processBlock (blk, midi);
        }
        return out;
    }

    float diffRms (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b, int from)
    {
        juce::AudioBuffer<float> d; d.makeCopyOf (a, true);
        for (int c = 0; c < 2; ++c) d.addFrom (c, 0, b, c, 0, b.getNumSamples(), -1.0f);
        return fx::rms (d, from);
    }
}

TEST_CASE ("Macro sweep: every HOME macro does something, evenly", "[macros]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    const int total = kSeconds * (int) kSr, from = total / 2;
    juce::AudioBuffer<float> input (2, total);
    fx::fillMusical (input, kSr, 0);

    const char* macros[] = { "macroTexture", "macroBeauty", "macroSpace", "macroChaos",
                             "macroMotion", "macroDamage", "macroEmotion" };

    std::cout << "[macros] " << std::left << std::setw (14) << "macro"
              << std::right << std::setw (9) << "total" << std::setw (8) << "cum@.3" << std::setw (8) << "cum@.5"
              << std::setw (8) << "cum@.7" << std::setw (10) << "maxstep" << "   step-by-step change\n";

    for (auto* id : macros)
    {
        std::vector<juce::AudioBuffer<float>> outs;
        for (int k = 0; k <= kSteps; ++k)
            outs.push_back (render (id, (float) k / kSteps, input));

        const float base  = juce::jmax (1.0e-6f, fx::rms (outs[0], from));
        const float total01 = diffRms (outs[kSteps], outs[0], from) / base;   // 0 -> 1 overall effect

        std::vector<float> step (kSteps, 0.0f);
        float sum = 0.0f;
        for (int k = 1; k <= kSteps; ++k) { step[(size_t) k - 1] = diffRms (outs[(size_t) k], outs[(size_t) k - 1], from) / base; sum += step[(size_t) k - 1]; }
        std::vector<float> cum (kSteps + 1, 0.0f);
        for (int k = 1; k <= kSteps; ++k) cum[(size_t) k] = cum[(size_t) k - 1] + step[(size_t) k - 1] / juce::jmax (1.0e-6f, sum);
        float maxStep = 0.0f; for (float s : step) maxStep = juce::jmax (maxStep, s / juce::jmax (1.0e-6f, sum));

        std::cout << "[macros] " << std::left << std::setw (14) << id
                  << std::right << std::fixed << std::setprecision (3) << std::setw (9) << total01
                  << std::setw (8) << cum[3] << std::setw (8) << cum[5] << std::setw (8) << cum[7]
                  << std::setw (10) << maxStep << "   ";
        for (float s : step) std::cout << std::setprecision (2) << s / juce::jmax (1.0e-6f, sum) << " ";
        std::cout << "\n";

        INFO ("macro " << id);
        CHECK (total01 > 0.05f);                          // it does something audible
        // Shape: by half travel it should have delivered a meaningful share of its
        // total effect, but not nearly all of it. Wide bounds -- this is a smoke
        // alarm for dead zones and cliffs, not a taste test.
        CHECK (cum[5] > 0.20f);
        CHECK (cum[5] < 0.85f);
        CHECK (maxStep < 0.50f);                          // no single 10% of travel does half the work
    }
}
