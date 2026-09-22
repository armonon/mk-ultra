// CPU benchmark of the real signal chain. Prints ms/block and % of real-time
// for three scenarios and FAILS if any scenario can't keep up with real time
// (that's a shipping bug, not a perf nicety). Numbers are machine-dependent;
// the point is a floor, a trend, and a way to see what "everything on" costs.
#include <catch2/catch_test_macros.hpp>
#include "ProcessorFixture.h"
#include <chrono>
#include <iostream>
#include <iomanip>

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int    kBlock = 512;

    struct Result { double meanMs, maxMs, pctRealtime; };

    Result runScenario (GrainFreezeProcessor& proc, int warmBlocks, int measureBlocks)
    {
        proc.prepareToPlay (kSr, kBlock);
        juce::AudioBuffer<float> buf (2, kBlock);
        juce::MidiBuffer midi;
        const double blockMs = 1000.0 * kBlock / kSr;

        for (int b = 0; b < warmBlocks; ++b)
        {
            fx::fillMusical (buf, kSr, b * kBlock);
            proc.processBlock (buf, midi);
        }
        double total = 0.0, worst = 0.0;
        for (int b = 0; b < measureBlocks; ++b)
        {
            fx::fillMusical (buf, kSr, (warmBlocks + b) * kBlock);
            const auto t0 = std::chrono::steady_clock::now();
            proc.processBlock (buf, midi);
            const auto t1 = std::chrono::steady_clock::now();
            const double ms = std::chrono::duration<double, std::milli> (t1 - t0).count();
            total += ms; worst = juce::jmax (worst, ms);
            REQUIRE (fx::allFinite (buf));
        }
        const double mean = total / measureBlocks;
        return { mean, worst, 100.0 * mean / blockMs };
    }

    void report (const char* name, const Result& r)
    {
        std::cout << std::fixed << std::setprecision (3)
                  << "[bench] " << std::left << std::setw (34) << name
                  << " mean " << std::setw (7) << r.meanMs << " ms/block   max " << std::setw (7) << r.maxMs
                  << " ms   " << std::setprecision (1) << r.pctRealtime << "% of real-time @48k/512\n";
    }

    void everythingOn (GrainFreezeProcessor& proc)
    {
        for (auto* p : proc.getParameters())
            if (auto* b = dynamic_cast<juce::AudioParameterBool*> (p))
                b->setValueNotifyingHost (1.0f);
        // Keep the plugin actually processing (panic would kill the tail).
        fx::setParam (proc, "panic", 0.0f);
        fx::setParam (proc, "pluginOn", 1.0f);
    }
}

TEST_CASE ("Benchmark: default preset keeps up with real time", "[bench]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;   // per-test JUCE lifetime: torn down before exit()
    GrainFreezeProcessor proc;
    proc.presets.loadDefaultPatch();
    const auto r = runScenario (proc, 40, 400);
    report ("default preset", r);
    CHECK (r.pctRealtime < 100.0);
}

TEST_CASE ("Benchmark: every stage on", "[bench]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;   // per-test JUCE lifetime: torn down before exit()
    GrainFreezeProcessor proc;
    proc.presets.loadDefaultPatch();
    everythingOn (proc);
    const auto r = runScenario (proc, 40, 400);
    report ("every stage on (defaults)", r);
    CHECK (r.pctRealtime < 100.0);
}

TEST_CASE ("Benchmark: worst case -- every stage on, max grain density", "[bench]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;   // per-test JUCE lifetime: torn down before exit()
    GrainFreezeProcessor proc;
    proc.presets.loadDefaultPatch();
    everythingOn (proc);
    fx::setParam (proc, "density",   180.0f);
    fx::setParam (proc, "grainSize", 2000.0f);
    fx::setParam (proc, "spray",     1600.0f);
    fx::setParam (proc, "damageAmount", 1.0f);
    fx::setParam (proc, "airExciterDrive", 1.0f);
    const auto r = runScenario (proc, 60, 400);
    report ("worst case", r);
    CHECK (r.pctRealtime < 100.0);
}
