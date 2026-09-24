// The Mod Matrix's new sources: LFO 2, the tempo-synced step sequencer and the
// random sample & hold as generators, then the end-to-end wiring -- every new
// source must actually reach a target parameter through the matrix.
#include <catch2/catch_test_macros.hpp>
#include "ProcessorFixture.h"
#include "GrainFreeze/Modulation/ModSources.h"

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int    kBlock = 512;

    // Run the generator forward for `seconds` in `blockSamples` steps, sampling
    // the value after each advance.
    template <typename Read>
    std::vector<float> sweep (gf::ModSources& src, gf::ModSources::Params p,
                              double seconds, double bpm, Read read, int blockSamples = 64)
    {
        std::vector<float> out;
        const int blocks = (int) (seconds * kSr / blockSamples);
        for (int i = 0; i < blocks; ++i)
        {
            src.advance (blockSamples, bpm, p);
            out.push_back (read (src));
        }
        return out;
    }
}

TEST_CASE ("LFO 2 runs free and locks to tempo", "[modsources]")
{
    gf::ModSources src;
    src.prepare (kSr);

    gf::ModSources::Params p;
    p.lfoRate = 1.0f;          // 1 Hz, free
    p.lfoShape = 0;            // sine
    p.lfoDivision = 0;

    // Count zero crossings over 4 seconds: a 1 Hz sine has 8.
    auto vals = sweep (src, p, 4.0, 120.0, [] (const gf::ModSources& s) { return s.lfo(); });
    int crossings = 0;
    for (size_t i = 1; i < vals.size(); ++i)
    {
        CHECK (std::abs (vals[i]) <= 1.0001f);
        if ((vals[i - 1] < 0.0f) != (vals[i] < 0.0f)) ++crossings;
    }
    INFO ("free 1 Hz sine zero crossings over 4 s: " << crossings);
    CHECK (crossings >= 7);
    CHECK (crossings <= 9);

    // Synced: 1/4 at 120 bpm is one beat = 0.5 s = 2 Hz -> 16 crossings in 4 s.
    src.reset();
    p.lfoDivision = 3;         // "1/4"
    p.lfoRate = 99.0f;         // ignored while synced
    vals = sweep (src, p, 4.0, 120.0, [] (const gf::ModSources& s) { return s.lfo(); });
    crossings = 0;
    for (size_t i = 1; i < vals.size(); ++i)
        if ((vals[i - 1] < 0.0f) != (vals[i] < 0.0f)) ++crossings;
    INFO ("1/4 at 120 bpm zero crossings over 4 s: " << crossings);
    CHECK (crossings >= 15);
    CHECK (crossings <= 17);
}

TEST_CASE ("The step sequencer steps in time and holds its values", "[modsources]")
{
    gf::ModSources src;
    src.prepare (kSr);

    const float steps[gf::ModSources::kMaxSteps] = { 1.0f, -1.0f, 0, 0, 0, 0, 0, 0,
                                                     0, 0, 0, 0, 0, 0, 0, 0 };
    gf::ModSources::Params p;
    p.steps = steps;
    p.stepLength = 2;
    p.stepDivision = 2;        // 1/4 -> one beat per step
    p.stepSmooth = 0.0f;

    // 120 bpm: a beat is 0.5 s, so the value flips every 0.5 s between +1 and -1.
    auto vals = sweep (src, p, 2.0, 120.0, [] (const gf::ModSources& s) { return s.step(); });
    for (float v : vals)
        CHECK ((std::abs (v - 1.0f) < 1.0e-6f || std::abs (v + 1.0f) < 1.0e-6f));
    int flips = 0;
    for (size_t i = 1; i < vals.size(); ++i)
        if (std::abs (vals[i] - vals[i - 1]) > 1.0e-6f) ++flips;
    INFO ("flips over 2 s at one step per beat: " << flips);
    CHECK (flips >= 3);
    CHECK (flips <= 5);

    // Glide smooths the transition instead of jumping.
    src.reset();
    p.stepSmooth = 1.0f;
    vals = sweep (src, p, 2.0, 120.0, [] (const gf::ModSources& s) { return s.step(); });
    int hardJumps = 0;
    for (size_t i = 1; i < vals.size(); ++i)
        if (std::abs (vals[i] - vals[i - 1]) > 0.5f) ++hardJumps;
    CHECK (hardJumps == 0);

    // Length is respected: only the active steps are visited.
    src.reset();
    p.stepSmooth = 0.0f;
    p.stepLength = 1;
    vals = sweep (src, p, 2.0, 120.0, [] (const gf::ModSources& s) { return s.step(); });
    for (float v : vals)
        CHECK (std::abs (v - 1.0f) < 1.0e-6f);   // never leaves step 1
}

TEST_CASE ("The random source holds, changes and repeats from a seed", "[modsources]")
{
    gf::ModSources a, b;
    a.prepare (kSr); b.prepare (kSr);
    a.setSeed (7u);  b.setSeed (7u);

    gf::ModSources::Params p;
    p.randomRate = 8.0f;

    auto va = sweep (a, p, 2.0, 120.0, [] (const gf::ModSources& s) { return s.random(); });
    auto vb = sweep (b, p, 2.0, 120.0, [] (const gf::ModSources& s) { return s.random(); });
    REQUIRE (va.size() == vb.size());
    for (size_t i = 0; i < va.size(); ++i)
        CHECK (std::abs (va[i] - vb[i]) < 1.0e-9f); // same seed, same sequence

    int changes = 0;
    for (size_t i = 1; i < va.size(); ++i)
    {
        CHECK (std::abs (va[i]) <= 1.0f);
        if (std::abs (va[i] - va[i - 1]) > 1.0e-9f) ++changes;
    }
    INFO ("new values over 2 s at 8 Hz: " << changes);
    CHECK (changes >= 14);
    CHECK (changes <= 18);
}

TEST_CASE ("Every Mod Matrix source reaches its target", "[modsources]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    // The grain cloud reads at `position` inside a multi-second capture buffer, so
    // it needs seconds of input before it has material there. Judge the second half.
    constexpr int kBlocks = 280;   // ~3 s at 48k / 512

    // Route a source to grain Output through slot 1 and return the settled second
    // half of the output. depth 0 is the unrouted baseline (identical whatever the
    // source is). Grain spawning is seeded, so two runs differ only by the routing.
    auto routed = [] (int sourceIndex, float depth) -> juce::AudioBuffer<float>
    {
        auto procOwner = std::make_unique<GrainFreezeProcessor>();
        auto& proc = *procOwner;
        proc.setDeterministicSeed (5u);
        fx::setParam (proc, "modSlot1Source", (float) sourceIndex);
        fx::setParam (proc, "modSlot1Target", 8.0f);     // "Output"
        fx::setParam (proc, "modSlot1Depth",  depth);
        fx::setParam (proc, "output",         0.5f);     // room to move either way
        fx::setParam (proc, "dryLevel",       0.0f);     // judge the wet path alone
        fx::setParam (proc, "modCcNumber",    74.0f);
        proc.prepareToPlay (kSr, kBlock);

        juce::AudioBuffer<float> work (2, kBlock);
        juce::AudioBuffer<float> tail (2, (kBlocks - kBlocks / 2) * kBlock);
        tail.clear();
        for (int b = 0; b < kBlocks; ++b)
        {
            fx::fillMusical (work, kSr, b * kBlock);
            juce::MidiBuffer midi;
            if (b == 0)
            {
                // A note at full velocity, plus mod wheel / expression / CC74 up,
                // so every MIDI-flavoured source has a value to give.
                midi.addEvent (juce::MidiMessage::noteOn (1, 84, 1.0f), 0);
                midi.addEvent (juce::MidiMessage::controllerEvent (1, 1,  127), 1);
                midi.addEvent (juce::MidiMessage::controllerEvent (1, 11, 127), 2);
                midi.addEvent (juce::MidiMessage::controllerEvent (1, 74, 127), 3);
            }
            proc.processBlock (work, midi);
            if (b >= kBlocks / 2)
                for (int c = 0; c < 2; ++c)
                    tail.copyFrom (c, (b - kBlocks / 2) * kBlock, work, c, 0, kBlock);
        }
        return tail;
    };

    const auto baseline = routed (0, 0.0f);
    const float base = fx::rms (baseline);
    REQUIRE (base > 1.0e-3f);

    struct Src { int index; const char* name; };
    for (auto s : { Src { 13, "LFO 2" }, Src { 14, "Step Seq" }, Src { 15, "Random" },
                    Src { 16, "Velocity" }, Src { 17, "Note Pitch" }, Src { 18, "Mod Wheel" },
                    Src { 19, "Expression" }, Src { 20, "MIDI CC" } })
    {
        auto out = routed (s.index, 1.0f);
        REQUIRE (out.getNumSamples() == baseline.getNumSamples());
        for (int c = 0; c < 2; ++c)
            out.addFrom (c, 0, baseline, c, 0, out.getNumSamples(), -1.0f);
        const float diff = fx::rms (out);
        INFO ("source " << s.name << ": routed-vs-unrouted difference "
                        << (100.0f * diff / base) << "% of the unrouted level");
        CHECK (std::isfinite (diff));
        CHECK (diff > base * 0.02f);   // routing it really moves the target
    }
}

TEST_CASE ("Every Mod Matrix target is really modulated", "[modsources]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    constexpr int kBlocks = 280;   // ~3 s: the grain cloud needs material to chew

    // Drive a target from the mod wheel (a steady 1.0 once CC1 is up) and return
    // the settled output. Every stage the new targets live in is switched on, so a
    // target that is wired but never consumed shows up as "no change".
    auto routed = [] (int targetIndex, float depth) -> juce::AudioBuffer<float>
    {
        auto procOwner = std::make_unique<GrainFreezeProcessor>();
        auto& proc = *procOwner;
        proc.setDeterministicSeed (11u);
        for (auto* on : { "damageOn", "airOn", "airExciterOn", "airSquelchOn", "airDelayOn",
                          "timeBreakerOn" })
            fx::setParam (proc, on, 1.0f);
        // Machines that default OFF also default to Mix 0, which would make their
        // stage inaudible no matter what the mod does to it.
        for (auto* mix : { "damageMix", "timeBreakerMix", "airExciterMix" })
            fx::setParam (proc, mix, 1.0f);
        fx::setParam (proc, "modSlot1Source", 18.0f);          // "Mod Wheel"
        fx::setParam (proc, "modSlot1Target", (float) targetIndex);
        fx::setParam (proc, "modSlot1Depth",  depth);
        proc.prepareToPlay (kSr, kBlock);

        juce::AudioBuffer<float> work (2, kBlock);
        juce::AudioBuffer<float> tail (2, (kBlocks - kBlocks / 2) * kBlock);
        tail.clear();
        for (int b = 0; b < kBlocks; ++b)
        {
            fx::fillMusical (work, kSr, b * kBlock);
            juce::MidiBuffer midi;
            if (b == 0)
                midi.addEvent (juce::MidiMessage::controllerEvent (1, 1, 127), 0);
            proc.processBlock (work, midi);
            if (b >= kBlocks / 2)
                for (int c = 0; c < 2; ++c)
                    tail.copyFrom (c, (b - kBlocks / 2) * kBlock, work, c, 0, kBlock);
        }
        return tail;
    };

    const auto baseline = routed (0, 0.0f);          // target "None"
    const float base = fx::rms (baseline);
    REQUIRE (base > 1.0e-3f);

    auto changeFrom = [&baseline, &base, &routed] (int targetIndex, float depth)
    {
        auto out = routed (targetIndex, depth);
        for (int c = 0; c < 2; ++c)
            out.addFrom (c, 0, baseline, c, 0, out.getNumSamples(), -1.0f);
        return fx::rms (out) / base;
    };

    struct Tgt { int index; const char* name; };
    for (auto t : { Tgt { 19, "Grain Shape" }, Tgt { 20, "Damage Amount" },
                    Tgt { 21, "Damage Bits" }, Tgt { 22, "Exciter Drive" },
                    Tgt { 23, "Air Mix" },     Tgt { 24, "Squelch Cutoff" },
                    Tgt { 25, "Air Delay Mix" }, Tgt { 26, "Dry/Wet" },
                    Tgt { 27, "Mix Width" },   Tgt { 28, "Stutter Chance" } })
    {
        // Some of these default to the top of their range, so try both directions.
        const float moved = juce::jmax (changeFrom (t.index, 1.0f), changeFrom (t.index, -1.0f));
        INFO ("target " << t.name << " moved the output by " << (100.0f * moved) << "%");
        CHECK (moved > 0.02f);
    }
}
