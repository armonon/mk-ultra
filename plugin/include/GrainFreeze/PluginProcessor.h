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

    const juce::String getName() const override { return "GrainFreeze"; }
    bool acceptsMidi() const override  { return true; }
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
    float modulated (gf::ParamId id, const char* paramID);

    // Copy modulation parameters from the APVTS into the mod-matrix slots.
    void syncModMatrix();

    // Cache the raw atomic pointers for every mod slot once, so syncModMatrix
    // (called at control rate) does no string building or map lookups on the
    // audio thread.
    void cacheModPointers();
    struct ModSlotPtrs
    {
        std::atomic<float>* lfoRate = nullptr; std::atomic<float>* lfoDepth = nullptr;
        std::atomic<float>* lfoShape = nullptr; std::atomic<float>* shRate = nullptr;
        std::atomic<float>* shDepth = nullptr;  std::atomic<float>* globalAmt = nullptr;
        std::atomic<float>* bipolar = nullptr;  std::atomic<float>* rangeMin = nullptr;
        std::atomic<float>* rangeMax = nullptr; std::atomic<float>* skew = nullptr;
    };
    std::array<ModSlotPtrs, (size_t) gf::kNumModParams> modPtrs;

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
