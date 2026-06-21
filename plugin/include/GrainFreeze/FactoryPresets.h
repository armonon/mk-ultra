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
struct FactoryPreset { const char* name; std::vector<FactoryParam> params; };

// The patch applied on a fresh insert (first impression), also selectable.
inline const char* kDefaultPresetName = "Default - MK Signature";

inline const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> presets = {
        { kDefaultPresetName, {
            { "textureGrainOn", 1 }, { "beautySpaceOn", 1 }, { "grainSize", 240 }, { "density", 40 },
            { "pitch", 0 }, { "spray", 120 }, { "echoOn", 1 }, { "echoTimeMs", 320 },
            { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.7f }, { "beautySpaceMix", 0.85f },
            { "textureGrainMix", 0.85f } } },

        // ---- Beautiful ----
        { "Beautiful - Angel Dust", {
            { "textureGrainOn", 1 }, { "grainSize", 600 }, { "density", 30 }, { "pitch", 12 },
            { "spray", 300 }, { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.85f },
            { "chorusOn", 1 }, { "spectralOn", 1 }, { "spectralMix", 0.3f } } },
        { "Beautiful - Glass Cathedral", {
            { "textureGrainOn", 1 }, { "grainSize", 900 }, { "density", 22 }, { "pitch", 7 },
            { "spectralOn", 1 }, { "spectralMix", 0.6f }, { "spectralAmount", 0.8f },
            { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.95f } } },
        { "Beautiful - Warm Bloom", {
            { "textureGrainOn", 1 }, { "grainSize", 320 }, { "density", 55 }, { "beautySpaceOn", 1 },
            { "beautyOn", 1 }, { "beautyAmount", 0.6f }, { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.6f },
            { "damageOn", 1 }, { "damageClip", 1 }, { "damageAmount", 0.15f }, { "damageMix", 0.3f } } },

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
            { "prettyReverbSize", 0.75f } } },

        // ---- Alien ----
        { "Alien - Transmission", {
            { "textureGrainOn", 1 }, { "grainSize", 80 }, { "density", 120 }, { "pitchJitter", 24 },
            { "spray", 200 }, { "pitchFormantOn", 1 }, { "pitchFormantMix", 0.6f }, { "pitch", -7 },
            { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.5f } } },
        { "Alien - Hive Mind", {
            { "textureGrainOn", 1 }, { "grainSize", 40 }, { "density", 200 }, { "pitchJitter", 36 },
            { "spray", 600 }, { "spectralOn", 1 }, { "spectralMix", 0.45f }, { "identityLossOn", 1 },
            { "identityLoss", 0.3f } } },
        { "Alien - Probe", {
            { "textureGrainOn", 1 }, { "grainSize", 120 }, { "density", 90 }, { "pitchFormantOn", 1 },
            { "pitchFormantMix", 0.8f }, { "pitch", 19 }, { "timeBreakerOn", 1 }, { "timeBreakerMix", 0.5f },
            { "timeBreakerSync", 1 }, { "timeBreakerDivision", 5 }, { "stutterChance", 0.5f } } },

        // ---- Destroyed ----
        { "Destroyed - Meltdown", {
            { "textureGrainOn", 1 }, { "grainSize", 120 }, { "density", 70 }, { "damageOn", 1 },
            { "damageClip", 2 }, { "damageAmount", 0.8f }, { "damageBits", 4 }, { "damageDropout", 0.3f },
            { "damageMix", 0.9f }, { "beautySpaceOn", 0 } } },
        { "Destroyed - Static Crush", {
            { "textureGrainOn", 1 }, { "grainSize", 200 }, { "density", 50 }, { "damageOn", 1 },
            { "damageClip", 0 }, { "damageAmount", 0.6f }, { "damageBits", 2 }, { "damageRate", 16 },
            { "damageNoise", 0.4f }, { "damageMix", 1.0f }, { "beautySpaceOn", 0 } } },
        { "Destroyed - Shrapnel", {
            { "textureGrainOn", 1 }, { "grainSize", 90 }, { "density", 100 }, { "damageOn", 1 },
            { "damageClip", 3 }, { "damageAmount", 0.7f }, { "damageBits", 6 }, { "timeBreakerOn", 1 },
            { "timeBreakerMix", 0.7f }, { "timeBreakerSync", 1 }, { "timeBreakerDivision", 6 },
            { "stutterChance", 0.7f }, { "reverseChance", 0.4f } } },

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
            { "prettyReverbSize", 1.0f } } },

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
            { "prettyReverbSize", 0.8f } } },

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
            { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.3f } } },

        { "Drums - Stutter Fill", {
            // Time Breaker on 1/16 stutter for fills; mostly dry with rhythmic glitches.
            { "textureGrainOn", 1 }, { "grainSize", 25 }, { "density", 120 }, { "textureGrainMix", 0.3f },
            { "timeBreakerOn", 1 }, { "timeBreakerSync", 1 }, { "timeBreakerDivision", 5 /*1/16*/ },
            { "stutterChance", 0.6f }, { "stutterSize", 0.5f }, { "reverseChance", 0.2f }, { "timeBreakerMix", 0.75f },
            { "duckOn", 1 }, { "duckAmount", 0.4f }, { "duckAttack", 3.0f }, { "duckRelease", 60.0f },
            { "beautySpaceOn", 1 }, { "echoOn", 1 }, { "echoMix", 0.15f }, { "echoFeedback", 0.25f },
            { "beautySpaceMix", 0.55f } } },

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
            { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.4f }, { "prettyReverbMix", 0.18f } } },

        { "Synth - Pluck Cloud", {
            { "textureGrainOn", 1 }, { "grainSize", 70 }, { "density", 90 }, { "spray", 50 },
            { "textureGrainMix", 0.55f },
            { "duckOn", 1 }, { "duckAmount", 0.4f }, { "duckRelease", 120.0f },
            { "beautySpaceOn", 1 }, { "echoOn", 1 }, { "echoMix", 0.22f }, { "echoFeedback", 0.4f },
            { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.55f } } },

        { "Synth - Detuned Choir", {
            { "textureGrainOn", 1 }, { "grainSize", 240 }, { "density", 50 }, { "pitchJitter", 0.4f },
            { "textureGrainMix", 0.65f },
            { "beautySpaceOn", 1 }, { "chorusOn", 1 }, { "chorusDepth", 0.7f }, { "chorusMix", 0.4f },
            { "harmonyOn", 1 }, { "harmonyMix", 0.25f },
            { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.75f } } },

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
            { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.8f }, { "prettyReverbMix", 0.35f } } },

        { "Vocal - Whisper Cloud", {
            { "textureGrainOn", 1 }, { "grainSize", 110 }, { "density", 90 }, { "spray", 120 },
            { "textureGrainMix", 0.7f }, { "spread", 0.8f },
            { "duckOn", 1 }, { "duckAmount", 0.4f }, { "duckRelease", 160.0f },
            { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.6f } } },

        { "Vocal - Formant Doubler", {
            { "textureGrainOn", 1 }, { "grainSize", 120 }, { "density", 60 },
            { "textureGrainMix", 0.4f },
            { "pitchFormantOn", 1 }, { "pitchFormantMix", 0.5f }, { "pitch", -5 }, { "pitchLockFormant", 1 },
            { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.55f } } },

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
            { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.6f } } },

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
            { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.3f } } },

        { "Bass - Reese Texture", {
            { "textureGrainOn", 1 }, { "grainSize", 100 }, { "density", 80 }, { "pitchJitter", 0.5f },
            { "textureGrainMix", 0.55f }, { "spread", 0.75f },
            { "damageOn", 1 }, { "damageClip", 0 /*Tube*/ }, { "damageAmount", 0.4f }, { "damageMix", 0.75f },
            { "damageSplitOn", 1 }, { "damageSplitHz", 800.0f }, { "damageHighAmount", 0.75f },
            { "beautySpaceOn", 1 }, { "chorusOn", 1 }, { "chorusMix", 0.2f } } },

        { "Bass - Granular Drone", {
            { "textureGrainOn", 1 }, { "grainSize", 480 }, { "density", 20 }, { "pitch", -7 },
            { "textureGrainMix", 0.75f }, { "spread", 0.4f },
            { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.85f }, { "prettyReverbDamping", 0.7f } } },

        { "Bass - Wobble Crush", {
            { "textureGrainOn", 1 }, { "grainSize", 60 }, { "density", 90 }, { "textureGrainMix", 0.45f },
            { "damageOn", 1 }, { "damageClip", 3 /*Fold*/ }, { "damageAmount", 0.5f }, { "damageMix", 0.85f },
            { "damageBits", 6.0f }, { "damageMix", 0.85f },
            { "duckOn", 1 }, { "duckAmount", 0.5f }, { "duckRelease", 100.0f } } },
    };
    return presets;
}

} // namespace gf
