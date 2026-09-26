// Amount: one dial that scales whatever the loaded preset did. 0 is the
// plugin's defaults, 1 is the preset as written, beyond that pushes it further.
#include <catch2/catch_test_macros.hpp>
#include "ProcessorFixture.h"
#include "GrainFreeze/FactoryPresets.h"

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int    kBlock = 512;

    float normValue (GrainFreezeProcessor& p, const juce::String& id)
    {
        auto* param = p.apvts.getParameter (id);
        return param != nullptr ? param->getValue() : -1.0f;
    }

    // A parameter this preset actually moves away from its default.
    juce::String findMovedParam (GrainFreezeProcessor& p)
    {
        for (auto* param : p.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
                if (std::abs (rp->getValue() - rp->getDefaultValue()) > 0.15f
                    && rp->paramID != "presetAmount")
                    return rp->paramID;
        return {};
    }
}

TEST_CASE ("Amount scales the loaded preset", "[amount]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    auto procOwner = std::make_unique<GrainFreezeProcessor>();
    auto& proc = *procOwner;

    REQUIRE (proc.presets.loadPreset ("Destroyed - Meltdown"));
    // Loading re-anchors Amount at 1: the preset as its author wrote it.
    CHECK (proc.apvts.getRawParameterValue ("presetAmount")->load() == 1.0f);

    const auto id = findMovedParam (proc);
    REQUIRE (id.isNotEmpty());
    auto* param = proc.apvts.getParameter (id);
    REQUIRE (param != nullptr);

    const float authored = param->getValue();
    const float base = param->getDefaultValue();
    INFO ("scaling \"" << id << "\": default " << base << ", preset " << authored);

    // 0 returns it to the default.
    fx::setParam (proc, "presetAmount", 0.0f);
    proc.applyPresetAmount();
    CHECK (std::abs (normValue (proc, id) - base) < 0.01f);

    // 1 restores exactly what the preset asked for.
    fx::setParam (proc, "presetAmount", 1.0f);
    proc.applyPresetAmount();
    CHECK (std::abs (normValue (proc, id) - authored) < 0.01f);

    // Halfway sits halfway.
    fx::setParam (proc, "presetAmount", 0.5f);
    proc.applyPresetAmount();
    CHECK (std::abs (normValue (proc, id) - (base + (authored - base) * 0.5f)) < 0.01f);

    // Past 1 pushes further in the same direction (clamped at the range ends).
    fx::setParam (proc, "presetAmount", 2.0f);
    proc.applyPresetAmount();
    const float pushed = normValue (proc, id);
    if (authored > base)  CHECK (pushed >= authored - 0.001f);
    else                  CHECK (pushed <= authored + 0.001f);
}

TEST_CASE ("Amount leaves locked knobs and the plumbing alone", "[amount]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    auto procOwner = std::make_unique<GrainFreezeProcessor>();
    auto& proc = *procOwner;
    REQUIRE (proc.presets.loadPreset ("Beautiful - Angel Dust"));

    // The morph axes and Panic are gestures, not part of a sound.
    fx::setParam (proc, "macroMorph", 0.8f);
    fx::setParam (proc, "presetAmount", 0.0f);
    proc.applyPresetAmount();
    CHECK (proc.apvts.getRawParameterValue ("macroMorph")->load() == 0.8f);
    CHECK (proc.apvts.getRawParameterValue ("panic")->load() < 0.5f);
}

TEST_CASE ("Amount changes what comes out", "[amount]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    constexpr int kBlocks = 200;

    auto render = [] (float amount)
    {
        auto procOwner = std::make_unique<GrainFreezeProcessor>();
        auto& proc = *procOwner;
        proc.setDeterministicSeed (8u);
        REQUIRE (proc.presets.loadPreset ("Destroyed - Meltdown"));
        fx::setParam (proc, "presetAmount", amount);
        proc.applyPresetAmount();
        proc.prepareToPlay (kSr, kBlock);

        juce::AudioBuffer<float> work (2, kBlock);
        juce::AudioBuffer<float> tail (2, (kBlocks / 2) * kBlock);
        tail.clear();
        juce::MidiBuffer midi;
        for (int b = 0; b < kBlocks; ++b)
        {
            fx::fillMusical (work, kSr, b * kBlock);
            proc.processBlock (work, midi);
            if (b >= kBlocks / 2)
                for (int c = 0; c < 2; ++c)
                    tail.copyFrom (c, (b - kBlocks / 2) * kBlock, work, c, 0, kBlock);
        }
        return tail;
    };

    auto atZero = render (0.0f);
    auto atOne  = render (1.0f);
    CHECK (fx::allFinite (atZero));
    CHECK (fx::allFinite (atOne));

    auto diff = atOne;
    for (int c = 0; c < 2; ++c)
        diff.addFrom (c, 0, atZero, c, 0, diff.getNumSamples(), -1.0f);
    const float change = fx::rms (diff) / juce::jmax (1.0e-6f, fx::rms (atOne));
    INFO ("Amount 0 vs 1 differ by " << (100.0f * change) << "% of the full-amount level");
    CHECK (change > 0.10f);
}
