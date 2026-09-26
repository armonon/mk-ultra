// Transient-locked grains: the detector has to find the hits where they
// actually are, ignore steady material that never hits, and the processor has
// to spawn grains on them rather than on the free-running clock.
#include <catch2/catch_test_macros.hpp>
#include "ProcessorFixture.h"
#include "GrainFreeze/OnsetDetector.h"

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int    kBlock = 512;

    // A drum-ish hit: a short burst with a fast attack and a decay.
    void addHit (juce::AudioBuffer<float>& b, int at, float amp = 0.9f)
    {
        const int len = (int) (kSr * 0.06);
        std::mt19937 rng ((unsigned) at);
        std::uniform_real_distribution<float> d (-1.0f, 1.0f);
        for (int i = 0; i < len && at + i < b.getNumSamples(); ++i)
        {
            const float env = std::exp (-(float) i / (float) (kSr * 0.012));
            const float v = amp * env * (0.6f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                          * 110.0f * (float) i / (float) kSr)
                                         + 0.4f * d (rng));
            for (int c = 0; c < b.getNumChannels(); ++c)
                b.addSample (c, at + i, v);
        }
    }
}

TEST_CASE ("The onset detector finds hits where they are", "[transient]")
{
    const int total = (int) (kSr * 2.0);
    juce::AudioBuffer<float> buf (1, total);
    buf.clear();

    // Four on the floor at 120 bpm: every half second.
    std::vector<int> expected;
    for (int i = 0; i < 4; ++i)
    {
        const int at = (int) (kSr * 0.5 * i) + 1000;
        expected.push_back (at);
        addHit (buf, at);
    }

    gf::OnsetDetector det;
    det.prepare (kSr);
    det.setSensitivity (0.5f);

    std::vector<int> found;
    for (int pos = 0; pos + kBlock <= total; pos += kBlock)
    {
        juce::AudioBuffer<float> blk (buf.getArrayOfWritePointers(), 1, pos, kBlock);
        int offsets[64];
        const int n = det.process (blk, offsets, 64);
        for (int i = 0; i < n; ++i)
            found.push_back (pos + offsets[i]);
    }

    INFO ("found " << found.size() << " hits, expected " << expected.size());
    REQUIRE (found.size() == expected.size());
    for (size_t i = 0; i < expected.size(); ++i)
    {
        INFO ("hit " << i << " at " << found[i] << ", expected near " << expected[i]);
        CHECK (std::abs (found[i] - expected[i]) < (int) (kSr * 0.01));   // within 10 ms
    }
}

TEST_CASE ("Steady material produces no false hits", "[transient]")
{
    const int total = (int) (kSr * 2.0);
    juce::AudioBuffer<float> buf (1, total);
    for (int i = 0; i < total; ++i)
        buf.setSample (0, i, 0.4f * std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * i / kSr));

    gf::OnsetDetector det;
    det.prepare (kSr);
    det.setSensitivity (0.5f);

    int found = 0;
    for (int pos = 0; pos + kBlock <= total; pos += kBlock)
    {
        juce::AudioBuffer<float> blk (buf.getArrayOfWritePointers(), 1, pos, kBlock);
        int offsets[64];
        found += det.process (blk, offsets, 64);
    }
    INFO ("false hits on a steady tone: " << found);
    CHECK (found <= 1);   // the very start of the tone is a legitimate onset
}

TEST_CASE ("Sensitivity decides how much gets through", "[transient]")
{
    const int total = (int) (kSr * 2.0);
    auto countWith = [total] (float sensitivity)
    {
        juce::AudioBuffer<float> buf (1, total);
        buf.clear();
        // Loud hits on the beat, quiet ghost notes between them.
        for (int i = 0; i < 4; ++i)
        {
            addHit (buf, (int) (kSr * 0.5 * i) + 1000, 0.9f);
            addHit (buf, (int) (kSr * 0.5 * i) + 1000 + (int) (kSr * 0.25), 0.12f);
        }
        gf::OnsetDetector det;
        det.prepare (kSr);
        det.setSensitivity (sensitivity);
        int found = 0;
        for (int pos = 0; pos + kBlock <= total; pos += kBlock)
        {
            juce::AudioBuffer<float> blk (buf.getArrayOfWritePointers(), 1, pos, kBlock);
            int offsets[64];
            found += det.process (blk, offsets, 64);
        }
        return found;
    };

    const int low  = countWith (0.0f);
    const int high = countWith (1.0f);
    INFO ("low sensitivity found " << low << ", high found " << high);
    CHECK (low >= 3);        // still catches the real hits
    CHECK (high > low);      // ...and turning it up catches the ghosts too
}

TEST_CASE ("Transient mode makes the grain cloud follow the hits", "[transient]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    constexpr int kBlocks = 200;

    // Feed a sparse pulse train. In Free mode the cloud runs on its own clock and
    // fills the gaps; in Transient mode it should be quiet between the hits.
    auto run = [] (int triggerMode) -> juce::AudioBuffer<float>
    {
        auto procOwner = std::make_unique<GrainFreezeProcessor>();
        auto& proc = *procOwner;
        proc.setDeterministicSeed (4u);
        fx::setParam (proc, "grainTrigger",   (float) triggerMode);
        fx::setParam (proc, "transientSense", 0.5f);
        fx::setParam (proc, "dryLevel",       0.0f);   // judge the grain path alone
        fx::setParam (proc, "beautySpaceOn",  0.0f);   // no reverb tail to fill the gaps
        fx::setParam (proc, "grainSize",      60.0f);
        fx::setParam (proc, "density",        40.0f);
        proc.prepareToPlay (kSr, kBlock);

        juce::AudioBuffer<float> out (2, kBlocks * kBlock);
        out.clear();
        juce::AudioBuffer<float> work (2, kBlock);
        juce::MidiBuffer midi;
        for (int b = 0; b < kBlocks; ++b)
        {
            work.clear();
            // One hit every 8 blocks (~85 ms apart).
            if (b % 8 == 0)
                addHit (work, 0, 0.9f);
            proc.processBlock (work, midi);
            for (int c = 0; c < 2; ++c)
                out.copyFrom (c, b * kBlock, work, c, 0, kBlock);
        }
        return out;
    };

    auto freeRun  = run (0);
    auto transRun = run (1);
    CHECK (fx::allFinite (transRun));

    // Measure how much energy sits in the gaps -- the second half of each
    // 8-block period, well after a hit.
    auto gapEnergy = [] (const juce::AudioBuffer<float>& b)
    {
        double sum = 0.0; int n = 0;
        for (int blk = 0; blk < kBlocks; ++blk)
        {
            if (blk % 8 < 5) continue;                 // skip the hit and its decay
            for (int i = 0; i < kBlock; ++i)
            {
                const double v = b.getSample (0, blk * kBlock + i);
                sum += v * v; ++n;
            }
        }
        return std::sqrt (sum / juce::jmax (1, n));
    };

    const double freeGap  = gapEnergy (freeRun);
    const double transGap = gapEnergy (transRun);
    INFO ("gap energy: free clock " << freeGap << ", transient " << transGap);
    CHECK (freeGap > 1.0e-4);                 // the free clock really does fill the gaps
    CHECK (transGap < freeGap * 0.6);         // transient mode leaves them alone
}
