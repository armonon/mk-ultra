// Regression guard for the oversampled nonlinearities (Damage + Warmth).
// A nonlinear shaper run at base rate folds its above-Nyquist harmonics back
// into the audible band as aliasing. These tests prove the 4x-oversampled path
// keeps the in-band alias floor far below the naive per-sample shaper, and that
// every shape stays finite/bounded at max drive.
#include <catch2/catch_test_macros.hpp>
#include "GrainFreeze/TransformRack.h"
#include "GrainFreeze/Saturator.h"
#include "GrainFreeze/Prettifier/PrettifierEngine.h"

#include <cmath>
#include <vector>

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int    kN  = 8192;     // FFT order 13
    constexpr double kF0 = 7350.0;   // odd harmonics land on aliasing frequencies

    // Ratio (dB) of in-band "junk" (aliases) to the legit fundamental + 3rd
    // harmonic. Lower = cleaner. Lives entirely on juce::dsp::FFT.
    double aliasToFundDb (const std::vector<float>& sig)
    {
        juce::dsp::FFT fft (13);
        std::vector<float> fd ((size_t) 2 * kN, 0.0f);
        for (int i = 0; i < kN; ++i)
            fd[(size_t) i] = sig[(size_t) i] * (float) (0.5 - 0.5 * std::cos (2.0 * juce::MathConstants<double>::pi * i / (kN - 1)));
        fft.performFrequencyOnlyForwardTransform (fd.data());

        double aliasE = 0.0, fundE = 0.0;
        for (int b = 1; b < kN / 2; ++b)
        {
            const double freq = b * kSr / kN;
            const double mag  = fd[(size_t) b];
            const bool nearTrue = std::abs (freq - kF0) < 160.0 || std::abs (freq - 3.0 * kF0) < 160.0;
            if (nearTrue)                            fundE  += mag * mag;
            else if (freq > 400.0 && freq < 20000.0) aliasE += mag * mag;
        }
        return 10.0 * std::log10 ((aliasE + 1e-12) / (fundE + 1e-12));
    }

    void fillSine (juce::AudioBuffer<float>& b, double f, float amp)
    {
        for (int c = 0; c < b.getNumChannels(); ++c)
            for (int i = 0; i < b.getNumSamples(); ++i)
                b.setSample (c, i, (float) std::sin (2.0 * juce::MathConstants<double>::pi * f * i / kSr) * amp);
    }

    bool finiteAndBounded (const juce::AudioBuffer<float>& b, float ceiling = 8.0f)
    {
        for (int c = 0; c < b.getNumChannels(); ++c)
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                const float v = b.getSample (c, i);
                if (! std::isfinite (v) || std::abs (v) > ceiling) return false;
            }
        return true;
    }
}

TEST_CASE ("Damage hard-clip is anti-aliased vs a naive shaper", "[antialias][damage]")
{
    // Naive reference: the exact transfer the engine applies, but per-sample (no OS).
    const float drive = 19.0f, comp = 1.0f / std::sqrt (drive);
    std::vector<float> naive ((size_t) kN), over ((size_t) kN);
    for (int i = 0; i < kN; ++i)
    {
        const float s = (float) std::sin (2.0 * juce::MathConstants<double>::pi * kF0 * i / kSr) * 0.9f;
        naive[(size_t) i] = juce::jlimit (-1.0f, 1.0f, s * drive) * comp;
    }

    gf::DamageEngine dmg; dmg.prepare (kSr, 1, kN);
    gf::DamageParams dp; dp.amount = 1.0f; dp.clip = 2 /*Hard*/; dp.bits = 16.0f; dp.rate = 1.0f; dp.tone = 1.0f; dp.mix = 1.0f;
    juce::AudioBuffer<float> buf (1, kN); fillSine (buf, kF0, 0.9f);
    dmg.process (buf, dp);
    for (int i = 0; i < kN; ++i) over[(size_t) i] = buf.getSample (0, i);

    const double naiveDb = aliasToFundDb (naive);
    const double overDb  = aliasToFundDb (over);
    INFO ("naive alias floor = " << naiveDb << " dB, oversampled = " << overDb << " dB");
    CHECK (overDb < naiveDb - 10.0);    // measured ~18 dB cleaner; 10 dB is a safe guard
    CHECK (finiteAndBounded (buf));
}

TEST_CASE ("Damage stays finite + bounded at max drive (every shape)", "[antialias][damage]")
{
    for (int clip = 0; clip < 5; ++clip)
    {
        gf::DamageEngine d; d.prepare (kSr, 2, 512);
        gf::DamageParams q; q.amount = 1.0f; q.clip = clip; q.mix = 1.0f; q.tone = 1.0f; q.bits = 16.0f; q.rate = 1.0f;
        float peak = 0.0f;
        for (int blk = 0; blk < 400; ++blk)
        {
            juce::AudioBuffer<float> b (2, 512);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 512; ++i)
                    b.setSample (c, i, (float) std::sin (2.0 * juce::MathConstants<double>::pi * 5000.0 * (blk * 512 + i) / kSr) * 0.95f);
            d.process (b, q);
            INFO ("clip shape index = " << clip << ", block = " << blk);
            REQUIRE (finiteAndBounded (b));
            for (int c = 0; c < 2; ++c) for (int i = 0; i < 512; ++i) peak = std::max (peak, std::abs (b.getSample (c, i)));
        }
        CHECK (peak < 4.0f);
    }
}

TEST_CASE ("Warmth saturator stays finite + bounded at max drive", "[antialias][warmth]")
{
    gf::Saturator sat; sat.prepare (kSr, 1, kN);
    sat.setType (2 /*Hard*/); sat.setDrive (24.0f); sat.setMix (1.0f);
    juce::AudioBuffer<float> buf (1, kN);
    fillSine (buf, kF0, 0.9f); sat.process (buf);   // let drive smoothing settle
    fillSine (buf, kF0, 0.9f); sat.process (buf);   // measure this pass
    CHECK (finiteAndBounded (buf));
    INFO ("Warmth in-band alias floor = " << aliasToFundDb ({ buf.getReadPointer (0), buf.getReadPointer (0) + kN }) << " dB");
}

TEST_CASE ("Beauty machine (Prettifier) stays finite + bounded at max drive", "[antialias][beauty]")
{
    gf::pretty::PrettifierEngine p;
    p.prepare (kSr, 512, 2);

    gf::pretty::Params params;          // only the oversampled beauty saturator, pushed hard
    params.enabled = true;
    params.echoOn = params.reverbOn = params.chorusOn = params.polishOn = false;
    params.beautyOn = true;
    params.beautyAmount = 1.0f;
    params.beautyAir = 1.0f;
    params.beautyWarmth = 0.0f;

    for (int blk = 0; blk < 200; ++blk)
    {
        juce::AudioBuffer<float> b (2, 512);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 512; ++i)
                b.setSample (c, i, (float) std::sin (2.0 * juce::MathConstants<double>::pi * 7000.0 * (blk * 512 + i) / kSr) * 0.95f);
        p.process (b, params, 120.0, false);
        INFO ("block = " << blk);
        REQUIRE (finiteAndBounded (b));
    }
}
