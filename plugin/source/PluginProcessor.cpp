#include "GrainFreeze/PluginProcessor.h"
#include "GrainFreeze/PluginEditor.h"
#include <array>

namespace
{
float loadParam (const std::atomic<float>* p, float fallback = 0.0f) noexcept
{
    return p != nullptr ? p->load (std::memory_order_relaxed) : fallback;
}

bool isOn (const std::atomic<float>* p, bool fallback = false) noexcept
{
    return loadParam (p, fallback ? 1.0f : 0.0f) > 0.5f;
}

int maxGrainsForPerformanceMode (int mode) noexcept
{
    switch (juce::jlimit (0, 3, mode))
    {
        case 0:  return 16;  // Eco
        case 2:  return 64;  // Studio
        case 3:  return 128; // Render
        default: return 32;  // Live
    }
}
}

GrainFreezeProcessor::GrainFreezeProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, "PARAMS", createLayout())
{
    slotA = apvts.copyState();
    slotB = apvts.copyState();
    cacheParameterPointers();
    cacheModPointers();
    apvts.addParameterListener ("pitchLockFormant", this);
}

GrainFreezeProcessor::~GrainFreezeProcessor()
{
    apvts.removeParameterListener ("pitchLockFormant", this);
}

void GrainFreezeProcessor::cacheParameterPointers()
{
    auto bind = [this] (std::atomic<float>*& dst, const char* id)
    {
        dst = apvts.getRawParameterValue (id);
        jassert (dst != nullptr);
    };

    bind (paramPtrs.panic, "panic");
    bind (paramPtrs.pluginOn, "pluginOn");
    bind (paramPtrs.performanceMode, "performanceMode");
    bind (paramPtrs.analyzerOn, "analyzerOn");
    bind (paramPtrs.waveformOn, "waveformOn");
    bind (paramPtrs.modScopeOn, "modScopeOn");
    bind (paramPtrs.ecoUiMode, "ecoUiMode");
    bind (paramPtrs.oversamplingMode, "oversamplingMode");
    bind (paramPtrs.experimentalInputToolsOn, "experimentalInputToolsOn");

    bind (paramPtrs.beautySpaceOn, "beautySpaceOn");
    bind (paramPtrs.beautySpaceMix, "beautySpaceMix");
    bind (paramPtrs.beautySpaceAmount, "beautySpaceAmount");
    bind (paramPtrs.textureGrainOn, "textureGrainOn");
    bind (paramPtrs.textureGrainMix, "textureGrainMix");
    bind (paramPtrs.textureGrainAmount, "textureGrainAmount");
    bind (paramPtrs.identityLossOn, "identityLossOn");
    bind (paramPtrs.identityLossMix, "identityLossMix");
    bind (paramPtrs.identityLoss, "identityLoss");
    bind (paramPtrs.identityLossAmount, "identityLossAmount");
    bind (paramPtrs.spectralOn, "spectralOn");
    bind (paramPtrs.spectralMix, "spectralMix");
    bind (paramPtrs.spectralAmount, "spectralAmount");
    bind (paramPtrs.pitchFormantOn, "pitchFormantOn");
    bind (paramPtrs.pitchFormantMix, "pitchFormantMix");
    bind (paramPtrs.timeBreakerOn, "timeBreakerOn");
    bind (paramPtrs.timeBreakerMix, "timeBreakerMix");
    bind (paramPtrs.damageOn, "damageOn");
    bind (paramPtrs.damageMix, "damageMix");
    bind (paramPtrs.damageAmount, "damageAmount");
    bind (paramPtrs.motionMatrixOn, "motionMatrixOn");
    bind (paramPtrs.dryWet, "dryWet");

    bind (paramPtrs.macroBeauty, "macroBeauty");
    bind (paramPtrs.macroChaos, "macroChaos");
    bind (paramPtrs.macroGlue, "macroGlue");
    bind (paramPtrs.macroTexture, "macroTexture");
    bind (paramPtrs.macroSpace, "macroSpace");
    bind (paramPtrs.macroMotion, "macroMotion");
    bind (paramPtrs.macroDamage, "macroDamage");
    bind (paramPtrs.mutationAmount, "mutationAmount");
    bind (paramPtrs.globalRate, "globalRate");
    bind (paramPtrs.globalShape, "globalShape");
    bind (paramPtrs.globalModOn, "globalModOn");

    bind (paramPtrs.sampleMode, "sampleMode");
    bind (paramPtrs.sampleWindow, "sampleWindow");
    bind (paramPtrs.sampleSource, "sampleSource");
    bind (paramPtrs.sampleLevel, "sampleLevel");
    bind (paramPtrs.midiEnable, "midiEnable");
    bind (paramPtrs.midiRoot, "midiRoot");
    bind (paramPtrs.glideTime, "glideTime");
    bind (paramPtrs.velToAmp, "velToAmp");

    bind (paramPtrs.frozen, "frozen");
    bind (paramPtrs.specFreeze, "specFreeze");
    bind (paramPtrs.specMix, "specMix");
    bind (paramPtrs.specShimmer, "specShimmer");
    bind (paramPtrs.satOn, "satOn");
    bind (paramPtrs.satType, "satType");
    bind (paramPtrs.satDrive, "satDrive");
    bind (paramPtrs.satMix, "satMix");

    bind (paramPtrs.prettifierEnabled, "prettifierEnabled");
    bind (paramPtrs.prettifierInTrim, "prettifierInTrim");
    bind (paramPtrs.prettifierOutTrim, "prettifierOutTrim");
    bind (paramPtrs.echoOn, "echoOn");
    bind (paramPtrs.reverbOn, "reverbOn");
    bind (paramPtrs.prettyReverbOn, "prettyReverbOn");
    bind (paramPtrs.prettyReverbSize, "prettyReverbSize");
    bind (paramPtrs.prettyReverbDamping, "prettyReverbDamping");
    bind (paramPtrs.chorusOn, "chorusOn");
    bind (paramPtrs.chorusMix, "chorusMix");
    bind (paramPtrs.beautyOn, "beautyOn");
    bind (paramPtrs.beautyAir, "beautyAir");
    bind (paramPtrs.beautyWarmth, "beautyWarmth");
    bind (paramPtrs.polishOn, "polishOn");
    bind (paramPtrs.polishAir, "polishAir");
    bind (paramPtrs.polishWarmth, "polishWarmth");
    bind (paramPtrs.polishHarshnessTame, "polishHarshnessTame");
    bind (paramPtrs.polishMix, "polishMix");
    bind (paramPtrs.crushOn, "crushOn");
    bind (paramPtrs.crushMix, "crushMix");
    bind (paramPtrs.dnaCharacter, "dnaCharacter");
    bind (paramPtrs.dnaAge, "dnaAge");
    bind (paramPtrs.dnaWarmth, "dnaWarmth");
    bind (paramPtrs.dnaWidth, "dnaWidth");
    bind (paramPtrs.dnaRandomness, "dnaRandomness");
    bind (paramPtrs.dnaAnalog, "dnaAnalog");
    bind (paramPtrs.dnaDigital, "dnaDigital");
    bind (paramPtrs.dnaSmoothness, "dnaSmoothness");
    bind (paramPtrs.dnaMotion, "dnaMotion");
    bind (paramPtrs.dnaShine, "dnaShine");

    bind (paramPtrs.routingMode, "routingMode");
    bind (paramPtrs.entropyOn, "entropyOn");
    bind (paramPtrs.prettifierOn, "prettifierOn");
    bind (paramPtrs.limiterOn, "limiterOn");
    bind (paramPtrs.mixEqOn, "mixEqOn");
    bind (paramPtrs.pitchMatchOn, "pitchMatchOn");
    bind (paramPtrs.tempoLockOn, "tempoLockOn");
    bind (paramPtrs.dryLevel, "dryLevel");
    bind (paramPtrs.entropySend, "entropySend");
    bind (paramPtrs.entropyReturn, "entropyReturn");
    bind (paramPtrs.prettifierSend, "prettifierSend");
    bind (paramPtrs.prettifierReturn, "prettifierReturn");
    bind (paramPtrs.mixOutput, "mixOutput");
    bind (paramPtrs.chaosBeauty, "chaosBeauty");
    bind (paramPtrs.ceilingDb, "ceilingDb");
    bind (paramPtrs.eqLow, "eqLow");
    bind (paramPtrs.eqMid, "eqMid");
    bind (paramPtrs.eqHigh, "eqHigh");
    bind (paramPtrs.eqLoFi, "eqLoFi");
    bind (paramPtrs.mixWidth, "mixWidth");
    bind (paramPtrs.mixGlue, "mixGlue");
    bind (paramPtrs.pitchLockOn, "pitchLockOn");
    bind (paramPtrs.pitchLockMode, "pitchLockMode");
    bind (paramPtrs.pitchLockKey, "pitchLockKey");
    bind (paramPtrs.pitchLockScale, "pitchLockScale");
    bind (paramPtrs.pitchLockAmount, "pitchLockAmount");
    bind (paramPtrs.pitchLockFormant, "pitchLockFormant");

    panicParameter = apvts.getParameter ("panic");
    jassert (panicParameter != nullptr);
}

void GrainFreezeProcessor::cacheModPointers()
{
    for (int i = 0; i < gf::kNumModParams; ++i)
    {
        const juce::String base (gf::paramIdString ((gf::ParamId) i));
        auto& p = modPtrs[(size_t) i];
        modTargetPtrs[(size_t) i] = apvts.getRawParameterValue (base);
        modTargetRanges[(size_t) i] = apvts.getParameterRange (base);
        p.lfoRate   = apvts.getRawParameterValue (base + "_lfoRate");
        p.lfoDepth  = apvts.getRawParameterValue (base + "_lfoDepth");
        p.lfoShape  = apvts.getRawParameterValue (base + "_lfoShape");
        p.shRate    = apvts.getRawParameterValue (base + "_shRate");
        p.shDepth   = apvts.getRawParameterValue (base + "_shDepth");
        p.globalAmt = apvts.getRawParameterValue (base + "_globalAmt");
        p.bipolar   = apvts.getRawParameterValue (base + "_bipolar");
        p.rangeMin  = apvts.getRawParameterValue (base + "_rangeMin");
        p.rangeMax  = apvts.getRawParameterValue (base + "_rangeMax");
        p.skew      = apvts.getRawParameterValue (base + "_skew");
    }
}

// The formant shifter adds lookahead; (de)activating it changes plugin latency.
void GrainFreezeProcessor::parameterChanged (const juce::String& id, float value)
{
    if (id == "pitchLockFormant")
    {
        formantLatencyActive.store (value > 0.5f, std::memory_order_relaxed);
        triggerAsyncUpdate(); // setLatencySamples must run on the message thread
    }
}

void GrainFreezeProcessor::handleAsyncUpdate()
{
    setLatencySamples (formantLatencyActive.load (std::memory_order_relaxed)
                       ? gf::FormantShifter::kLatency : 0);
}

juce::AudioProcessorValueTreeState::ParameterLayout GrainFreezeProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    gf::entropy::EntropyEngine::addParameters (layout);
    gf::mix::MixEngine::addParameters (layout);
    gf::pretty::PrettifierEngine::addParameters (layout);

    auto addBool = [&layout] (const juce::String& id, const juce::String& name, bool defaultValue)
    {
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, defaultValue));
    };
    auto addFloat = [&layout] (const juce::String& id, const juce::String& name,
                               NormalisableRange<float> range, float defaultValue)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, range, defaultValue));
    };
    auto addChoice = [&layout] (const juce::String& id, const juce::String& name,
                                const StringArray& choices, int defaultIndex)
    {
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, choices, defaultIndex));
    };
    auto addInt = [&layout] (const juce::String& id, const juce::String& name,
                             int minValue, int maxValue, int defaultValue)
    {
        layout.add (std::make_unique<AudioParameterInt> (ParameterID { id, 1 }, name, minValue, maxValue, defaultValue));
    };
    auto addMachine = [&] (const juce::String& stem, const juce::String& name, bool defaultOn)
    {
        addBool  (stem + "On",     name + " On", defaultOn);
        addFloat (stem + "Mix",    name + " Mix",    NormalisableRange<float> (0.0f, 1.0f, 0.001f), defaultOn ? 1.0f : 0.0f);
        addFloat (stem + "Amount", name + " Amount", NormalisableRange<float> (0.0f, 1.0f, 0.001f), defaultOn ? 0.5f : 0.0f);
        addBool  (stem + "ModOn",  name + " Mod On", true);
        addBool  (stem + "Lock",   name + " Randomize Lock", false);
    };
    auto addModLock = [&] (const juce::String& stem, const juce::String& name)
    {
        addBool (stem + "ModOn", name + " Mod On", true);
        addBool (stem + "Lock",  name + " Randomize Lock", false);
    };

    addChoice ("performanceMode", "Performance Mode", StringArray { "Eco", "Live", "Studio", "Render" }, 1);
    addBool ("analyzerOn", "Analyzer On", true);
    addBool ("waveformOn", "Waveform On", true);
    addBool ("modScopeOn", "Mod Scope On", true);
    addBool ("animationsOn", "Animations On", true);
    addBool ("ecoUiMode", "Eco UI Mode", false);
    addBool ("experimentalInputToolsOn", "Experimental MIDI / Sample Tools", false);
    addFloat ("analyzerFps", "Analyzer FPS", NormalisableRange<float> (5.0f, 60.0f, 1.0f), 30.0f);
    addChoice ("oversamplingMode", "Oversampling Mode", StringArray { "Off", "Auto", "Always" }, 1);
    addFloat ("dryWet", "Dry/Wet", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f);

    addMachine ("beautySpace", "Beauty & Space", true);
    addMachine ("textureGrain", "Texture / Grain", true);
    addMachine ("identityLoss", "Identity Loss", true);
    addMachine ("spectral", "Spectral", false);
    addMachine ("pitchFormant", "Pitch / Formant", false);
    addMachine ("timeBreaker", "Time Breaker", false);
    addMachine ("damage", "Damage", false);
    addMachine ("motionMatrix", "Motion Matrix", true);
    addFloat ("analyzerScopeMix", "Analyzer / Scopes Mix", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f);
    addFloat ("analyzerScopeAmount", "Analyzer / Scopes Amount", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f);
    addBool ("analyzerScopeModOn", "Analyzer / Scopes Mod On", true);
    addBool ("analyzerScopeLock", "Analyzer / Scopes Randomize Lock", false);

    addFloat ("identityLoss", "Identity Loss", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f);
    addInt ("mutationSeed", "Mutation Seed", 0, 999999, 0);
    addChoice ("mutationMode", "Mutation Mode",
               StringArray { "Beautiful", "Glitch", "Horror", "Alien", "Dream", "Broken", "Machine", "Cinematic", "Identity Loss" }, 0);
    addBool ("mutationTempoSync", "Mutation Tempo Sync", false);
    addFloat ("mutationSmoothing", "Mutation Smoothing", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.25f);

    for (const auto& stemAndName : {
        std::pair<const char*, const char*> { "echo", "Echo" },
        { "reverb", "Reverb" },
        { "chorus", "Chorus" },
        { "beauty", "Beauty" },
        { "polish", "Polish" },
    })
        addModLock (stemAndName.first, stemAndName.second);

    addBool ("grainReverseOn", "Grain Reverse On", false);
    addFloat ("grainReverseChance", "Grain Reverse Chance", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f);
    addChoice ("grainShape", "Grain Shape", StringArray { "Hann", "Tukey", "Gaussian", "Rect", "Ramp", "Random" }, 0);
    addFloat ("grainSkew", "Grain Skew", NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f);
    addFloat ("grainChaos", "Grain Chaos", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f);
    addFloat ("grainStartJitter", "Grain Start Jitter", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f);
    addFloat ("grainStretch", "Grain Stretch", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f);
    addFloat ("grainBlur", "Grain Blur", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f);
    addBool ("grainFeedbackOn", "Grain Feedback On", false);
    addFloat ("grainFeedback", "Grain Feedback", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f);
    addBool ("grainFilterOn", "Grain Filter On", false);
    addChoice ("grainFilterType", "Grain Filter Type", StringArray { "LP", "BP", "HP", "Notch", "Comb" }, 0);
    addFloat ("grainFilterCutoff", "Grain Filter Cutoff", NormalisableRange<float> (20.0f, 20000.0f, 1.0f, 0.35f), 12000.0f);
    addFloat ("grainFilterResonance", "Grain Filter Resonance", NormalisableRange<float> (0.1f, 12.0f, 0.001f), 0.7f);
    addFloat ("grainFilterChaos", "Grain Filter Chaos", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f);
    addFloat ("grainDrive", "Grain Drive", NormalisableRange<float> (0.0f, 24.0f, 0.001f), 0.0f);
    addBool ("grainOctaveCloudOn", "Grain Octave Cloud On", false);
    addFloat ("grainOctaveCloud", "Grain Octave Cloud", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f);
    addFloat ("grainAmpChaos", "Grain Amp Chaos", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f);
    for (const auto& stemAndName : {
        std::pair<const char*, const char*> { "grainReverse", "Grain Reverse" },
        { "grainFeedback", "Grain Feedback" },
        { "grainFilter", "Grain Filter" },
        { "grainOctaveCloud", "Grain Octave Cloud" },
        { "grainStretch", "Grain Stretch" },
    })
        addModLock (stemAndName.first, stemAndName.second);

    addBool ("spectralWarpOn", "Spectral Warp On", false);
    addFloat ("spectralWarp", "Spectral Warp", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f);
    addBool ("spectralSmearOn", "Spectral Smear On", false);
    addFloat ("spectralSmear", "Spectral Smear", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f);
    addFloat ("spectralTilt", "Spectral Tilt", NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f);
    addBool ("spectralMaskOn", "Spectral Mask On", false);
    addFloat ("spectralMask", "Spectral Mask", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f);
    addBool ("spectralShuffleOn", "Spectral Shuffle On", false);
    addFloat ("spectralShuffle", "Spectral Shuffle", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f);
    addFloat ("spectralPhaseChaos", "Spectral Phase Chaos", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f);
    addFloat ("spectralRobot", "Spectral Robot", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f);
    addFloat ("spectralVowel", "Spectral Vowel", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f);
    addFloat ("spectralResonance", "Spectral Resonance", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f);
    addChoice ("spectralQuality", "Spectral Quality", StringArray { "Eco", "Live", "Studio", "Render" }, 1);
    for (const auto& stemAndName : {
        std::pair<const char*, const char*> { "spectralWarp", "Spectral Warp" },
        { "spectralSmear", "Spectral Smear" },
        { "spectralShuffle", "Spectral Shuffle" },
        { "spectralMask", "Spectral Mask" },
        { "spectralRobot", "Spectral Robot" },
    })
        addModLock (stemAndName.first, stemAndName.second);

    addFloat ("pitchSpread", "Pitch Spread", NormalisableRange<float> (0.0f, 48.0f, 0.001f), 0.0f);
    addBool ("pitchQuantizeOn", "Pitch Quantize On", false);
    addFloat ("pitchQuantize", "Pitch Quantize", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f);
    addBool ("pitchRandomOn", "Pitch Random On", false);
    addFloat ("pitchRandom", "Pitch Random", NormalisableRange<float> (0.0f, 48.0f, 0.001f), 0.0f);
    addBool ("formantShiftOn", "Formant Shift On", false);
    addFloat ("formantShift", "Formant Shift", NormalisableRange<float> (-24.0f, 24.0f, 0.001f), 0.0f);
    addFloat ("formantSmear", "Formant Smear", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f);
    addBool ("ringModOn", "Ring Mod On", false);
    addFloat ("ringModFreq", "Ring Mod Frequency", NormalisableRange<float> (0.1f, 8000.0f, 0.001f, 0.35f), 220.0f);
    addFloat ("ringModMix", "Ring Mod Mix", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f);
    addBool ("frequencyShiftOn", "Frequency Shift On", false);
    addFloat ("frequencyShiftHz", "Frequency Shift Hz", NormalisableRange<float> (-5000.0f, 5000.0f, 0.001f), 0.0f);
    addFloat ("frequencyShiftMix", "Frequency Shift Mix", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f);
    addFloat ("inharmonicity", "Inharmonicity", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f);
    for (const auto& stemAndName : {
        std::pair<const char*, const char*> { "formantShift", "Formant Shift" },
        { "ringMod", "Ring Mod" },
        { "frequencyShift", "Frequency Shift" },
        { "pitchRandom", "Pitch Random" },
        { "pitchQuantize", "Pitch Quantize" },
    })
        addModLock (stemAndName.first, stemAndName.second);

    addBool ("stutterOn", "Stutter On", false);
    addFloat ("stutterRate", "Stutter Rate", NormalisableRange<float> (0.25f, 64.0f, 0.001f), 8.0f);
    addFloat ("stutterSize", "Stutter Size", NormalisableRange<float> (1.0f, 1000.0f, 1.0f), 80.0f);
    addFloat ("stutterChance", "Stutter Chance", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f);
    addFloat ("gateChance", "Gate Chance", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f);
    addFloat ("reverseChance", "Reverse Chance", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f);
    addBool ("reverseChanceOn", "Reverse Chance On", false);
    addFloat ("tapeStopAmount", "Tape Stop Amount", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f);
    addBool ("tapeStopOn", "Tape Stop On", false);
    addFloat ("tapeStartAmount", "Tape Start Amount", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f);
    addFloat ("scrubSpeed", "Scrub Speed", NormalisableRange<float> (-4.0f, 4.0f, 0.001f), 1.0f);
    addFloat ("scrubJitter", "Scrub Jitter", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f);
    addBool ("bufferJumpOn", "Buffer Jump On", false);
    addFloat ("bufferJumpChance", "Buffer Jump Chance", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f);
    addFloat ("bufferJumpSize", "Buffer Jump Size", NormalisableRange<float> (1.0f, 4000.0f, 1.0f), 250.0f);
    addBool ("gateOn", "Gate On", false);
    for (const auto& stemAndName : {
        std::pair<const char*, const char*> { "stutter", "Stutter" },
        { "reverseChance", "Reverse Chance" },
        { "tapeStop", "Tape Stop" },
        { "bufferJump", "Buffer Jump" },
        { "gate", "Gate" },
    })
        addModLock (stemAndName.first, stemAndName.second);

    addBool ("bitCrushOn", "Bit Crush On", false);
    addFloat ("bitDepth", "Bit Depth", NormalisableRange<float> (1.0f, 24.0f, 0.001f), 16.0f);
    addBool ("sampleRateOn", "Sample Rate On", false);
    addFloat ("sampleRateHz", "Sample Rate Hz", NormalisableRange<float> (250.0f, 48000.0f, 1.0f, 0.45f), 48000.0f);
    addFloat ("sampleRateMix", "Sample Rate Mix", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f);
    addFloat ("aliasMix", "Alias Mix", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f);
    addBool ("wavefoldOn", "Wavefold On", false);
    addFloat ("wavefoldAmount", "Wavefold Amount", NormalisableRange<float> (0.0f, 24.0f, 0.001f), 0.0f);
    addFloat ("wavefoldSymmetry", "Wavefold Symmetry", NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f);
    addFloat ("rectifyAmount", "Rectify Amount", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f);
    addFloat ("zeroCrossMangle", "Zero Cross Mangle", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f);
    addBool ("dropoutOn", "Dropout On", false);
    addFloat ("dropoutChance", "Dropout Chance", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f);
    addFloat ("dropoutLength", "Dropout Length", NormalisableRange<float> (1.0f, 1000.0f, 1.0f), 20.0f);
    addFloat ("digitalClipping", "Digital Clipping", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f);
    addFloat ("speakerBreakup", "Speaker Breakup", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f);
    addBool ("speakerBreakupOn", "Speaker Breakup On", false);
    addFloat ("codecCrush", "Codec Crush", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f);
    addBool ("codecCrushOn", "Codec Crush On", false);
    for (const auto& stemAndName : {
        std::pair<const char*, const char*> { "bitCrush", "Bit Crush" },
        { "sampleRate", "Sample Rate" },
        { "wavefold", "Wavefold" },
        { "dropout", "Dropout" },
        { "codecCrush", "Codec Crush" },
        { "speakerBreakup", "Speaker Breakup" },
    })
        addModLock (stemAndName.first, stemAndName.second);

    // Modulation parameters, per modulatable knob. These live in the APVTS so
    // they are automatable AND captured by the .preset files automatically.
    for (int i = 0; i < gf::kNumModParams; ++i)
    {
        const auto base = juce::String (gf::paramIdString ((gf::ParamId) i));
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { base + "_lfoRate", 1 }, base + " LFO Rate",
            NormalisableRange<float> (0.0f, 12.0f, 0.01f), 0.0f));
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { base + "_lfoDepth", 1 }, base + " LFO Depth",
            NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));
        layout.add (std::make_unique<AudioParameterChoice> (
            ParameterID { base + "_lfoShape", 1 }, base + " LFO Shape",
            StringArray { "Sine", "Triangle", "Saw", "Square" }, 0));
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { base + "_shRate", 1 }, base + " S&H Rate",
            NormalisableRange<float> (0.0f, 12.0f, 0.01f), 0.0f));
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { base + "_shDepth", 1 }, base + " S&H Depth",
            NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { base + "_globalAmt", 1 }, base + " Global Mod",
            NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f));
        layout.add (std::make_unique<AudioParameterBool> (
            ParameterID { base + "_bipolar", 1 }, base + " Bipolar", true));
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { base + "_rangeMin", 1 }, base + " Mod Min",
            NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f));
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { base + "_rangeMax", 1 }, base + " Mod Max",
            NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f));
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { base + "_skew", 1 }, base + " Mod Skew",
            NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f));
    }

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "globalRate", 1 }, "Global Mod Rate",
        NormalisableRange<float> (0.01f, 8.0f, 0.01f), 0.5f));
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "globalShape", 1 }, "Global Mod Shape",
        StringArray { "Sine", "Triangle", "Saw", "Square" }, 0));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "globalModOn", 1 }, "Global Mod On", true));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "panic", 1 }, "Panic", false));

    // Sample Mode: freeze a moment of audio and loop/play it from the keyboard.
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "sampleMode", 1 }, "Sample Mode", false));
    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "sampleSource", 1 }, "Sample Source", StringArray { "Input", "Output" }, 0));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "sampleWindow", 1 }, "Sample Window",
                                                        NormalisableRange<float> (1.0f, 10.0f, 0.1f), 4.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "sampleLevel", 1 }, "Sample Level",
                                                        NormalisableRange<float> (0.0f, 1.5f, 0.001f), 1.0f));

    static constexpr const char* macroIds[] = {
        "macroBeauty", "macroChaos", "macroEmotion", "macroDamage",
        "macroMotion", "macroSpace", "macroTexture", "macroGlue", "macroMorph"
    };
    static constexpr const char* macroNames[] = {
        "Macro Beauty", "Macro Chaos", "Macro Emotion", "Macro Damage",
        "Macro Motion", "Macro Space", "Macro Texture", "Macro Glue", "Macro Morph"
    };
    for (size_t i = 0; i < std::size (macroIds); ++i)
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { macroIds[i], 1 }, macroNames[i],
            NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "randomMode", 1 }, "Random Mode",
        StringArray { "Subtle", "Musical", "Glitch", "Ambient", "Horror", "Destroyed", "Cinematic", "Beautiful", "Dream", "Angel", "Vintage", "Alien", "Machine", "Identity Loss" }, 1));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "mutationAmount", 1 }, "Mutation Amount",
                                                        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.35f));

    for (const auto& text : { juce::String ("More Beautiful"), juce::String ("More Expensive"), juce::String ("More Wide"),
                               juce::String ("More Dreamy"), juce::String ("More Emotional"), juce::String ("More Dark"),
                               juce::String ("More Angelic"), juce::String ("More Broken"), juce::String ("More Vintage"),
                               juce::String ("More Cinematic"), juce::String ("More Controlled"), juce::String ("More Chaotic") })
    {
        const auto id = "assistant_" + text.toLowerCase().replaceCharacter (' ', '_');
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, text, false));
    }

    return layout;
}

void GrainFreezeProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    entropyEngine.prepare (sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    mixEngine.prepare (sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    prettifierEngine.prepare (sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    midiCtrl.prepare (sampleRate);
    sampleEngine.prepare (sampleRate, getTotalNumOutputChannels());

    // Report the formant shifter's lookahead so the host can compensate.
    formantLatencyActive.store (isOn (paramPtrs.pitchLockFormant),
                                std::memory_order_relaxed);
    setLatencySamples (formantLatencyActive.load (std::memory_order_relaxed)
                       ? gf::FormantShifter::kLatency : 0);
    dryInBuffer.setSize (getTotalNumOutputChannels(), samplesPerBlock);
    entropyBuffer.setSize (getTotalNumOutputChannels(), samplesPerBlock);
    prettifierBuffer.setSize (getTotalNumOutputChannels(), samplesPerBlock);

    // Run the mod matrix at ~100 Hz regardless of sample rate.
    controlRateSamples = juce::jmax (1, (int) (sampleRate / 100.0));
    controlRateDivider = 0;
    modMatrix.prepare (sampleRate / (double) controlRateSamples);

    // Output scope window: 256 points across ~50 ms, independent of block size.
    currentSampleRate = sampleRate;
    constexpr double kScopeWindowMs = 50.0;
    waveformDecimate = juce::jmax (1, (int) std::round (sampleRate * (kScopeWindowMs / 1000.0) / 256.0));
    waveformDecimateCounter = 0;
}

bool GrainFreezeProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

GrainFreezeProcessor::ControlSnapshot GrainFreezeProcessor::makeControlSnapshot() const
{
    ControlSnapshot s;
    s.pluginOn = isOn (paramPtrs.pluginOn, true);
    s.performanceMode = (int) loadParam (paramPtrs.performanceMode, 1.0f);
    s.maxGrains = maxGrainsForPerformanceMode (s.performanceMode);
    s.oversamplingMode = (int) loadParam (paramPtrs.oversamplingMode, 1.0f);

    s.identityLoss = loadParam (paramPtrs.identityLoss, 0.0f);
    s.mutationAmount = loadParam (paramPtrs.mutationAmount, 0.35f);
    s.beauty = loadParam (paramPtrs.macroBeauty, 0.0f);
    s.texture = loadParam (paramPtrs.macroTexture, 0.0f);
    s.space = loadParam (paramPtrs.macroSpace, 0.0f);
    s.motion = loadParam (paramPtrs.macroMotion, 0.0f);
    s.damage = juce::jmax (loadParam (paramPtrs.macroDamage, 0.0f), loadParam (paramPtrs.damageAmount, 0.0f));
    s.chaos = loadParam (paramPtrs.macroChaos, 0.0f);
    s.dryWet = loadParam (paramPtrs.dryWet, 1.0f);
    s.outputLevel = loadParam (paramPtrs.mixOutput, 1.0f);

    const bool ecoUi = isOn (paramPtrs.ecoUiMode);
    s.analyzerOn = isOn (paramPtrs.analyzerOn, true) && ! ecoUi;
    s.waveformOn = isOn (paramPtrs.waveformOn, true) && ! ecoUi;
    s.modScopeOn = isOn (paramPtrs.modScopeOn, true) && ! ecoUi;
    s.motionMatrixOn = isOn (paramPtrs.motionMatrixOn, true);
    s.inputToolsOn = kMkUltraExperimentalInputTools || isOn (paramPtrs.experimentalInputToolsOn);
    s.sampleModeOn = s.inputToolsOn && isOn (paramPtrs.sampleMode);
    s.midiEnabled = s.inputToolsOn && isOn (paramPtrs.midiEnable);
    s.tempoLockOn = isOn (paramPtrs.tempoLockOn);

    const bool oldBeautyOn = isOn (paramPtrs.prettifierOn, true) && isOn (paramPtrs.prettifierEnabled, true);
    const bool oldTextureOn = isOn (paramPtrs.entropyOn, true);
    const bool identityActive = isOn (paramPtrs.identityLossOn, true)
                             && loadParam (paramPtrs.identityLossMix, 1.0f) > 0.001f
                             && s.identityLoss > 0.001f;

    s.beautySpaceOn = oldBeautyOn
                   && isOn (paramPtrs.beautySpaceOn, true)
                   && loadParam (paramPtrs.beautySpaceMix, 1.0f) > 0.001f;
    s.textureGrainOn = (oldTextureOn
                    && isOn (paramPtrs.textureGrainOn, true)
                    && loadParam (paramPtrs.textureGrainMix, 1.0f) > 0.001f)
                    || identityActive;
    s.identityLossOn = identityActive;
    s.spectralOn = isOn (paramPtrs.spectralOn)
                && loadParam (paramPtrs.spectralMix, 0.0f) > 0.001f;
    s.pitchFormantOn = isOn (paramPtrs.pitchFormantOn)
                    && loadParam (paramPtrs.pitchFormantMix, 0.0f) > 0.001f;
    s.timeBreakerOn = isOn (paramPtrs.timeBreakerOn)
                   && loadParam (paramPtrs.timeBreakerMix, 0.0f) > 0.001f;
    s.damageOn = isOn (paramPtrs.damageOn)
              && loadParam (paramPtrs.damageMix, 0.0f) > 0.001f;

    return s;
}

float GrainFreezeProcessor::modulated (gf::ParamId id)
{
    const auto idx = (size_t) id;
    const float base = loadParam (modTargetPtrs[idx]);
    const float offset = modMatrix.getOffset (id);            // -1..1
    const auto& range  = modTargetRanges[idx];
    const float span   = range.end - range.start;

    float value = base + offset * span * 0.5f;

    // Per-knob mod range window: clamp the modulated value to [min,max] of range.
    auto& slot = modMatrix.slot (id);
    const float lo = range.start + slot.rangeMin.load() * span;
    const float hi = range.start + slot.rangeMax.load() * span;
    value = juce::jlimit (juce::jmin (lo, hi), juce::jmax (lo, hi), value);

    return juce::jlimit (range.start, range.end, value);
}

void GrainFreezeProcessor::syncModMatrix()
{
    for (int i = 0; i < gf::kNumModParams; ++i)
    {
        auto& s = modMatrix.slot ((gf::ParamId) i);
        const auto& p = modPtrs[(size_t) i]; // cached: no strings/lookups on the audio thread

        s.lfoRate.store   (p.lfoRate->load());
        s.lfoDepth.store  (p.lfoDepth->load());
        s.lfoShape.store  ((int) p.lfoShape->load());
        s.shRate.store    (p.shRate->load());
        s.shDepth.store   (p.shDepth->load());
        s.globalAmt.store (p.globalAmt->load());
        s.bipolar.store   (p.bipolar->load() > 0.5f);
        s.rangeMin.store  (p.rangeMin->load());
        s.rangeMax.store  (p.rangeMax->load());
        s.skew.store      (p.skew->load());
    }
    modMatrix.setGlobalRate    (loadParam (paramPtrs.globalRate, 0.5f));
    modMatrix.setGlobalShape   ((int) loadParam (paramPtrs.globalShape, 0.0f));
    modMatrix.setGlobalEnabled (isOn (paramPtrs.globalModOn, true));
}

void GrainFreezeProcessor::pullParameters()
{
    using PI = gf::ParamId;
    juce::ignoreUnused (modulated (PI::grainSize));
    juce::ignoreUnused (modulated (PI::density));
    juce::ignoreUnused (modulated (PI::pitch));
    juce::ignoreUnused (modulated (PI::spray));
    juce::ignoreUnused (modulated (PI::spread));
    juce::ignoreUnused (modulated (PI::position));
    juce::ignoreUnused (modulated (PI::pitchJitter));
    juce::ignoreUnused (modulated (PI::output));
}

void GrainFreezeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const auto ctl = makeControlSnapshot();
    const auto& p = paramPtrs;
    const int channels = buffer.getNumChannels();
    const int n = buffer.getNumSamples();

    if (isOn (p.panic))
    {
        panicKillTail();
        if (panicParameter != nullptr)
            panicParameter->setValueNotifyingHost (0.0f);
    }

    if (! ctl.pluginOn)
        return;

    if (! ctl.inputToolsOn)
        sampleFreezeRequested.store (false, std::memory_order_release);

    const bool sampleFreezePending = ctl.inputToolsOn && sampleFreezeRequested.load (std::memory_order_acquire);
    const bool textureActive = ctl.textureGrainOn || ctl.identityLossOn;
    const bool beautyActive = ctl.beautySpaceOn;
    const int routingModeValue = (int) loadParam (p.routingMode, 0.0f);
    const bool pitchMatchOn = isOn (p.pitchMatchOn);
    const bool needsDry = loadParam (p.dryLevel) > 0.001f
                       || ctl.dryWet < 0.999f
                       || pitchMatchOn
                       || ctl.sampleModeOn
                       || sampleFreezePending
                       || (! textureActive && ! beautyActive);
    if (needsDry)
        dryInBuffer.makeCopyOf (buffer, true);

    if (ctl.sampleModeOn || sampleFreezePending)
        sampleEngine.pushInput (needsDry ? dryInBuffer : buffer);

    // Advance the mod matrix in control-rate steps across the block. We tick at
    // least once per block, and pull modulated parameters after each tick so the
    // engine tracks modulation without per-sample overhead.
    int processed = 0;
    float noteOffset = 0.0f;
    float midiVelocity = 1.0f;
    // The keyboard drives the grains (when MIDI is enabled) and/or the frozen
    // sample (when Sample Mode is on), so run the note controller for either.
    const bool keyboardActive = ctl.midiEnabled || ctl.sampleModeOn;
    if (keyboardActive)
    {
        keyboardState.processNextMidiBuffer (midi, 0, n, true);
        midiCtrl.setRoot ((int) loadParam (p.midiRoot, 60.0f));
        midiCtrl.setGlideMs (loadParam (p.glideTime));
        midiCtrl.updateGlideTime();
        for (const auto meta : midi)
            midiCtrl.handleMessage (meta.getMessage());
        noteOffset = midiCtrl.nextOffsetSemis();
        midiVelocity = midiCtrl.getVelocity();
    }

    if (ctl.motionMatrixOn)
    {
        do
        {
            if (controlRateDivider <= 0)
            {
                syncModMatrix();
                modMatrix.tick();
                const float gmv = modMatrix.getGlobalValue();
                globalModValue.store (gmv, std::memory_order_relaxed);

                if (ctl.modScopeOn)
                {
                    int mh = modScopeHead.load (std::memory_order_relaxed);
                    modScopeHistory[(size_t) mh].store (gmv, std::memory_order_relaxed);
                    modScopeHead.store ((mh + 1) % 256, std::memory_order_relaxed);
                }

                pullParameters();
                controlRateDivider = controlRateSamples;
                if (keyboardActive)
                    noteOffset = midiCtrl.nextOffsetSemis();
            }
            const int step = juce::jmin (controlRateDivider, n - processed);
            controlRateDivider -= step;
            processed += step;
        } while (processed < n);
    }
    else
    {
        globalModValue.store (0.0f, std::memory_order_relaxed);
    }

    auto target = [this, &ctl] (gf::ParamId id)
    {
        const auto idx = (size_t) id;
        return ctl.motionMatrixOn ? modulated (id) : loadParam (modTargetPtrs[idx]);
    };

    gf::entropy::Params entropyParams;
    const float identity = ctl.identityLossOn ? juce::jlimit (0.0f, 1.0f, ctl.identityLoss * loadParam (p.identityLossMix, 1.0f)) : 0.0f;
    entropyParams.frozen = isOn (p.frozen);
    entropyParams.grainSize = target (gf::ParamId::grainSize) * (1.0f + identity * 0.35f);
    entropyParams.density = target (gf::ParamId::density) * (1.0f + ctl.chaos * 0.5f + ctl.texture * 0.75f + identity * 0.8f);
    entropyParams.pitch = target (gf::ParamId::pitch);
    entropyParams.noteOffset = ctl.midiEnabled ? noteOffset : 0.0f; // grains only follow MIDI when enabled
    entropyParams.spray = target (gf::ParamId::spray) * (1.0f + ctl.chaos * 0.6f) + ctl.texture * 300.0f + identity * 1200.0f;
    entropyParams.spread = target (gf::ParamId::spread);
    entropyParams.position = target (gf::ParamId::position);
    entropyParams.pitchJitter = target (gf::ParamId::pitchJitter) + ctl.chaos * 2.0f + identity * 24.0f;
    entropyParams.output = target (gf::ParamId::output);
    entropyParams.maxGrains = ctl.maxGrains;
    entropyParams.velocity = midiVelocity;
    entropyParams.velToAmp = loadParam (p.velToAmp);
    const float oldSpecMix = isOn (p.specFreeze) ? loadParam (p.specMix) : 0.0f;
    const float newSpecMix = ctl.spectralOn ? loadParam (p.spectralMix) * juce::jmax (0.001f, loadParam (p.spectralAmount, 1.0f)) : 0.0f;
    entropyParams.spectralFreeze = isOn (p.specFreeze) || ctl.spectralOn || identity > 0.65f;
    entropyParams.spectralMix = juce::jmax (oldSpecMix, juce::jmax (newSpecMix, identity * 0.45f));
    entropyParams.spectralShimmer = juce::jlimit (0.0f, 1.0f, loadParam (p.specShimmer, 0.2f) + identity * 0.35f);
    entropyParams.reverbMix = juce::jlimit (0.0f, 1.0f, target (gf::ParamId::reverbMix) + ctl.space * 0.35f);
    entropyParams.satOn = isOn (p.satOn) || ctl.damageOn || identity > 0.5f;
    entropyParams.satType = (int) loadParam (p.satType);
    entropyParams.satDrive = loadParam (p.satDrive, 1.0f) * (1.0f + ctl.damage * 4.0f + identity * 3.0f);
    entropyParams.satMix = juce::jmax (loadParam (p.satMix), juce::jmax (ctl.damage * 0.35f, identity * 0.25f));

    gf::pretty::Params prettyParams;
    prettyParams.enabled = beautyActive;
    prettyParams.inputTrim = loadParam (p.prettifierInTrim, 1.0f);
    prettyParams.outputTrim = loadParam (p.prettifierOutTrim, 1.0f);
    prettyParams.echoOn = isOn (p.echoOn) && beautyActive;
    prettyParams.echoTimeMs = target (gf::ParamId::echoTime);
    prettyParams.echoFeedback = target (gf::ParamId::echoFeedback);
    prettyParams.echoMix = target (gf::ParamId::echoMix);
    prettyParams.reverbOn = isOn (p.prettyReverbOn) && isOn (p.reverbOn, true) && beautyActive;
    prettyParams.reverbSize = loadParam (p.prettyReverbSize, 0.65f);
    prettyParams.reverbDamping = loadParam (p.prettyReverbDamping, 0.4f);
    prettyParams.reverbMix = juce::jlimit (0.0f, 1.0f, target (gf::ParamId::prettyReverbMix) + ctl.space * 0.45f);
    prettyParams.chorusOn = isOn (p.chorusOn) && beautyActive;
    prettyParams.chorusRate = target (gf::ParamId::chorusRate);
    prettyParams.chorusDepth = target (gf::ParamId::chorusDepth);
    prettyParams.chorusMix = juce::jlimit (0.0f, 1.0f, loadParam (p.chorusMix) + ctl.motion * 0.25f);
    prettyParams.beautyOn = isOn (p.beautyOn) && beautyActive;
    prettyParams.beautyAmount = juce::jlimit (0.0f, 1.0f, target (gf::ParamId::beautyAmount) + ctl.beauty * 0.5f);
    prettyParams.beautyAir = loadParam (p.beautyAir, 0.3f);
    prettyParams.beautyWarmth = loadParam (p.beautyWarmth, 0.35f);
    prettyParams.polishOn = isOn (p.polishOn) && beautyActive;
    prettyParams.width = target (gf::ParamId::polishWidth);
    prettyParams.crushOn = isOn (p.crushOn) || ctl.damageOn || identity > 0.55f;
    prettyParams.crushBits = juce::jmin (target (gf::ParamId::bitCrush), 24.0f - (ctl.damage * 12.0f + identity * 16.0f));
    prettyParams.crushMix = juce::jmax (loadParam (p.crushMix), juce::jmax (ctl.damage * 0.75f, identity * 0.55f));
    prettyParams.air = loadParam (p.polishAir, 0.2f);
    prettyParams.warmth = loadParam (p.polishWarmth, 0.2f);
    prettyParams.harshnessTame = loadParam (p.polishHarshnessTame, 0.1f);
    prettyParams.polishMix = loadParam (p.polishMix, 1.0f);
    prettyParams.dnaCharacter = loadParam (p.dnaCharacter, 0.5f);
    prettyParams.dnaAge = loadParam (p.dnaAge, 0.2f);
    prettyParams.dnaWarmth = loadParam (p.dnaWarmth, 0.5f);
    prettyParams.dnaWidth = loadParam (p.dnaWidth, 0.5f);
    prettyParams.dnaRandomness = loadParam (p.dnaRandomness);
    prettyParams.dnaAnalog = loadParam (p.dnaAnalog, 0.5f);
    prettyParams.dnaDigital = loadParam (p.dnaDigital, 0.5f);
    prettyParams.dnaSmoothness = loadParam (p.dnaSmoothness, 0.5f);
    prettyParams.dnaMotion = loadParam (p.dnaMotion, 0.5f);
    prettyParams.dnaShine = loadParam (p.dnaShine, 0.5f);

    double hostBpm = 120.0;
    if (auto* ph = getPlayHead())
    {
        juce::AudioPlayHead::CurrentPositionInfo info;
        if (ph->getCurrentPosition (info) && info.bpm > 0.0)
            hostBpm = info.bpm;
    }
    if (textureActive && beautyActive && routingModeValue == 1) // Texture / Grain -> Beauty & Space
    {
        entropyBuffer.makeCopyOf (buffer, true);
        entropyEngine.pushInput (entropyBuffer);
        entropyEngine.process (entropyBuffer, entropyParams);
        prettifierBuffer.makeCopyOf (entropyBuffer, true);
        prettifierEngine.process (prettifierBuffer, prettyParams, hostBpm, ctl.tempoLockOn);
    }
    else if (textureActive && beautyActive && routingModeValue == 2) // Beauty & Space -> Texture / Grain
    {
        prettifierBuffer.makeCopyOf (buffer, true);
        prettifierEngine.process (prettifierBuffer, prettyParams, hostBpm, ctl.tempoLockOn);
        entropyBuffer.makeCopyOf (prettifierBuffer, true);
        entropyEngine.pushInput (entropyBuffer);
        entropyEngine.process (entropyBuffer, entropyParams);
    }
    else
    {
        if (textureActive)
        {
            entropyBuffer.makeCopyOf (buffer, true);
            entropyEngine.pushInput (entropyBuffer);
            entropyEngine.process (entropyBuffer, entropyParams);
        }
        if (beautyActive)
        {
            prettifierBuffer.makeCopyOf (buffer, true);
            prettifierEngine.process (prettifierBuffer, prettyParams, hostBpm, ctl.tempoLockOn);
        }
    }

    gf::mix::Params mixParams;
    mixParams.pluginOn = true;
    mixParams.entropyOn = textureActive;
    mixParams.prettifierOn = beautyActive;
    mixParams.mixEqOn = isOn (p.mixEqOn);
    mixParams.pitchMatchOn = pitchMatchOn;
    mixParams.tempoLockOn = ctl.tempoLockOn;
    mixParams.limiterOn = isOn (p.limiterOn, true);
    mixParams.dryLevel = juce::jmax (loadParam (p.dryLevel), 1.0f - ctl.dryWet);
    if (! textureActive && ! beautyActive)
        mixParams.dryLevel = 1.0f;
    mixParams.dryLevel *= 1.0f - identity * 0.75f;
    mixParams.entropySend = loadParam (p.entropySend, 1.0f);
    mixParams.entropyReturn = loadParam (p.entropyReturn, 1.0f) * juce::jmax (loadParam (p.textureGrainMix, 1.0f), identity);
    mixParams.prettifierSend = loadParam (p.prettifierSend, 1.0f);
    mixParams.prettifierReturn = loadParam (p.prettifierReturn, 1.0f) * loadParam (p.beautySpaceMix, 1.0f);
    mixParams.outputLevel = ctl.outputLevel;
    mixParams.chaosBeauty = juce::jlimit (0.0f, 1.0f, loadParam (p.chaosBeauty, 0.5f) + ctl.beauty * 0.25f - ctl.chaos * 0.25f);
    mixParams.routing = (gf::mix::RoutingMode) routingModeValue;
    mixParams.ceilingDb = loadParam (p.ceilingDb, -0.1f) - loadParam (p.macroGlue) * 2.0f;
    mixParams.eqLow  = loadParam (p.eqLow);
    mixParams.eqMid  = loadParam (p.eqMid);
    mixParams.eqHigh = loadParam (p.eqHigh);
    mixParams.eqLoFi = juce::jlimit (0.0f, 1.0f, loadParam (p.eqLoFi) + identity * 0.35f);
    mixParams.width  = juce::jlimit (0.0f, 2.0f, loadParam (p.mixWidth, 1.0f) + ctl.space * 0.35f);
    mixParams.pitchLockOn     = isOn (p.pitchLockOn) || ctl.pitchFormantOn;
    mixParams.pitchLockMode   = (int) loadParam (p.pitchLockMode, 1.0f);
    mixParams.pitchLockKey    = (int) loadParam (p.pitchLockKey);
    mixParams.pitchLockScale  = (int) loadParam (p.pitchLockScale, 1.0f);
    mixParams.pitchLockAmount = juce::jmax (loadParam (p.pitchLockAmount, 1.0f), ctl.pitchFormantOn ? loadParam (p.pitchFormantMix, 0.0f) : 0.0f);
    mixParams.pitchLockFormant = isOn (p.pitchLockFormant);


    // Master bus, minus the final dynamics (deferred until after Sample Mode).
    mixEngine.processParallel (buffer,
                               needsDry ? dryInBuffer : buffer,
                               entropyBuffer,
                               prettifierBuffer,
                               mixParams);

    // Sample Mode: capture the clean master (before our own playback, so the loop
    // can't feed back), service a pending Freeze, then layer the looping sample.
    if (ctl.sampleModeOn || sampleFreezePending)
    {
        sampleEngine.pushOutput (buffer);
        if (sampleFreezeRequested.exchange (false, std::memory_order_acquire))
            sampleEngine.freeze (loadParam (p.sampleWindow, 4.0f),
                                 (int) loadParam (p.sampleSource));
        if (ctl.sampleModeOn)
        {
            const float rate  = std::pow (2.0f, noteOffset / 12.0f); // keyboard transposes the loop
            const float level = loadParam (p.sampleLevel, 1.0f);
            sampleEngine.render (buffer, rate, level);
        }
    }

    // Final master dynamics over the whole program (mix + Sample Mode): glue
    // bus-comp then the ceiling limiter, so nothing escapes the ceiling.
    const float glueAmount = loadParam (p.mixGlue);
    mixEngine.processMasterDynamics (buffer, glueAmount, mixParams.ceilingDb, mixParams.limiterOn);

    // Output metering: peak per channel with a smooth decay, read by the UI.
    auto measure = [&buffer] (int ch) -> float
    {
        if (ch >= buffer.getNumChannels()) return 0.0f;
        return buffer.getMagnitude (ch, 0, buffer.getNumSamples());
    };
    const float decay = 0.85f;
    const float pL = measure (0);
    const float pR = channels > 1 ? measure (1) : pL;
    outLevelL.store (juce::jmax (pL, outLevelL.load (std::memory_order_relaxed) * decay),
                     std::memory_order_relaxed);
    outLevelR.store (juce::jmax (pR, outLevelR.load (std::memory_order_relaxed) * decay),
                     std::memory_order_relaxed);

    if (ctl.waveformOn && channels > 0 && n > 0)
    {
        // Decimate at a fixed rate so the 256-point scope always spans the same
        // ~50 ms window, no matter what block size the host hands us.
        int head    = waveformHead.load (std::memory_order_relaxed);
        int counter = waveformDecimateCounter;
        for (int i = 0; i < n; ++i)
        {
            if (--counter <= 0)
            {
                waveformHistory[(size_t) head].store (buffer.getSample (0, i), std::memory_order_relaxed);
                head = (head + 1) % 256;
                counter = waveformDecimate;
            }
        }
        waveformHead.store (head, std::memory_order_relaxed);
        waveformDecimateCounter = counter;
    }

    // Spectrum analyzer: accumulate output samples and run an FFT each time the
    // fifo fills, mapping the magnitudes onto log-spaced bins for the display.
    if (ctl.analyzerOn && channels > 0 && n > 0)
    {
        for (int i = 0; i < n; ++i)
        {
            float mono = buffer.getSample (0, i);
            if (channels > 1) mono = 0.5f * (mono + buffer.getSample (1, i));
            spectrumFifo[(size_t) spectrumFifoIndex] = mono;
            if (++spectrumFifoIndex >= kFftSize)
            {
                spectrumFifoIndex = 0;
                // Hann window into the FFT scratch buffer.
                for (int k = 0; k < kFftSize; ++k)
                {
                    const float win = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::twoPi * (float) k / (float) (kFftSize - 1)));
                    spectrumFftData[(size_t) k] = spectrumFifo[(size_t) k] * win;
                }
                std::fill (spectrumFftData.begin() + kFftSize, spectrumFftData.end(), 0.0f);
                spectrumFft.performFrequencyOnlyForwardTransform (spectrumFftData.data());

                // Log-spaced bins from ~20 Hz to Nyquist, magnitude in dB -> 0..1.
                const int numFreq = kFftSize / 2;
                const float minHz = 20.0f, maxHz = (float) (currentSampleRate * 0.5);
                for (int b = 0; b < kSpectrumBins; ++b)
                {
                    const float f0 = minHz * std::pow (maxHz / minHz, (float) b / (float) kSpectrumBins);
                    const float f1 = minHz * std::pow (maxHz / minHz, (float) (b + 1) / (float) kSpectrumBins);
                    int k0 = juce::jlimit (1, numFreq - 1, (int) (f0 / maxHz * numFreq));
                    int k1 = juce::jlimit (k0 + 1, numFreq, (int) (f1 / maxHz * numFreq));
                    float mag = 0.0f;
                    for (int k = k0; k < k1; ++k) mag = juce::jmax (mag, spectrumFftData[(size_t) k]);
                    const float db = juce::Decibels::gainToDecibels (mag / (float) kFftSize + 1.0e-9f);
                    const float norm = juce::jlimit (0.0f, 1.0f, (db + 90.0f) / 90.0f);
                    spectrumBins[(size_t) b].store (norm, std::memory_order_relaxed);
                }
            }
        }
    }
}

void GrainFreezeProcessor::getSpectrumSnapshot (std::array<float, kSpectrumBins>& out) const
{
    for (int i = 0; i < kSpectrumBins; ++i)
        out[(size_t) i] = spectrumBins[(size_t) i].load (std::memory_order_relaxed);
}

void GrainFreezeProcessor::getModScopeSnapshot (std::array<float, 256>& out) const
{
    const int head = modScopeHead.load (std::memory_order_relaxed);
    for (int i = 0; i < 256; ++i)
    {
        const int idx = (head + i) % 256;
        out[(size_t) i] = modScopeHistory[(size_t) idx].load (std::memory_order_relaxed);
    }
}

juce::AudioProcessorEditor* GrainFreezeProcessor::createEditor()
{
    return new GrainFreezeEditor (*this);
}

void GrainFreezeProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
    {
        state.removeChild (slotA, nullptr);
        state.removeChild (slotB, nullptr);
        state.appendChild (slotA.createCopy(), nullptr);
        state.appendChild (slotB.createCopy(), nullptr);
        juce::MemoryOutputStream stream (destData, false);
        state.writeToStream (stream);
    }
}

void GrainFreezeProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto tree = juce::ValueTree::readFromData (data, (size_t) sizeInBytes);
    if (tree.isValid())
    {
        if (auto a = tree.getChildWithName ("AB_SLOT_A"); a.isValid()) slotA = a;
        if (auto b = tree.getChildWithName ("AB_SLOT_B"); b.isValid()) slotB = b;
        tree.removeChild (tree.getChildWithName ("AB_SLOT_A"), nullptr);
        tree.removeChild (tree.getChildWithName ("AB_SLOT_B"), nullptr);
        apvts.replaceState (tree);
    }
}

void GrainFreezeProcessor::storeSlotA() { slotA = apvts.copyState(); }
void GrainFreezeProcessor::storeSlotB() { slotB = apvts.copyState(); }
void GrainFreezeProcessor::copyAToB() { slotB = slotA.createCopy(); }
void GrainFreezeProcessor::copyBToA() { slotA = slotB.createCopy(); }
void GrainFreezeProcessor::loadSlotA() { preserveLocked ([this] { if (slotA.isValid()) apvts.replaceState (slotA.createCopy()); }); }
void GrainFreezeProcessor::loadSlotB() { preserveLocked ([this] { if (slotB.isValid()) apvts.replaceState (slotB.createCopy()); }); }
void GrainFreezeProcessor::resetSlotB()
{
    slotB = slotA.createCopy();
}

void GrainFreezeProcessor::preserveLocked (const std::function<void()>& op)
{
    // Snapshot the locked knobs, run the operation, then restore them.
    std::array<std::pair<bool, float>, (size_t) gf::kNumModParams> saved;
    for (int i = 0; i < gf::kNumModParams; ++i)
    {
        const bool locked = randomizer.isLocked ((gf::ParamId) i);
        const float v = locked ? apvts.getRawParameterValue (gf::paramIdString ((gf::ParamId) i))->load() : 0.0f;
        saved[(size_t) i] = { locked, v };
    }

    op();

    for (int i = 0; i < gf::kNumModParams; ++i)
    {
        if (! saved[(size_t) i].first) continue;
        const char* id = gf::paramIdString ((gf::ParamId) i);
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (apvts.getParameterRange (id).convertTo0to1 (saved[(size_t) i].second));
    }
}

void GrainFreezeProcessor::panicKillTail()
{
    entropyEngine.reset();
    prettifierEngine.panic();
    mixEngine.panic();
    midiCtrl.reset();
}

void GrainFreezeProcessor::getWaveformSnapshot (std::array<float, 256>& out) const
{
    const int head = waveformHead.load (std::memory_order_relaxed);
    for (int i = 0; i < 256; ++i)
    {
        const int idx = (head + i) % 256;
        out[(size_t) i] = waveformHistory[(size_t) idx].load (std::memory_order_relaxed);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GrainFreezeProcessor();
}
