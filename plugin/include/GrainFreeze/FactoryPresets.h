#pragma once

#include <juce_core/juce_core.h>
#include <vector>

// Built-in factory presets. Each is a name (category-prefixed so the browser
// groups them when sorted) plus a list of {paramID, real value} overrides; all
// other params reset to their defaults when the preset is applied. Values are
// REAL (not normalized) -PresetManager converts via each param's range.
namespace gf
{

struct FactoryParam { const char* id; float value; };

// A preset is its parameter overrides plus, optionally, a drawn Curve shape.
// The Curve is patch data rather than a parameter (it is a list of nodes, not a
// number), so it rides along as the same string the state tree stores.
struct FactoryPreset
{
    const char* name;
    std::vector<FactoryParam> params;
    const char* curve = nullptr;   // nullptr = leave the Curve at its default
};

// Curve shapes, in CurveSource's serialised form: count, then x,y,tension per
// node. Written out rather than computed so a preset reads as what it draws.
namespace curves
{
    // Falling ramp, then hold: the classic sidechain duck.
    inline constexpr const char* duck =
        "4,0.0000,1.0000,0.450,0.3200,-1.0000,0.000,0.8000,-1.0000,-0.400,1.0000,1.0000,0.000";
    // Hard on/off eighths.
    inline constexpr const char* gate8 =
        "6,0.0000,1.0000,0.980,0.2500,1.0000,-0.980,0.2600,-1.0000,0.980,0.5000,-1.0000,-0.980,"
        "0.5100,1.0000,0.980,1.0000,1.0000,0.000";
    // Slow swell and release.
    inline constexpr const char* swell =
        "3,0.0000,-1.0000,0.600,0.7000,1.0000,-0.550,1.0000,-1.0000,0.000";
    // Four descending steps.
    inline constexpr const char* stairs =
        "9,0.0000,1.0000,0.950,0.2500,1.0000,0.950,0.2600,0.3300,0.950,0.5000,0.3300,0.950,"
        "0.5100,-0.3300,0.950,0.7500,-0.3300,0.950,0.7600,-1.0000,0.950,0.9900,-1.0000,0.000,"
        "1.0000,1.0000,0.000";
    // One long rise across the whole loop.
    inline constexpr const char* rise =
        "3,0.0000,-1.0000,0.350,0.9200,1.0000,0.000,1.0000,-1.0000,0.000";
    // Two quick stabs, then space.
    inline constexpr const char* stabs =
        "7,0.0000,1.0000,0.900,0.1200,-1.0000,0.000,0.2500,1.0000,0.900,0.3700,-1.0000,0.000,"
        "0.9000,-1.0000,0.000,0.9700,1.0000,0.000,1.0000,1.0000,0.000";
}

// The patch applied on a fresh insert (first impression), also selectable.
inline const char* kDefaultPresetName = "Default - MK Signature";

inline const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> presets = {
        { kDefaultPresetName, {
            { "textureGrainOn", 1 }, { "beautySpaceOn", 1 }, { "grainSize", 240 }, { "density", 40 },
            { "pitch", 0 }, { "spray", 120 }, { "echoOn", 1 }, { "echoTimeMs", 320 },
            { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.7f }, { "beautySpaceMix", 0.85f },
            { "textureGrainMix", 0.85f }, { "mixOutput", 1.40f } } },

        // ---- Beautiful ----
        { "Beautiful - Angel Dust", {
            { "textureGrainOn", 1 }, { "grainSize", 600 }, { "density", 30 }, { "pitch", 12 },
            { "spray", 300 }, { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.85f },
            { "chorusOn", 1 }, { "spectralOn", 1 }, { "spectralMix", 0.3f } } },
        { "Beautiful - Glass Cathedral", {
            { "textureGrainOn", 1 }, { "grainSize", 900 }, { "density", 22 }, { "pitch", 7 },
            { "spectralOn", 1 }, { "spectralMix", 0.6f }, { "spectralAmount", 0.8f },
            { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.95f }, { "mixOutput", 1.47f } } },
        { "Beautiful - Warm Bloom", {
            { "textureGrainOn", 1 }, { "grainSize", 320 }, { "density", 55 }, { "beautySpaceOn", 1 },
            { "beautyOn", 1 }, { "beautyAmount", 0.6f }, { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.6f },
            { "damageOn", 1 }, { "damageClip", 1 }, { "damageAmount", 0.15f }, { "damageMix", 0.3f }, { "mixOutput", 0.69f } } },

        // ---- Dream ----
        { "Dream - Floating", {
            { "textureGrainOn", 1 }, { "grainSize", 800 }, { "density", 24 }, { "pitch", 12 },
            { "spray", 900 }, { "pitchJitter", 3 }, { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 },
            { "prettyReverbSize", 0.9f }, { "lfoDivision", 2 } } },
        { "Dream - Underwater", {
            { "textureGrainOn", 1 }, { "grainSize", 500 }, { "density", 35 }, { "chorusOn", 1 },
            { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.8f },
            { "damageOn", 1 }, { "damageClip", 1 }, { "damageTone", 0.35f }, { "damageMix", 0.4f } } },
        { "Dream - Halcyon", {
            { "textureGrainOn", 1 }, { "grainSize", 700 }, { "density", 28 }, { "pitch", 7 },
            { "spectralOn", 1 }, { "spectralMix", 0.4f }, { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 },
            { "prettyReverbSize", 0.75f }, { "mixOutput", 0.72f } } },

        // ---- Alien ----
        { "Alien - Transmission", {
            { "textureGrainOn", 1 }, { "grainSize", 80 }, { "density", 120 }, { "pitchJitter", 24 },
            { "spray", 200 }, { "pitchFormantOn", 1 }, { "pitchFormantMix", 0.6f }, { "pitch", -7 },
            { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.5f }, { "mixOutput", 1.56f } } },
        { "Alien - Hive Mind", {
            { "textureGrainOn", 1 }, { "grainSize", 40 }, { "density", 200 }, { "pitchJitter", 36 },
            { "spray", 600 }, { "spectralOn", 1 }, { "spectralMix", 0.45f }, { "identityLossOn", 1 },
            { "identityLoss", 0.3f } } },
        { "Alien - Probe", {
            { "textureGrainOn", 1 }, { "grainSize", 120 }, { "density", 90 }, { "pitchFormantOn", 1 },
            { "pitchFormantMix", 0.8f }, { "pitch", 19 }, { "timeBreakerOn", 1 }, { "timeBreakerMix", 0.5f },
            { "timeBreakerSync", 1 }, { "timeBreakerDivision", 5 }, { "stutterChance", 0.5f }, { "mixOutput", 2.00f } } },

        // ---- Destroyed ----
        { "Destroyed - Meltdown", {
            { "textureGrainOn", 1 }, { "grainSize", 120 }, { "density", 70 }, { "damageOn", 1 },
            { "damageClip", 2 }, { "damageAmount", 0.8f }, { "damageBits", 4 }, { "damageDropout", 0.3f },
            { "damageMix", 0.9f }, { "beautySpaceOn", 0 }, { "mixOutput", 0.64f } } },
        { "Destroyed - Static Crush", {
            { "textureGrainOn", 1 }, { "grainSize", 200 }, { "density", 50 }, { "damageOn", 1 },
            { "damageClip", 0 }, { "damageAmount", 0.6f }, { "damageBits", 2 }, { "damageRate", 16 },
            { "damageNoise", 0.4f }, { "damageMix", 1.0f }, { "beautySpaceOn", 0 }, { "mixOutput", 1.73f } } },
        { "Destroyed - Shrapnel", {
            { "textureGrainOn", 1 }, { "grainSize", 90 }, { "density", 100 }, { "damageOn", 1 },
            { "damageClip", 3 }, { "damageAmount", 0.7f }, { "damageBits", 6 }, { "timeBreakerOn", 1 },
            { "timeBreakerMix", 0.7f }, { "timeBreakerSync", 1 }, { "timeBreakerDivision", 6 },
            { "stutterChance", 0.7f }, { "reverseChance", 0.4f }, { "mixOutput", 1.36f } } },

        // ---- Cinematic ----
        { "Cinematic - Rise", {
            { "textureGrainOn", 1 }, { "grainSize", 700 }, { "density", 26 }, { "pitch", 12 },
            { "spray", 500 }, { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.92f },
            { "lfoDivision", 1 } } },
        { "Cinematic - Tension", {
            { "textureGrainOn", 1 }, { "grainSize", 300 }, { "density", 40 }, { "pitch", -12 },
            { "spectralOn", 1 }, { "spectralMix", 0.5f }, { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 },
            { "prettyReverbSize", 0.85f }, { "damageOn", 1 }, { "damageClip", 1 }, { "damageTone", 0.3f },
            { "damageMix", 0.3f } } },
        { "Cinematic - Impact Tail", {
            { "textureGrainOn", 1 }, { "grainSize", 1000 }, { "density", 20 }, { "spectralOn", 1 },
            { "spectralMix", 0.7f }, { "spectralAmount", 0.9f }, { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 },
            { "prettyReverbSize", 1.0f }, { "mixOutput", 2.00f } } },

        // ---- Identity Loss ----
        { "Identity Loss - Dissolve", {
            { "textureGrainOn", 1 }, { "identityLossOn", 1 }, { "identityLoss", 0.7f }, { "identityLossMix", 1.0f },
            { "grainSize", 300 }, { "density", 60 }, { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 },
            { "prettyReverbSize", 0.7f } } },
        { "Identity Loss - Erased", {
            { "textureGrainOn", 1 }, { "identityLossOn", 1 }, { "identityLoss", 0.92f }, { "identityLossMix", 1.0f },
            { "grainSize", 180 }, { "density", 90 }, { "spectralOn", 1 }, { "spectralMix", 0.5f } } },
        { "Identity Loss - Phantom", {
            { "textureGrainOn", 1 }, { "identityLossOn", 1 }, { "identityLoss", 0.5f }, { "pitch", 7 },
            { "pitchFormantOn", 1 }, { "pitchFormantMix", 0.4f }, { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 },
            { "prettyReverbSize", 0.8f }, { "mixOutput", 1.66f } } },

        // ---- Drums ---- (tuned for transient/percussive material: short grains,
        // ducker keeps the original punch, multiband damage targets hihats not kick)
        { "Drums - Tape Saturated", {
            // Warm tape colour + a gentle granular texture layered behind the dry. Ducker keeps the kick clear.
            { "textureGrainOn", 1 }, { "grainSize", 40 }, { "density", 80 }, { "spray", 30 },
            { "textureGrainMix", 0.45f },
            { "damageOn", 1 }, { "damageClip", 1 }, { "damageAmount", 0.35f }, { "damageMix", 0.7f },
            { "duckOn", 1 }, { "duckAmount", 0.5f }, { "duckThreshold", 0.15f }, { "duckAttack", 5.0f }, { "duckRelease", 120.0f },
            { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.35f }, { "beautySpaceMix", 0.6f } } },

        { "Drums - Destroyed Top", {
            // Multiband damage hammers the highs (hats/crash) while the low band stays clean.
            { "textureGrainOn", 1 }, { "grainSize", 30 }, { "density", 110 }, { "textureGrainMix", 0.35f },
            { "damageOn", 1 }, { "damageClip", 3 /*Fold*/ }, { "damageAmount", 0.25f }, { "damageMix", 0.9f },
            { "damageSplitOn", 1 }, { "damageSplitHz", 1200.0f }, { "damageHighAmount", 0.9f },
            { "duckOn", 1 }, { "duckAmount", 0.45f }, { "duckRelease", 90.0f },
            { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.3f }, { "mixOutput", 0.50f } } },

        { "Drums - Stutter Fill", {
            // Time Breaker on 1/16 stutter for fills; mostly dry with rhythmic glitches.
            { "textureGrainOn", 1 }, { "grainSize", 25 }, { "density", 120 }, { "textureGrainMix", 0.3f },
            { "timeBreakerOn", 1 }, { "timeBreakerSync", 1 }, { "timeBreakerDivision", 5 /*1/16*/ },
            { "stutterChance", 0.6f }, { "stutterSize", 0.5f }, { "reverseChance", 0.2f }, { "timeBreakerMix", 0.75f },
            { "duckOn", 1 }, { "duckAmount", 0.4f }, { "duckAttack", 3.0f }, { "duckRelease", 60.0f },
            { "beautySpaceOn", 1 }, { "echoOn", 1 }, { "echoMix", 0.15f }, { "echoFeedback", 0.25f },
            { "beautySpaceMix", 0.55f }, { "mixOutput", 2.00f } } },

        { "Drums - Lo-Fi Dusty", {
            // Heavy bit-crush + SR reduction + dropouts for a dusty boombap / sampler vibe.
            { "textureGrainOn", 1 }, { "grainSize", 60 }, { "density", 60 }, { "textureGrainMix", 0.5f },
            { "damageOn", 1 }, { "damageClip", 1 /*Tape*/ }, { "damageAmount", 0.2f },
            { "damageBits", 9.0f }, { "damageRate", 8.0f }, { "damageNoise", 0.12f }, { "damageDropout", 0.05f },
            { "damageTone", 0.5f }, { "damageMix", 0.85f },
            { "beautySpaceOn", 1 }, { "crushOn", 1 }, { "crushBits", 12.0f }, { "crushMix", 0.65f },
            { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.3f }, { "prettyReverbDamping", 0.7f } } },

        { "Drums - Sidechain Pump", {
            // Big lush reverb on the WET, ducker hammers it down so the dry kick punches through -> the classic
            // ambient/pump bloom that opens and closes with the groove.
            { "textureGrainOn", 1 }, { "grainSize", 80 }, { "density", 50 }, { "textureGrainMix", 0.55f },
            { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.9f }, { "prettyReverbDamping", 0.3f },
            { "prettyReverbMix", 0.55f },
            { "duckOn", 1 }, { "duckAmount", 0.85f }, { "duckThreshold", 0.08f }, { "duckAttack", 2.0f }, { "duckRelease", 220.0f },
            { "beautySpaceMix", 0.8f } } },

        { "Drums - Shimmer Wash", {
            // Angel octave-up + Convolve-friendly wide reverb on the wet, ducked so the original hits stay sharp.
            { "textureGrainOn", 1 }, { "grainSize", 50 }, { "density", 70 }, { "textureGrainMix", 0.4f },
            { "beautySpaceOn", 1 }, { "angelOn", 1 }, { "angelMix", 0.45f },
            { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.85f }, { "prettyReverbMix", 0.4f },
            { "duckOn", 1 }, { "duckAmount", 0.6f }, { "duckThreshold", 0.1f }, { "duckRelease", 180.0f },
            { "beautySpaceMix", 0.7f } } },

        // ---- Synth ---- (the core use case: pads, leads, plucks get layered grain texture)
        { "Synth - Pad Bloom", {
            { "textureGrainOn", 1 }, { "grainSize", 360 }, { "density", 35 }, { "spray", 200 },
            { "textureGrainMix", 0.7f },
            { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.85f }, { "prettyReverbMix", 0.4f },
            { "chorusOn", 1 }, { "chorusMix", 0.25f } } },

        { "Synth - Wide Lead", {
            { "textureGrainOn", 1 }, { "grainSize", 90 }, { "density", 70 }, { "spray", 80 },
            { "textureGrainMix", 0.5f }, { "spread", 0.85f },
            { "beautySpaceOn", 1 }, { "chorusOn", 1 }, { "chorusDepth", 0.55f }, { "chorusMix", 0.35f },
            { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.4f }, { "prettyReverbMix", 0.18f }, { "mixOutput", 1.35f } } },

        { "Synth - Pluck Cloud", {
            { "textureGrainOn", 1 }, { "grainSize", 70 }, { "density", 90 }, { "spray", 50 },
            { "textureGrainMix", 0.55f },
            { "duckOn", 1 }, { "duckAmount", 0.4f }, { "duckRelease", 120.0f },
            { "beautySpaceOn", 1 }, { "echoOn", 1 }, { "echoMix", 0.22f }, { "echoFeedback", 0.4f },
            { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.55f }, { "mixOutput", 1.33f } } },

        { "Synth - Detuned Choir", {
            { "textureGrainOn", 1 }, { "grainSize", 240 }, { "density", 50 }, { "pitchJitter", 0.4f },
            { "textureGrainMix", 0.65f },
            { "beautySpaceOn", 1 }, { "chorusOn", 1 }, { "chorusDepth", 0.7f }, { "chorusMix", 0.4f },
            { "harmonyOn", 1 }, { "harmonyMix", 0.25f },
            { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.75f }, { "mixOutput", 1.29f } } },

        { "Synth - Movement Loop", {
            // Tempo-synced density rides the 1/8 grid, perfect under arps.
            { "textureGrainOn", 1 }, { "grainSize", 80 }, { "densityDivision", 4 /* 1/8 */ },
            { "textureGrainMix", 0.6f },
            { "beautySpaceOn", 1 }, { "echoOn", 1 }, { "echoMix", 0.18f }, { "echoDivision", 6 /* 1/16 */ },
            { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.5f } } },

        { "Synth - Glass Pluck", {
            { "textureGrainOn", 1 }, { "grainSize", 50 }, { "density", 80 }, { "pitch", 12 },
            { "textureGrainMix", 0.5f },
            { "beautySpaceOn", 1 }, { "angelOn", 1 }, { "angelMix", 0.5f },
            { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.7f } } },

        // ---- Vocal ---- (designed for vocal sources -- Pitch Lock, formant care, ducker keeps lyrics clear)
        { "Vocal - Heaven Stack", {
            { "textureGrainOn", 1 }, { "grainSize", 180 }, { "density", 50 }, { "textureGrainMix", 0.45f },
            { "duckOn", 1 }, { "duckAmount", 0.55f }, { "duckRelease", 200.0f },
            { "beautySpaceOn", 1 }, { "angelOn", 1 }, { "angelMix", 0.4f },
            { "harmonyOn", 1 }, { "harmonyMix", 0.3f },
            { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.8f }, { "prettyReverbMix", 0.35f }, { "mixOutput", 0.77f } } },

        { "Vocal - Whisper Cloud", {
            { "textureGrainOn", 1 }, { "grainSize", 110 }, { "density", 90 }, { "spray", 120 },
            { "textureGrainMix", 0.7f }, { "spread", 0.8f },
            { "duckOn", 1 }, { "duckAmount", 0.4f }, { "duckRelease", 160.0f },
            { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.6f } } },

        { "Vocal - Formant Doubler", {
            { "textureGrainOn", 1 }, { "grainSize", 120 }, { "density", 60 },
            { "textureGrainMix", 0.4f },
            { "pitchFormantOn", 1 }, { "pitchFormantMix", 0.5f }, { "pitch", -5 }, { "pitchLockFormant", 1 },
            { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.55f }, { "mixOutput", 1.82f } } },

        { "Vocal - Reverse Halo", {
            { "textureGrainOn", 1 }, { "grainSize", 280 }, { "density", 40 },
            { "textureGrainMix", 0.55f },
            { "timeBreakerOn", 1 }, { "timeBreakerSync", 1 }, { "timeBreakerDivision", 3 /*1/4*/ },
            { "reverseChance", 0.45f }, { "stutterChance", 0.15f }, { "timeBreakerMix", 0.35f },
            { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.75f } } },

        { "Vocal - Tape Texture", {
            { "textureGrainOn", 1 }, { "grainSize", 80 }, { "density", 70 }, { "textureGrainMix", 0.45f },
            { "damageOn", 1 }, { "damageClip", 1 /*Tape*/ }, { "damageAmount", 0.25f }, { "damageMix", 0.6f },
            { "damageNoise", 0.05f }, { "damageTone", 0.6f },
            { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.5f } } },

        // ---- Field ---- (texture-on-field-recording: ambient, drones, sound design)
        { "Field - Soft Wind", {
            { "textureGrainOn", 1 }, { "grainSize", 320 }, { "density", 30 }, { "spray", 250 },
            { "textureGrainMix", 0.7f }, { "spread", 0.7f },
            { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.9f }, { "prettyReverbMix", 0.3f } } },

        { "Field - Distant Choir", {
            { "textureGrainOn", 1 }, { "grainSize", 500 }, { "density", 25 }, { "pitchJitter", 0.2f },
            { "textureGrainMix", 0.65f }, { "pitch", 7 },
            { "beautySpaceOn", 1 }, { "angelOn", 1 }, { "angelMix", 0.35f },
            { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.92f } } },

        { "Field - Tape Ruined", {
            { "textureGrainOn", 1 }, { "grainSize", 150 }, { "density", 50 }, { "textureGrainMix", 0.6f },
            { "damageOn", 1 }, { "damageClip", 1 /*Tape*/ }, { "damageAmount", 0.4f }, { "damageMix", 0.85f },
            { "damageBits", 11.0f }, { "damageNoise", 0.18f }, { "damageDropout", 0.1f }, { "damageTone", 0.5f },
            { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.6f }, { "mixOutput", 0.65f } } },

        { "Field - Glacier Drift", {
            { "textureGrainOn", 1 }, { "grainSize", 600 }, { "density", 15 }, { "spray", 400 },
            { "textureGrainMix", 0.8f }, { "spread", 0.95f }, { "pitch", -7 },
            { "beautySpaceOn", 1 }, { "dreamOn", 1 }, { "dreamMix", 0.45f },
            { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.95f }, { "prettyReverbMix", 0.45f } } },

        { "Field - Insect Garden", {
            { "textureGrainOn", 1 }, { "grainSize", 40 }, { "density", 140 }, { "spray", 60 },
            { "textureGrainMix", 0.65f }, { "pitchJitter", 0.6f }, { "spread", 0.9f },
            { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.7f } } },

        { "Field - Underwater Cave", {
            { "textureGrainOn", 1 }, { "grainSize", 240 }, { "density", 40 }, { "pitch", -12 },
            { "textureGrainMix", 0.7f },
            { "beautySpaceOn", 1 }, { "phaserOn", 1 }, { "phaserMix", 0.35f },
            { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.88f }, { "prettyReverbDamping", 0.65f } } },

        // ---- Bass ---- (sub emphasis, multiband Damage destroys highs not low)
        { "Bass - Subterranean", {
            { "textureGrainOn", 1 }, { "grainSize", 200 }, { "density", 40 }, { "pitch", -12 },
            { "textureGrainMix", 0.4f }, { "spread", 0.2f },
            { "damageOn", 1 }, { "damageClip", 1 /*Tape*/ }, { "damageAmount", 0.3f }, { "damageMix", 0.7f },
            { "damageSplitOn", 1 }, { "damageSplitHz", 600.0f }, { "damageHighAmount", 0.6f },
            { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.3f }, { "mixOutput", 0.78f } } },

        { "Bass - Reese Texture", {
            { "textureGrainOn", 1 }, { "grainSize", 100 }, { "density", 80 }, { "pitchJitter", 0.5f },
            { "textureGrainMix", 0.55f }, { "spread", 0.75f },
            { "damageOn", 1 }, { "damageClip", 0 /*Tube*/ }, { "damageAmount", 0.4f }, { "damageMix", 0.75f },
            { "damageSplitOn", 1 }, { "damageSplitHz", 800.0f }, { "damageHighAmount", 0.75f },
            { "beautySpaceOn", 1 }, { "chorusOn", 1 }, { "chorusMix", 0.2f }, { "mixOutput", 0.74f } } },

        { "Bass - Granular Drone", {
            { "textureGrainOn", 1 }, { "grainSize", 480 }, { "density", 20 }, { "pitch", -7 },
            { "textureGrainMix", 0.75f }, { "spread", 0.4f },
            { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.85f }, { "prettyReverbDamping", 0.7f } } },

        // ---- Air ---- (the AIR page: tonal toys on the top band, lows untouched)
        { "Air - Acid Squelch", {
            // Resonant bandpass riding the envelope + a little exciter growl: the 303 move, on any source.
            { "textureGrainOn", 1 }, { "grainSize", 90 }, { "density", 60 }, { "textureGrainMix", 0.4f },
            { "airOn", 1 }, { "airCrossover", 1800.0f }, { "airMix", 0.85f },
            { "airSquelchOn", 1 }, { "airSquelchMode", 1 }, { "airSquelchHz", 2400.0f },
            { "airSquelchRes", 0.82f }, { "airSquelchEnv", 0.6f },
            { "airExciterOn", 1 }, { "airExciterDrive", 0.45f }, { "airExciterMix", 0.5f },
            { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.35f }, { "mixOutput", 1.29f } } },

        { "Air - Silk Lift", {
            // Dynamic shelf lifts air only when there's energy; gentle harmonics. Mastering-grade sheen.
            { "textureGrainOn", 1 }, { "grainSize", 200 }, { "density", 30 }, { "textureGrainMix", 0.3f },
            { "airOn", 1 }, { "airCrossover", 4000.0f }, { "airMix", 0.7f },
            { "airShelfOn", 1 }, { "airShelfHz", 9000.0f }, { "airShelfAmount", 0.55f }, { "airShelfThreshold", 0.15f },
            { "airExciterOn", 1 }, { "airExciterDrive", 0.2f }, { "airExciterMix", 0.35f },
            { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.5f }, { "mixOutput", 1.27f } } },

        { "Air - Comb Tone", {
            // Short high-feedback delay on the top tunes a comb pitch into the note; slow phaser keeps it moving.
            { "textureGrainOn", 1 }, { "grainSize", 120 }, { "density", 45 }, { "textureGrainMix", 0.45f },
            { "airOn", 1 }, { "airCrossover", 2200.0f }, { "airMix", 0.8f },
            { "airDelayOn", 1 }, { "airDelayMs", 4.5f }, { "airDelayFeedback", 0.86f }, { "airDelayMix", 0.7f },
            { "airPhaserOn", 1 }, { "airPhaserRate", 0.12f }, { "airPhaserDepth", 0.7f }, { "airPhaserMix", 0.45f },
            { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.6f } } },

        { "Bass - Wobble Crush", {
            { "textureGrainOn", 1 }, { "grainSize", 60 }, { "density", 90 }, { "textureGrainMix", 0.45f },
            { "damageOn", 1 }, { "damageClip", 3 /*Fold*/ }, { "damageAmount", 0.5f }, { "damageMix", 0.85f },
            { "damageBits", 6.0f }, { "damageMix", 0.85f },
            { "duckOn", 1 }, { "duckAmount", 0.5f }, { "duckRelease", 100.0f }, { "mixOutput", 0.50f } } },

        // ================================================================
        // LOCKED - grains spawned on the hits in the incoming audio rather
        // than from a clock. This is the thing no other granular does: on a
        // loop the texture stays inside the groove instead of smearing it.
        // Play these on drums, percussion loops, or a rhythmic vocal.
        // ================================================================
        { "Locked - Ghost Hats", {
            // Short grains on every hit including the quiet ones, lifted into the air band.
            { "textureGrainOn", 1 }, { "grainTrigger", 1 }, { "transientSense", 0.82f },
            { "transientGrains", 2 }, { "grainSize", 46 }, { "grainShape", 0.18f },
            { "pitch", 12 }, { "spray", 40 }, { "spread", 0.75f }, { "textureGrainMix", 0.55f },
            { "airOn", 1 }, { "airCrossover", 4200.0f }, { "airMix", 0.55f },
            { "airExciterOn", 1 }, { "airExciterDrive", 0.45f }, { "airExciterMix", 0.6f },
            { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.35f },
            { "beautySpaceMix", 0.4f }, { "dryLevel", 0.9f },
            { "mixOutput", 0.67f } } },

        { "Locked - Snare Bloom", {
            // Each hit opens into a pitched cloud that decays with the drum.
            { "textureGrainOn", 1 }, { "grainTrigger", 1 }, { "transientSense", 0.45f },
            { "transientGrains", 6 }, { "grainSize", 260 }, { "grainShape", 0.88f },
            { "pitch", 7 }, { "pitchJitter", 5 }, { "spray", 260 }, { "spread", 0.9f },
            { "textureGrainMix", 0.7f },
            { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.8f },
            { "duckOn", 1 }, { "duckAmount", 0.7f }, { "duckAttack", 3.0f }, { "duckRelease", 220.0f },
            { "dryLevel", 0.85f }, { "mixOutput", 0.61f } } },

        { "Locked - Broken Toy", {
            // Transient-locked and destroyed: every hit arrives already broken.
            { "textureGrainOn", 1 }, { "grainTrigger", 1 }, { "transientSense", 0.6f },
            { "transientGrains", 3 }, { "grainSize", 80 }, { "grainShape", 0.12f },
            { "pitch", -5 }, { "pitchJitter", 9 }, { "spray", 90 }, { "textureGrainMix", 0.6f },
            { "damageOn", 1 }, { "damageClip", 2 /*Hard*/ }, { "damageAmount", 0.45f },
            { "damageBits", 5.0f }, { "damageRate", 8.0f }, { "damageMix", 0.8f },
            { "dryLevel", 0.75f }, { "mixOutput", 0.30f } } },

        { "Locked - Iron Lung", {
            // Big slow grains on the hits, with the Ducker letting the source breathe
            // back through the texture -- the pairing this plugin was built around.
            { "textureGrainOn", 1 }, { "grainTrigger", 1 }, { "transientSense", 0.35f },
            { "transientGrains", 5 }, { "grainSize", 720 }, { "grainShape", 0.62f },
            { "pitch", -12 }, { "spray", 500 }, { "spread", 0.85f }, { "textureGrainMix", 0.8f },
            { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.92f },
            { "prettyReverbDamping", 0.55f },
            { "duckOn", 1 }, { "duckAmount", 0.85f }, { "duckThreshold", 0.06f },
            { "duckAttack", 2.0f }, { "duckRelease", 420.0f },
            { "dryLevel", 1.0f }, { "mixOutput", 0.59f } } },

        { "Locked - Tape Skip", {
            // Hits trigger grains; the Time Breaker throws slices of them backwards.
            { "textureGrainOn", 1 }, { "grainTrigger", 1 }, { "transientSense", 0.55f },
            { "transientGrains", 4 }, { "grainSize", 150 }, { "spray", 120 }, { "textureGrainMix", 0.65f },
            { "timeBreakerOn", 1 }, { "timeBreakerMix", 0.8f }, { "timeBreakerSync", 1 },
            { "timeBreakerDivision", 5 /*1/16*/ }, { "stutterChance", 0.45f }, { "reverseChance", 0.5f },
            { "damageOn", 1 }, { "damageClip", 1 /*Tape*/ }, { "damageAmount", 0.28f }, { "damageMix", 0.5f },
            { "dryLevel", 0.6f }, { "mixOutput", 0.50f } } },

        // ================================================================
        // DRAWN - the Curve is doing the work. Open the play surface to see
        // the shape each one is drawing, and drag its points to reshape the
        // sound. Everything here is locked to the host timeline.
        // ================================================================
        { "Drawn - Pump", {
            // The shape ducks the wet on the beat and lets it swell back between.
            { "textureGrainOn", 1 }, { "grainSize", 300 }, { "density", 45 }, { "spray", 200 },
            { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.75f },
            { "echoOn", 1 }, { "echoTimeMs", 375 }, { "echoMix", 0.3f },
            { "dryWet", 0.62f }, { "curveBars", 2 /*1 Bar*/ }, { "curveSync", 1 },
            { "modSlot1Source", 21 /*Curve*/ }, { "modSlot1Target", 26 /*Dry/Wet*/ }, { "modSlot1Depth", 0.8f },
            { "mixOutput", 0.73f } }, curves::duck },

        { "Drawn - Gate Cathedral", {
            // A huge reverb chopped into eighths by the drawn gate.
            { "textureGrainOn", 1 }, { "grainSize", 800 }, { "density", 26 }, { "pitch", 12 },
            { "spray", 700 }, { "spread", 0.95f }, { "output", 0.75f },
            { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.96f },
            { "beautySpaceMix", 0.9f },
            { "curveBars", 2 /*1 Bar*/ }, { "curveSync", 1 },
            { "modSlot1Source", 21 }, { "modSlot1Target", 8 /*Output*/ }, { "modSlot1Depth", 1.0f },
            { "mixOutput", 0.68f } }, curves::gate8 },

        { "Drawn - Rising Tide", {
            // Four bars of the grain cloud thickening, then dropping away. A riser
            // you can redraw instead of automating.
            { "textureGrainOn", 1 }, { "grainSize", 180 }, { "density", 90 }, { "pitch", 0 },
            { "spray", 400 }, { "spread", 0.8f }, { "textureGrainMix", 0.8f },
            { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.9f },
            { "curveBars", 4 /*4 Bars*/ }, { "curveSync", 1 },
            { "modSlot1Source", 21 }, { "modSlot1Target", 2 /*Density*/ }, { "modSlot1Depth", 0.55f },
            { "modSlot2Source", 21 }, { "modSlot2Target", 3 /*Pitch*/ }, { "modSlot2Depth", 0.18f },
            { "mixOutput", 1.06f } }, curves::rise },

        { "Drawn - Filter Steps", {
            // The drawn staircase walks the Squelch filter down the top band.
            { "textureGrainOn", 1 }, { "grainSize", 120 }, { "density", 70 }, { "spray", 150 },
            { "airOn", 1 }, { "airCrossover", 1400.0f }, { "airMix", 0.9f },
            { "airSquelchOn", 1 }, { "airSquelchMode", 1 /*Band*/ }, { "airSquelchHz", 5000.0f },
            { "airSquelchRes", 0.72f },
            { "curveBars", 2 }, { "curveSync", 1 },
            { "modSlot1Source", 21 }, { "modSlot1Target", 24 /*Squelch Cutoff*/ }, { "modSlot1Depth", 0.62f },
            { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.5f },
            { "mixOutput", 1.09f } }, curves::stairs },

        { "Drawn - Stabs", {
            // Two quick hits of destruction per bar, drawn rather than automated.
            { "textureGrainOn", 1 }, { "grainSize", 90 }, { "density", 80 }, { "spray", 60 },
            { "damageOn", 1 }, { "damageClip", 4 /*Diode*/ }, { "damageAmount", 0.15f },
            { "damageBits", 16.0f }, { "damageMix", 0.9f },
            { "curveBars", 2 }, { "curveSync", 1 },
            { "modSlot1Source", 21 }, { "modSlot1Target", 20 /*Damage Amount*/ }, { "modSlot1Depth", 0.85f },
            { "beautySpaceOn", 1 }, { "echoOn", 1 }, { "echoTimeMs", 250 }, { "echoMix", 0.25f },
            { "mixOutput", 0.87f } }, curves::stabs },

        // ================================================================
        // MOTION - the matrix's own generators: the step sequencer, the
        // second LFO, the random sample & hold, and the mod wheel.
        // ================================================================
        { "Motion - Stutter Grid", {
            // The step sequencer opens and closes the Time Breaker on a 1/16 grid.
            { "textureGrainOn", 1 }, { "grainSize", 140 }, { "density", 60 }, { "spray", 120 },
            { "timeBreakerOn", 1 }, { "timeBreakerMix", 0.9f }, { "timeBreakerSync", 1 },
            { "timeBreakerDivision", 5 /*1/16*/ }, { "stutterChance", 0.3f },
            { "stepSeqDivision", 5 /*1/16*/ }, { "stepSeqLength", 8 }, { "stepSeqSmooth", 0.0f },
            { "modSlot1Source", 14 /*Step Seq*/ }, { "modSlot1Target", 28 /*Stutter Chance*/ },
            { "modSlot1Depth", 0.85f },
            { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.45f },
            { "mixOutput", 1.30f } } },

        { "Motion - Dice", {
            // A random value every few beats, thrown at the grain pitch.
            { "textureGrainOn", 1 }, { "grainSize", 200 }, { "density", 38 }, { "spray", 250 },
            { "spread", 0.85f }, { "modRandomRate", 3.5f },
            { "modSlot1Source", 15 /*Random*/ }, { "modSlot1Target", 3 /*Pitch*/ }, { "modSlot1Depth", 0.16f },
            { "modSlot2Source", 15 }, { "modSlot2Target", 1 /*Grain Size*/ }, { "modSlot2Depth", 0.25f },
            { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.8f },
            { "echoOn", 1 }, { "echoTimeMs", 500 }, { "echoFeedback", 0.45f }, { "echoMix", 0.3f },
            { "mixOutput", 0.91f } } },

        { "Motion - Slow Tide", {
            // LFO 2 on a one-bar cycle, opening the stereo field and the air band
            // together so the sound breathes sideways.
            { "textureGrainOn", 1 }, { "grainSize", 520 }, { "density", 30 }, { "spray", 450 },
            { "spread", 0.7f }, { "lfo2Sync", 1 /*1/1*/ }, { "lfo2Shape", 0 },
            { "modSlot1Source", 13 /*LFO 2*/ }, { "modSlot1Target", 27 /*Mix Width*/ }, { "modSlot1Depth", 0.45f },
            { "modSlot2Source", 13 }, { "modSlot2Target", 23 /*Air Mix*/ }, { "modSlot2Depth", 0.5f },
            { "airOn", 1 }, { "airCrossover", 3000.0f }, { "airMix", 0.5f },
            { "airShelfOn", 1 }, { "airShelfAmount", 0.4f },
            { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.88f },
            { "mixOutput", 0.95f } } },

        { "Motion - Wheel Wrecker", {
            // Nothing happens until you push the mod wheel; then it falls apart.
            { "textureGrainOn", 1 }, { "grainSize", 170 }, { "density", 55 }, { "spray", 180 },
            { "damageOn", 1 }, { "damageClip", 0 /*Tube*/ }, { "damageAmount", 0.08f },
            { "damageBits", 16.0f }, { "damageMix", 0.85f },
            { "modSlot1Source", 18 /*Mod Wheel*/ }, { "modSlot1Target", 20 /*Damage Amount*/ },
            { "modSlot1Depth", 0.9f },
            { "modSlot2Source", 18 }, { "modSlot2Target", 21 /*Damage Bits*/ }, { "modSlot2Depth", -0.8f },
            { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.6f },
            { "mixOutput", 1.03f } } },

        // ================================================================
        // AIR - more of the top-band page, which had only three.
        // ================================================================
        { "Air - Glass Rails", {
            // A resonant band sliding on the input envelope, with a tuned comb above it.
            { "textureGrainOn", 1 }, { "grainSize", 90 }, { "density", 65 }, { "textureGrainMix", 0.4f },
            { "airOn", 1 }, { "airCrossover", 1800.0f }, { "airMix", 0.85f },
            { "airSquelchOn", 1 }, { "airSquelchMode", 1 }, { "airSquelchHz", 3200.0f },
            { "airSquelchRes", 0.88f }, { "airSquelchEnv", 0.7f },
            { "airDelayOn", 1 }, { "airDelayMs", 7.0f }, { "airDelayFeedback", 0.78f }, { "airDelayMix", 0.5f },
            { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.7f },
            { "mixOutput", 1.19f } } },

        { "Air - Static Halo", {
            // Exciter and dynamic shelf together: a bright ring that only appears
            // on the loud parts, swept slowly by the phaser.
            { "textureGrainOn", 1 }, { "grainSize", 380 }, { "density", 34 }, { "pitch", 12 },
            { "spray", 300 }, { "textureGrainMix", 0.5f },
            { "airOn", 1 }, { "airCrossover", 5000.0f }, { "airMix", 0.7f },
            { "airExciterOn", 1 }, { "airExciterDrive", 0.7f }, { "airExciterMix", 0.65f },
            { "airShelfOn", 1 }, { "airShelfHz", 9000.0f }, { "airShelfAmount", 0.62f },
            { "airShelfThreshold", 0.18f },
            { "airPhaserOn", 1 }, { "airPhaserRate", 0.08f }, { "airPhaserDepth", 0.8f }, { "airPhaserMix", 0.4f },
            { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.85f },
            { "mixOutput", 1.09f } } },

        { "Air - Breath", {
            // Barely there: the top band opens with the player and closes again.
            { "textureGrainOn", 1 }, { "grainSize", 300 }, { "density", 28 }, { "textureGrainMix", 0.3f },
            { "airOn", 1 }, { "airCrossover", 6500.0f }, { "airMix", 0.42f },
            { "airSquelchOn", 1 }, { "airSquelchMode", 2 /*High*/ }, { "airSquelchHz", 7000.0f },
            { "airSquelchRes", 0.3f }, { "airSquelchEnv", 0.85f },
            { "airShelfOn", 1 }, { "airShelfHz", 11000.0f }, { "airShelfAmount", 0.35f },
            { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.55f },
            { "dryLevel", 1.0f }, { "mixOutput", 0.55f } } },
    };
    return presets;
}

} // namespace gf
