// Save -> load round-trip. The bug users remember is a session that reloads
// wrong; this pins every parameter, the four morph snapshots and the
// convolution IR path across getStateInformation / setStateInformation.
#include <catch2/catch_test_macros.hpp>
#include "ProcessorFixture.h"

TEST_CASE ("State round-trips every parameter, morph slots and IR path", "[state]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    GrainFreezeProcessor a;
    a.presets.loadPreset ("Air - Acid Squelch");
    // Poke a spread of "unusual" values across subsystems added at different times.
    fx::setParam (a, "grainShape",      0.83f);
    fx::setParam (a, "modSlot2Source",  12.0f);   // Input Env
    fx::setParam (a, "modSlot2Target",  7.0f);    // Pitch Jitter
    fx::setParam (a, "modSlot2Depth",  -0.42f);
    fx::setParam (a, "airSquelchRes",   0.91f);
    fx::setParam (a, "damageSplitHz",   3210.0f);
    fx::setParam (a, "duckRelease",     640.0f);
    fx::setParam (a, "mpeOn",           1.0f);
    fx::setParam (a, "densityDivision", 4.0f);
    fx::setParam (a, "macroMorph",      0.31f);
    fx::setParam (a, "macroMorphY",     0.77f);
    // Morph corners: capture four distinct states.
    a.storeSlotA();
    fx::setParam (a, "grainSize", 900.0f);  a.storeSlotB();
    fx::setParam (a, "density",   150.0f);  a.storeSlotC();
    fx::setParam (a, "pitch",     -12.0f);  a.storeSlotD();
    a.loadConvolutionIR (juce::File ("/nonexistent/but/remembered/hall.wav"));   // path is remembered even if missing

    juce::MemoryBlock blob;
    a.getStateInformation (blob);
    REQUIRE (blob.getSize() > 0);

    GrainFreezeProcessor b;
    b.setStateInformation (blob.getData(), (int) blob.getSize());

    // Every parameter, by ID, normalised value.
    const auto& pa = a.getParameters();
    const auto& pb = b.getParameters();
    REQUIRE (pa.size() == pb.size());
    int checked = 0;
    for (int i = 0; i < pa.size(); ++i)
    {
        auto* x = dynamic_cast<juce::RangedAudioParameter*> (pa[i]);
        auto* y = dynamic_cast<juce::RangedAudioParameter*> (pb[i]);
        REQUIRE (x != nullptr); REQUIRE (y != nullptr);
        INFO ("param " << x->paramID);
        REQUIRE (x->paramID == y->paramID);
        CHECK (std::abs (x->getValue() - y->getValue()) < 1.0e-5f);
        ++checked;
    }
    CHECK (checked > 250);

    // Morph slots survive (each child of the state tree named AB_SLOT_*).
    for (auto* slot : { "AB_SLOT_A", "AB_SLOT_B", "AB_SLOT_C", "AB_SLOT_D" })
    {
        INFO (slot);
        auto sa = a.getSlotTree (slot), sb = b.getSlotTree (slot);
        REQUIRE (sa.isValid()); REQUIRE (sb.isValid());
        CHECK (sa.isEquivalentTo (sb));
    }
    CHECK (b.getConvolutionIRPath() == "/nonexistent/but/remembered/hall.wav");
}
