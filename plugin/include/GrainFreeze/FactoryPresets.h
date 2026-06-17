#pragma once

#include <juce_core/juce_core.h>
#include <vector>

// Built-in factory presets. Each is a name (category-prefixed so the browser
// groups them when sorted) plus a list of {paramID, real value} overrides; all
// other params reset to their defaults when the preset is applied. Values are
// REAL (not normalized) — PresetManager converts via each param's range.
namespace gf
{

struct FactoryParam { const char* id; float value; };
struct FactoryPreset { const char* name; std::vector<FactoryParam> params; };

// The patch applied on a fresh insert (first impression), also selectable.
inline const char* kDefaultPresetName = "Default — MK Signature";

inline const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> presets = {
        { kDefaultPresetName, {
            { "textureGrainOn", 1 }, { "beautySpaceOn", 1 }, { "grainSize", 240 }, { "density", 40 },
            { "pitch", 0 }, { "spray", 120 }, { "echoOn", 1 }, { "echoTimeMs", 320 },
            { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.7f }, { "beautySpaceMix", 0.85f },
            { "textureGrainMix", 0.85f } } },

        // ---- Beautiful ----
        { "Beautiful — Angel Dust", {
            { "textureGrainOn", 1 }, { "grainSize", 600 }, { "density", 30 }, { "pitch", 12 },
            { "spray", 300 }, { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.85f },
            { "chorusOn", 1 }, { "spectralOn", 1 }, { "spectralMix", 0.3f } } },
        { "Beautiful — Glass Cathedral", {
            { "textureGrainOn", 1 }, { "grainSize", 900 }, { "density", 22 }, { "pitch", 7 },
            { "spectralOn", 1 }, { "spectralMix", 0.6f }, { "spectralAmount", 0.8f },
            { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.95f } } },
        { "Beautiful — Warm Bloom", {
            { "textureGrainOn", 1 }, { "grainSize", 320 }, { "density", 55 }, { "beautySpaceOn", 1 },
            { "beautyOn", 1 }, { "beautyAmount", 0.6f }, { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.6f },
            { "damageOn", 1 }, { "damageClip", 1 }, { "damageAmount", 0.15f }, { "damageMix", 0.3f } } },

        // ---- Dream ----
        { "Dream — Floating", {
            { "textureGrainOn", 1 }, { "grainSize", 800 }, { "density", 24 }, { "pitch", 12 },
            { "spray", 900 }, { "pitchJitter", 3 }, { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 },
            { "prettyReverbSize", 0.9f }, { "lfoDivision", 2 } } },
        { "Dream — Underwater", {
            { "textureGrainOn", 1 }, { "grainSize", 500 }, { "density", 35 }, { "chorusOn", 1 },
            { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.8f },
            { "damageOn", 1 }, { "damageClip", 1 }, { "damageTone", 0.35f }, { "damageMix", 0.4f } } },
        { "Dream — Halcyon", {
            { "textureGrainOn", 1 }, { "grainSize", 700 }, { "density", 28 }, { "pitch", 7 },
            { "spectralOn", 1 }, { "spectralMix", 0.4f }, { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 },
            { "prettyReverbSize", 0.75f } } },

        // ---- Alien ----
        { "Alien — Transmission", {
            { "textureGrainOn", 1 }, { "grainSize", 80 }, { "density", 120 }, { "pitchJitter", 24 },
            { "spray", 200 }, { "pitchFormantOn", 1 }, { "pitchFormantMix", 0.6f }, { "pitch", -7 },
            { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.5f } } },
        { "Alien — Hive Mind", {
            { "textureGrainOn", 1 }, { "grainSize", 40 }, { "density", 200 }, { "pitchJitter", 36 },
            { "spray", 600 }, { "spectralOn", 1 }, { "spectralMix", 0.45f }, { "identityLossOn", 1 },
            { "identityLoss", 0.3f } } },
        { "Alien — Probe", {
            { "textureGrainOn", 1 }, { "grainSize", 120 }, { "density", 90 }, { "pitchFormantOn", 1 },
            { "pitchFormantMix", 0.8f }, { "pitch", 19 }, { "timeBreakerOn", 1 }, { "timeBreakerMix", 0.5f },
            { "timeBreakerSync", 1 }, { "timeBreakerDivision", 5 }, { "stutterChance", 0.5f } } },

        // ---- Destroyed ----
        { "Destroyed — Meltdown", {
            { "textureGrainOn", 1 }, { "grainSize", 120 }, { "density", 70 }, { "damageOn", 1 },
            { "damageClip", 2 }, { "damageAmount", 0.8f }, { "damageBits", 4 }, { "damageDropout", 0.3f },
            { "damageMix", 0.9f }, { "beautySpaceOn", 0 } } },
        { "Destroyed — Static Crush", {
            { "textureGrainOn", 1 }, { "grainSize", 200 }, { "density", 50 }, { "damageOn", 1 },
            { "damageClip", 0 }, { "damageAmount", 0.6f }, { "damageBits", 2 }, { "damageRate", 16 },
            { "damageNoise", 0.4f }, { "damageMix", 1.0f }, { "beautySpaceOn", 0 } } },
        { "Destroyed — Shrapnel", {
            { "textureGrainOn", 1 }, { "grainSize", 90 }, { "density", 100 }, { "damageOn", 1 },
            { "damageClip", 3 }, { "damageAmount", 0.7f }, { "damageBits", 6 }, { "timeBreakerOn", 1 },
            { "timeBreakerMix", 0.7f }, { "timeBreakerSync", 1 }, { "timeBreakerDivision", 6 },
            { "stutterChance", 0.7f }, { "reverseChance", 0.4f } } },

        // ---- Cinematic ----
        { "Cinematic — Rise", {
            { "textureGrainOn", 1 }, { "grainSize", 700 }, { "density", 26 }, { "pitch", 12 },
            { "spray", 500 }, { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 }, { "prettyReverbSize", 0.92f },
            { "lfoDivision", 1 } } },
        { "Cinematic — Tension", {
            { "textureGrainOn", 1 }, { "grainSize", 300 }, { "density", 40 }, { "pitch", -12 },
            { "spectralOn", 1 }, { "spectralMix", 0.5f }, { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 },
            { "prettyReverbSize", 0.85f }, { "damageOn", 1 }, { "damageClip", 1 }, { "damageTone", 0.3f },
            { "damageMix", 0.3f } } },
        { "Cinematic — Impact Tail", {
            { "textureGrainOn", 1 }, { "grainSize", 1000 }, { "density", 20 }, { "spectralOn", 1 },
            { "spectralMix", 0.7f }, { "spectralAmount", 0.9f }, { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 },
            { "prettyReverbSize", 1.0f } } },

        // ---- Identity Loss ----
        { "Identity Loss — Dissolve", {
            { "textureGrainOn", 1 }, { "identityLossOn", 1 }, { "identityLoss", 0.7f }, { "identityLossMix", 1.0f },
            { "grainSize", 300 }, { "density", 60 }, { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 },
            { "prettyReverbSize", 0.7f } } },
        { "Identity Loss — Erased", {
            { "textureGrainOn", 1 }, { "identityLossOn", 1 }, { "identityLoss", 0.92f }, { "identityLossMix", 1.0f },
            { "grainSize", 180 }, { "density", 90 }, { "spectralOn", 1 }, { "spectralMix", 0.5f } } },
        { "Identity Loss — Phantom", {
            { "textureGrainOn", 1 }, { "identityLossOn", 1 }, { "identityLoss", 0.5f }, { "pitch", 7 },
            { "pitchFormantOn", 1 }, { "pitchFormantMix", 0.4f }, { "beautySpaceOn", 1 }, { "prettyReverbOn", 1 },
            { "prettyReverbSize", 0.8f } } },
    };
    return presets;
}

} // namespace gf
