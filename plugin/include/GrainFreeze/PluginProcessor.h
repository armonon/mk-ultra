#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include "GrainFreeze/Entropy/EntropyEngine.h"
#include "GrainFreeze/Mix/MixEngine.h"
#include "GrainFreeze/Modulation/MidiNoteController.h"
#include "GrainFreeze/ModMatrix.h"
#include "GrainFreeze/Prettifier/PrettifierEngine.h"
#include "GrainFreeze/Presets/SnapshotManager.h"
#include "GrainFreeze/PresetManager.h"
#include "GrainFreeze/Randomizer.h"
#include "GrainFreeze/SampleFreeze.h"
#include "GrainFreeze/SpectralFreeze.h"
#include "GrainFreeze/TransformRack.h"

#ifndef MKULTRA_ENABLE_EXPERIMENTAL_INPUT_TOOLS
#define MKULTRA_ENABLE_EXPERIMENTAL_INPUT_TOOLS 0
#endif

inline constexpr bool kMkUltraExperimentalInputTools =
    MKULTRA_ENABLE_EXPERIMENTAL_INPUT_TOOLS != 0;

class GrainFreezeProcessor : public juce::AudioProcessor,
                             private juce::AudioProcessorValueTreeState::Listener,
                             private juce::AsyncUpdater
{
public:
    GrainFreezeProcessor();
    ~GrainFreezeProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MK-ULTRA"; }
    bool acceptsMidi() const override  { return kMkUltraExperimentalInputTools; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    juce::UndoManager undoManager;
    gf::entropy::EntropyEngine entropyEngine;
    gf::mix::MixEngine mixEngine;
    gf::pretty::PrettifierEngine prettifierEngine;
    gf::ModMatrix      modMatrix;
    // Dedicated transform machines (master-bus inserts, each gated by its own On).
    gf::SpectralFreeze      spectralMachine;
    gf::DamageEngine        damageMachine;
    gf::TimeBreakerEngine   timeBreaker;
    gf::PitchFormantMachine pitchFormantMachine;
    gf::PresetManager  presets { apvts };
    gf::SnapshotManager snapshots { apvts };
    gf::Randomizer     randomizer { apvts };
    juce::MidiKeyboardState keyboardState;

    // Live, normalized (-1..1) modulation offset for a parameter, for UI display.
    float getModOffset (gf::ParamId id) const { return modMatrix.getOffset (id); }

    // Post-saturation output level for metering (linear, per channel, decaying).
    float getOutputLevel (int channel) const
    {
        return channel == 0 ? outLevelL.load (std::memory_order_relaxed)
                            : outLevelR.load (std::memory_order_relaxed);
    }

    // Live value of the global modulation source (-1..1), for the mod visualizer.
    float getGlobalModValue() const { return globalModValue.load (std::memory_order_relaxed); }
    void getWaveformSnapshot (std::array<float, 256>& out) const;

    // Snapshot of the global mod source captured at control rate (~100 Hz),
    // decoupled from the UI timer so the scope shows the real LFO shape.
    void getModScopeSnapshot (std::array<float, 256>& out) const;

    // Master spectrum analyzer: 128 log-spaced magnitude bins (0..1), computed
    // from the output via a 2048-point FFT. Drives the Mix-tab analyzer display.
    static constexpr int kSpectrumBins = 128;
    void getSpectrumSnapshot (std::array<float, kSpectrumBins>& out) const;

    // A/B compare controls.
    void storeSlotA();
    void storeSlotB();
    void copyAToB();
    void copyBToA();
    void loadSlotA();
    void loadSlotB();
    void resetSlotB();

    // Panic clears long tails and buffers.
    void panicKillTail();

    // Run an operation (Init / preset load / A-B recall) while holding every
    // locked knob at its current value, so a lock means "never changed except
    // by my own hand."
    void preserveLocked (const std::function<void()>& op);

    // Sample Mode: the Freeze button asks the audio thread to snapshot the most
    // recent window of audio on its next block. ready() drives the UI lamp.
    void triggerSampleFreeze() { sampleFreezeRequested.store (true, std::memory_order_release); }
    bool sampleFreezeReady() const { return sampleEngine.ready(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void pullParameters();

    // Returns the parameter's base value with the mod-matrix offset applied,
    // clamped to the parameter's real range.
    float modulated (gf::ParamId id);

    // Copy modulation parameters from the APVTS into the mod-matrix slots.
    void syncModMatrix();

    // Cache the raw atomic pointers for every mod slot once, so syncModMatrix
    // (called at control rate) does no string building or map lookups on the
    // audio thread.
    void cacheModPointers();
    void cacheParameterPointers();

    struct ControlSnapshot
    {
        bool pluginOn = true;
        bool beautySpaceOn = true;
        bool textureGrainOn = true;
        bool identityLossOn = true;
        bool spectralOn = false;
        bool pitchFormantOn = false;
        bool timeBreakerOn = false;
        bool damageOn = false;
        bool motionMatrixOn = true;
        bool analyzerOn = true;
        bool waveformOn = true;
        bool modScopeOn = true;
        bool inputToolsOn = false;
        bool sampleModeOn = false;
        bool midiEnabled = false;
        bool tempoLockOn = false;

        int performanceMode = 1;
        int maxGrains = 32;
        int oversamplingMode = 1;

        float identityLoss = 0.0f;
        float mutationAmount = 0.35f;
        float beauty = 0.0f;
        float texture = 0.0f;
        float space = 0.0f;
        float motion = 0.0f;
        float damage = 0.0f;
        float chaos = 0.0f;
        float dryWet = 1.0f;
        float outputLevel = 1.0f;
    };
    ControlSnapshot makeControlSnapshot() const;

    struct ParameterPointers
    {
        std::atomic<float>* panic = nullptr;
        std::atomic<float>* pluginOn = nullptr;
        std::atomic<float>* performanceMode = nullptr;
        std::atomic<float>* analyzerOn = nullptr;
        std::atomic<float>* waveformOn = nullptr;
        std::atomic<float>* modScopeOn = nullptr;
        std::atomic<float>* ecoUiMode = nullptr;
        std::atomic<float>* oversamplingMode = nullptr;
        std::atomic<float>* experimentalInputToolsOn = nullptr;

        std::atomic<float>* beautySpaceOn = nullptr;
        std::atomic<float>* beautySpaceMix = nullptr;
        std::atomic<float>* beautySpaceAmount = nullptr;
        std::atomic<float>* textureGrainOn = nullptr;
        std::atomic<float>* textureGrainMix = nullptr;
        std::atomic<float>* textureGrainAmount = nullptr;
        std::atomic<float>* identityLossOn = nullptr;
        std::atomic<float>* identityLossMix = nullptr;
        std::atomic<float>* identityLoss = nullptr;
        std::atomic<float>* identityLossAmount = nullptr;
        std::atomic<float>* spectralOn = nullptr;
        std::atomic<float>* spectralMix = nullptr;
        std::atomic<float>* spectralAmount = nullptr;
        std::atomic<float>* pitchFormantOn = nullptr;
        std::atomic<float>* pitchFormantMix = nullptr;
        std::atomic<float>* timeBreakerOn = nullptr;
        std::atomic<float>* timeBreakerMix = nullptr;
        std::atomic<float>* stutterRate = nullptr;
        std::atomic<float>* stutterSize = nullptr;
        std::atomic<float>* stutterChance = nullptr;
        std::atomic<float>* reverseChance = nullptr;
        std::atomic<float>* damageOn = nullptr;
        std::atomic<float>* damageMix = nullptr;
        std::atomic<float>* damageAmount = nullptr;
        std::atomic<float>* motionMatrixOn = nullptr;
        std::atomic<float>* dryWet = nullptr;

        std::atomic<float>* macroBeauty = nullptr;
        std::atomic<float>* macroChaos = nullptr;
        std::atomic<float>* macroGlue = nullptr;
        std::atomic<float>* macroTexture = nullptr;
        std::atomic<float>* macroSpace = nullptr;
        std::atomic<float>* macroMotion = nullptr;
        std::atomic<float>* macroDamage = nullptr;
        std::atomic<float>* mutationAmount = nullptr;
        std::atomic<float>* globalRate = nullptr;
        std::atomic<float>* globalShape = nullptr;
        std::atomic<float>* globalModOn = nullptr;

        std::atomic<float>* sampleMode = nullptr;
        std::atomic<float>* sampleWindow = nullptr;
        std::atomic<float>* sampleSource = nullptr;
        std::atomic<float>* sampleLevel = nullptr;
        std::atomic<float>* midiEnable = nullptr;
        std::atomic<float>* midiRoot = nullptr;
        std::atomic<float>* glideTime = nullptr;
        std::atomic<float>* velToAmp = nullptr;

        std::atomic<float>* frozen = nullptr;
        std::atomic<float>* specFreeze = nullptr;
        std::atomic<float>* specMix = nullptr;
        std::atomic<float>* specShimmer = nullptr;
        std::atomic<float>* satOn = nullptr;
        std::atomic<float>* satType = nullptr;
        std::atomic<float>* satDrive = nullptr;
        std::atomic<float>* satMix = nullptr;

        std::atomic<float>* prettifierEnabled = nullptr;
        std::atomic<float>* prettifierInTrim = nullptr;
        std::atomic<float>* prettifierOutTrim = nullptr;
        std::atomic<float>* echoOn = nullptr;
        std::atomic<float>* reverbOn = nullptr;
        std::atomic<float>* prettyReverbOn = nullptr;
        std::atomic<float>* prettyReverbSize = nullptr;
        std::atomic<float>* prettyReverbDamping = nullptr;
        std::atomic<float>* chorusOn = nullptr;
        std::atomic<float>* chorusMix = nullptr;
        std::atomic<float>* beautyOn = nullptr;
        std::atomic<float>* beautyAir = nullptr;
        std::atomic<float>* beautyWarmth = nullptr;
        std::atomic<float>* polishOn = nullptr;
        std::atomic<float>* polishAir = nullptr;
        std::atomic<float>* polishWarmth = nullptr;
        std::atomic<float>* polishHarshnessTame = nullptr;
        std::atomic<float>* polishMix = nullptr;
        std::atomic<float>* crushOn = nullptr;
        std::atomic<float>* crushMix = nullptr;
        std::atomic<float>* dnaCharacter = nullptr;
        std::atomic<float>* dnaAge = nullptr;
        std::atomic<float>* dnaWarmth = nullptr;
        std::atomic<float>* dnaWidth = nullptr;
        std::atomic<float>* dnaRandomness = nullptr;
        std::atomic<float>* dnaAnalog = nullptr;
        std::atomic<float>* dnaDigital = nullptr;
        std::atomic<float>* dnaSmoothness = nullptr;
        std::atomic<float>* dnaMotion = nullptr;
        std::atomic<float>* dnaShine = nullptr;

        std::atomic<float>* routingMode = nullptr;
        std::atomic<float>* entropyOn = nullptr;
        std::atomic<float>* prettifierOn = nullptr;
        std::atomic<float>* limiterOn = nullptr;
        std::atomic<float>* mixEqOn = nullptr;
        std::atomic<float>* pitchMatchOn = nullptr;
        std::atomic<float>* tempoLockOn = nullptr;
        std::atomic<float>* dryLevel = nullptr;
        std::atomic<float>* entropySend = nullptr;
        std::atomic<float>* entropyReturn = nullptr;
        std::atomic<float>* prettifierSend = nullptr;
        std::atomic<float>* prettifierReturn = nullptr;
        std::atomic<float>* mixOutput = nullptr;
        std::atomic<float>* chaosBeauty = nullptr;
        std::atomic<float>* ceilingDb = nullptr;
        std::atomic<float>* eqLow = nullptr;
        std::atomic<float>* eqMid = nullptr;
        std::atomic<float>* eqHigh = nullptr;
        std::atomic<float>* eqLoFi = nullptr;
        std::atomic<float>* mixWidth = nullptr;
        std::atomic<float>* mixGlue = nullptr;
        std::atomic<float>* pitchLockOn = nullptr;
        std::atomic<float>* pitchLockMode = nullptr;
        std::atomic<float>* pitchLockKey = nullptr;
        std::atomic<float>* pitchLockScale = nullptr;
        std::atomic<float>* pitchLockAmount = nullptr;
        std::atomic<float>* pitchLockFormant = nullptr;
    };
    ParameterPointers paramPtrs;
    juce::RangedAudioParameter* panicParameter = nullptr;
    struct ModSlotPtrs
    {
        std::atomic<float>* lfoRate = nullptr; std::atomic<float>* lfoDepth = nullptr;
        std::atomic<float>* lfoShape = nullptr; std::atomic<float>* shRate = nullptr;
        std::atomic<float>* shDepth = nullptr;  std::atomic<float>* globalAmt = nullptr;
        std::atomic<float>* bipolar = nullptr;  std::atomic<float>* rangeMin = nullptr;
        std::atomic<float>* rangeMax = nullptr; std::atomic<float>* skew = nullptr;
    };
    std::array<ModSlotPtrs, (size_t) gf::kNumModParams> modPtrs;
    std::array<std::atomic<float>*, (size_t) gf::kNumModParams> modTargetPtrs {};
    std::array<juce::NormalisableRange<float>, (size_t) gf::kNumModParams> modTargetRanges {};

    // Latency reporting: the formant-preserving shifter adds lookahead when on.
    void parameterChanged (const juce::String& id, float value) override;
    void handleAsyncUpdate() override;
    std::atomic<bool> formantLatencyActive { false };

    int    controlRateDivider = 0;   // counts samples between mod-matrix ticks
    int    controlRateSamples = 441; // ~100 Hz control rate at 44.1k

    double currentSampleRate = 44100.0;
    // Output scope shows a fixed ~50 ms window regardless of host block size:
    // we store every Nth output sample so 256 points always span the same time.
    int    waveformDecimate = 8;     // samples between stored scope points
    int    waveformDecimateCounter = 0;

    gf::MidiNoteController midiCtrl;
    gf::SampleFreezeEngine sampleEngine;
    std::atomic<bool> sampleFreezeRequested { false };
    // Spectral machine: capture a fresh spectrum for a short window each time it
    // is switched on, then hold (freeze) it.
    int  spectralCaptureLeft = 0;
    bool spectralWasOn = false;
    juce::AudioBuffer<float> dryInBuffer;
    juce::AudioBuffer<float> entropyBuffer;
    juce::AudioBuffer<float> prettifierBuffer;
    juce::ValueTree slotA { "AB_SLOT_A" };
    juce::ValueTree slotB { "AB_SLOT_B" };

    std::atomic<float> outLevelL { 0.0f };
    std::atomic<float> outLevelR { 0.0f };
    std::atomic<float> globalModValue { 0.0f };
    std::array<std::atomic<float>, 256> waveformHistory {};
    std::atomic<int> waveformHead { 0 };
    std::array<std::atomic<float>, 256> modScopeHistory {};
    std::atomic<int> modScopeHead { 0 };

    // Spectrum analyzer FFT (audio-thread fill, UI-thread read via atomics).
    static constexpr int kFftOrder = 11;
    static constexpr int kFftSize = 1 << kFftOrder; // 2048
    juce::dsp::FFT spectrumFft { kFftOrder };
    std::array<float, kFftSize> spectrumFifo {};
    std::array<float, 2 * kFftSize> spectrumFftData {};
    int spectrumFifoIndex = 0;
    std::array<std::atomic<float>, kSpectrumBins> spectrumBins {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GrainFreezeProcessor)
};
