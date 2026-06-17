#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_opengl/juce_opengl.h>   // macOS 26 (Tahoe) glyph-flip workaround
#include "GrainFreeze/PluginProcessor.h"
#include "GrainFreeze/ModPanel.h"
#include "GrainFreeze/Saturator.h"
#include "GrainFreeze/BiohazardLookAndFeel.h"
#include "GrainFreeze/Biohazard.h"
#include <array>

// Draws the saturation transfer curve for the current type + drive.
class SaturationCurve : public juce::Component
{
public:
    explicit SaturationCurve (juce::AudioProcessorValueTreeState& s) : apvts (s) {}
    void paint (juce::Graphics&) override;
private:
    juce::AudioProcessorValueTreeState& apvts;
};

// Two-channel vertical peak meter reading the processor's output level.
class LevelMeter : public juce::Component
{
public:
    explicit LevelMeter (GrainFreezeProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
private:
    GrainFreezeProcessor& proc;
};

// Scrolling oscilloscope of the global modulation source value over time.
// Reads a control-rate snapshot from the processor so the trace reflects the
// real LFO shape rather than whatever the UI timer happened to catch.
class ModScope : public juce::Component
{
public:
    explicit ModScope (GrainFreezeProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    static constexpr int kPoints = 256;
private:
    GrainFreezeProcessor& proc;
};

class WaveformMemoryDisplay : public juce::Component
{
public:
    explicit WaveformMemoryDisplay (GrainFreezeProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
private:
    GrainFreezeProcessor& proc;
};

// Master spectrum analyzer (Mix tab): log-frequency magnitude bars read from
// the processor's FFT snapshot.
class SpectrumDisplay : public juce::Component
{
public:
    explicit SpectrumDisplay (GrainFreezeProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
private:
    GrainFreezeProcessor& proc;
};

// A thin ring drawn around a rotary knob that visualizes live modulation.
// It reads the processor's current normalized mod offset (-1..1) for one
// parameter and sweeps an arc from the knob's base position.
class ModRing : public juce::Component
{
public:
    explicit ModRing (juce::Slider& s) : slider (s)
    {
        setInterceptsMouseClicks (false, false); // never steal clicks from the knob
        trail.fill (0.0f);
    }

    // Push the latest offset and advance the fading trail history.
    void setOffset (float normalized)
    {
        offset = normalized;
        trail[(size_t) trailHead] = normalized;
        trailHead = (trailHead + 1) % kTrailLen;
    }

    void paint (juce::Graphics&) override;

    static constexpr int kTrailLen = 10;

private:
    juce::Slider& slider;
    float         offset = 0.0f;
    std::array<float, kTrailLen> trail {};
    int           trailHead = 0;
};

// Transparent overlay that sits on top of all controls and paints a fading
// scrim used for the cross-fade when switching tabs.
class FadeOverlay : public juce::Component
{
public:
    FadeOverlay() { setInterceptsMouseClicks (false, false); }
    void setAmount (float a) { amount = a; setVisible (a > 0.001f); repaint(); }
    void paint (juce::Graphics&) override;
private:
    float amount = 0.0f;
};

// Searchable preset browser shown in a callout from the preset bar.
class PresetBrowser : public juce::Component,
                      private juce::ListBoxModel
{
public:
    PresetBrowser (juce::StringArray names, juce::String currentName,
                   std::function<void (juce::String)> onChoose);

    void resized() override;
    void paint (juce::Graphics&) override;

    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void returnKeyPressed (int lastRowSelected) override;

private:
    void applyFilter();
    void commit (int row);

    juce::Label       title;
    juce::TextEditor  search;
    juce::ListBox     list { "presets", this };
    juce::StringArray allNames, filtered;
    juce::String      current;
    std::function<void (juce::String)> choose;
};

class GrainFreezeEditor : public juce::AudioProcessorEditor,
                          private juce::Timer
{
public:
    explicit GrainFreezeEditor (GrainFreezeProcessor&);
    ~GrainFreezeEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;
    void updateTabVisibility();

    struct LabeledKnob
    {
        juce::Slider       slider;
        juce::Label        label;
        juce::ToggleButton lock { "L" };
        // Gear glyph (U+2699) so it reads clearly as a settings button.
        juce::TextButton   modButton { juce::String (juce::CharPointer_UTF8 ("\xe2\x9a\x99")) };
        gf::ParamId        id {};
        juce::String       paramID;
        std::unique_ptr<SliderAttachment> attachment;
        std::unique_ptr<ModRing>          ring;
    };

    void addKnob (LabeledKnob& k, gf::ParamId id, const juce::String& paramID, const juce::String& name);
    void openModPanel (LabeledKnob& k, const juce::String& name);
    void refreshPresetList();

    GrainFreezeProcessor& proc;

    gf::BiohazardLookAndFeel lnf;
    juce::TooltipWindow tooltipWindow { this, 600 }; // hover hints across the editor
    juce::Path watermark;
    juce::Image logoImage;
    juce::Image whiteLogoImage;   // white-tinted emblem for the Mix tab
    juce::Image blackLogoImage;   // black-tinted emblem for the Entropy tab
    juce::Image greenLogoImage;   // toxic-green emblem for the Entropy tab
    juce::Image bgImage;
    juce::Image mixBgImage;        // Mix-tab background artwork
    juce::Image prettifierBgImage;
    juce::Image prettifierLogoImage;
    float glowLevel = 0.0f;   // smoothed output level driving the centre glow
    float flicker   = 1.0f;   // multiplicative glow flicker (CRT instability)
    float bootPhase = 0.0f;   // 0..1 power-on animation, 1 = fully booted
    float tabFade   = 1.0f;   // 0..1 cross-fade when switching tabs
    juce::Random flickerRng;

    // Continuous background animation: a clock + a field of slowly drifting
    // "spores" that float behind the controls, tinted by the active tab accent.
    float animPhase = 0.0f;
    juce::Random animRng;
    struct Spore { float x = 0, y = 0, vx = 0, vy = 0, size = 0, phase = 0, twinkle = 0; };
    static constexpr int kNumSpores = 34;
    std::array<Spore, kNumSpores> spores {};
    bool sporesReady = false;
    void initSpores();

    juce::ToggleButton freezeButton { "Freeze" };
    std::unique_ptr<ButtonAttachment> freezeAttachment;

    juce::TextButton randomizeButton { "Randomize" };
    juce::TextButton saveButton      { "Save" };
    juce::TextButton browseButton    { "Browse" };
    juce::TextButton prevButton      { "<" };
    juce::TextButton nextButton      { ">" };
    juce::ComboBox   presetBox;
    juce::TextEditor presetName;
    // Tab buttons. Internal indices are unchanged (0 Texture, 1 Master, 2 Space,
    // 3 Machines); HOME is a new index 4. Visual order is set in resized().
    juce::TextButton tabHome { "HOME" };
    juce::TextButton tabEntropy { "TEXTURE" };
    juce::TextButton tabMachines { "MACHINES" };
    juce::TextButton tabMix { "MASTER" };
    juce::TextButton tabPrettifier { "SPACE" };
    int currentTab = 4;   // land on HOME

    // ---- HOME cockpit: the macros that drive the whole chain + a stage strip. ----
    static constexpr int kNumMacros = 7;
    std::array<juce::Slider, kNumMacros> macroKnobs;
    std::array<juce::Label,  kNumMacros> macroLabels;
    std::array<std::unique_ptr<SliderAttachment>, kNumMacros> macroAttach;
    juce::Label homeTitle, homeFlowLabel;
    // Signal-flow stage toggles (own attachments; sync with the deep-tab toggles).
    juce::ToggleButton homeTextureOn { "Texture" }, homeMachinesLabel { "Machines" },
                       homeSpaceOn { "Space" }, homeMasterLabel { "Master" };
    std::unique_ptr<ButtonAttachment> homeTextureAttach, homeSpaceAttach;
    int undoTxnCounter = 0;   // coarse undo-transaction grouping in the timer
    bool sampleReadyShown = false; // tracks the FREEZE button's armed-state tint

    // A/B compare controls.
    juce::TextButton buttonA { "A" };
    juce::TextButton buttonB { "B" };
    juce::TextButton copyAToBButton { "A " + juce::String (juce::CharPointer_UTF8 ("\xe2\x86\x92")) + " B" };
    juce::TextButton copyBToAButton { "B " + juce::String (juce::CharPointer_UTF8 ("\xe2\x86\x92")) + " A" };
    juce::TextButton resetBButton { "Reset B" };

    // Utility tools.
    juce::TextButton undoButton { juce::String (juce::CharPointer_UTF8 ("\xe2\x86\xb6")) }; // U+21B6
    juce::TextButton redoButton { juce::String (juce::CharPointer_UTF8 ("\xe2\x86\xb7")) }; // U+21B7
    juce::TextButton initButton { "Init" };
    juce::TextButton randomizeAllButton { "Randomize All" };
    juce::TextButton panicButton { "STOP" };

    // Global mod source + saturation bottom bar. The section headers double as
    // on/off toggles (Global Mod / Saturation).
    juce::Label    globalModLabel;
    juce::ToggleButton globalModOnButton { "Global Mod" };
    juce::Slider   globalRate;
    juce::ComboBox globalShape;
    juce::ComboBox lfoSyncBox;   // Free + tempo divisions for the Global Mod LFO
    juce::ComboBox echoSyncBox;  // Free + tempo divisions for the Echo time
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> lfoSyncAttach, echoSyncAttach;
    std::unique_ptr<SliderAttachment>   globalRateAttach;
    std::unique_ptr<ButtonAttachment>   globalModOnAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> globalShapeAttach;

    juce::Label    satLabel;
    juce::ToggleButton satOnButton { "Warmth" };
    juce::ComboBox satType;
    juce::Slider   satDrive, satMix;
    juce::Label    satTypeLabel, satDriveLabel, satMixLabel;
    std::unique_ptr<ButtonAttachment>   satOnAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> satTypeAttach;
    std::unique_ptr<SliderAttachment>   satDriveAttach, satMixAttach;

    std::unique_ptr<SaturationCurve> satCurve;
    std::unique_ptr<LevelMeter>      meter;
    std::unique_ptr<ModScope>        modScope;
    std::unique_ptr<WaveformMemoryDisplay> waveformDisplay;
    std::unique_ptr<SpectrumDisplay>       spectrumDisplay;

    // Spectral freeze controls.
    juce::ToggleButton specFreezeButton { "Spectral" };
    juce::Slider   specMix, specShimmer;
    juce::Label    specLabel;
    std::unique_ptr<ButtonAttachment> specFreezeAttach;
    std::unique_ptr<SliderAttachment> specMixAttach, specShimmerAttach;

    // MIDI play controls (Entropy tab) — bring the shared piano roll to life.
    // Enable routes keyboard/MIDI notes into the granular pitch; Root sets the
    // zero-transpose note, Glide the portamento time, Vel->Amp the dynamics.
    juce::ToggleButton midiEnableButton { "MIDI" };
    juce::Slider midiRootSlider, midiGlideSlider, midiVelAmpSlider;
    juce::Label  midiRootLabel, midiGlideLabel, midiVelAmpLabel;
    std::unique_ptr<ButtonAttachment> midiEnableAttach;
    std::unique_ptr<SliderAttachment> midiRootAttach, midiGlideAttach, midiVelAmpAttach;

    // Mix tab controls.
    juce::Slider dryLevel, entropySend, entropyReturn, prettifierSend, prettifierReturn, mixOutput, chaosBeauty, mixWidth, mixGlue, mixCeiling;
    juce::ComboBox routingMode;
    std::unique_ptr<SliderAttachment> dryLevelAttach, entropySendAttach, entropyReturnAttach, prettifierSendAttach, prettifierReturnAttach, mixOutputAttach, chaosBeautyAttach, mixWidthAttach, mixGlueAttach, mixCeilingAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> routingModeAttach;

    // Mix EQ: Low / Mid / High tone + Lo-Fi band-limit (enabled by the EQ toggle).
    juce::Slider eqLowKnob, eqMidKnob, eqHighKnob, eqLoFiKnob;
    std::unique_ptr<SliderAttachment> eqLowAttach, eqMidAttach, eqHighAttach, eqLoFiAttach;
    juce::Label eqHeader;
    std::array<juce::Label, 4> eqLabels;

    // Sample Mode (Mix tab): Freeze captures a moment of audio that then loops
    // and plays back from the keyboard. Source = Input/Output, Window = 1-10 s.
    juce::Label        sampleHeader;
    juce::ToggleButton sampleModeButton { "Sample Mode" };
    juce::TextButton   sampleFreezeButton { "FREEZE" };
    juce::ComboBox     sampleSourceBox;
    juce::Slider       sampleWindowSlider, sampleLevelSlider;
    juce::Label        sampleSourceLabel, sampleWindowLabel, sampleLevelLabel;
    std::unique_ptr<ButtonAttachment> sampleModeAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> sampleSourceAttach;
    std::unique_ptr<SliderAttachment> sampleWindowAttach, sampleLevelAttach;

    // Master Pitch Lock (Mix tab): tunes the whole output to Key + Scale and
    // overrides Pitch Match. Mode = Chromatic / Scale / Root.
    juce::Label        pitchLockHeader;
    juce::ToggleButton pitchLockButton { "Pitch Lock" };
    juce::ToggleButton pitchLockFormantButton { "Formant" };
    juce::ComboBox     pitchLockModeBox, pitchLockKeyBox, pitchLockScaleBox;
    juce::Slider       pitchLockAmountSlider;
    juce::Label        pitchLockModeLabel, pitchLockKeyLabel, pitchLockScaleLabel, pitchLockAmountLabel;
    std::unique_ptr<ButtonAttachment> pitchLockAttach, pitchLockFormantAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> pitchLockModeAttach, pitchLockKeyAttach, pitchLockScaleAttach;
    std::unique_ptr<SliderAttachment> pitchLockAmountAttach;

    // Global toggles.
    juce::ToggleButton pluginOnButton { "Plugin" };
    juce::ToggleButton entropyOnButton { "Texture / Grain" };
    juce::ToggleButton prettifierOnButton { "Beauty & Space" };
    juce::ToggleButton limiterOnButton { "Limiter" };
    juce::ToggleButton mixEqOnButton { "EQ" };
    juce::ToggleButton pitchMatchOnButton { "Pitch Match" };
    juce::ToggleButton tempoLockOnButton { "Tempo Lock" };
    std::unique_ptr<ButtonAttachment> pluginOnAttach, entropyOnAttach, prettifierOnAttach, limiterOnAttach, mixEqOnAttach, pitchMatchOnAttach, tempoLockOnAttach;

    // Prettifier controls: full LabeledKnobs (lock + mod + ring) just like Entropy.
    juce::Label prettifierHeader;
    static constexpr int kNumPrettyKnobs = 9;
    std::array<LabeledKnob, kNumPrettyKnobs> prettyKnobs;

    // Prettifier output gain (plain knob, fills the 10th grid cell).
    juce::Slider prettyOutputKnob;
    juce::Label  prettyOutputLabel;
    std::unique_ptr<SliderAttachment> prettyOutputAttach;

    // Prettifier "DNA" character bank + per-module on/off toggles. These were
    // built into the DSP from the start but had no UI until now.
    juce::Label dnaHeader;
    static constexpr int kNumDna = 10;
    std::array<juce::Slider, kNumDna> dnaKnobs;
    std::array<juce::Label,  kNumDna> dnaLabels;
    std::array<std::unique_ptr<SliderAttachment>, kNumDna> dnaAttach;

    juce::ToggleButton echoOnButton   { "Echo" },   reverbOnButton { "Reverb" },
                       chorusOnButton { "Chorus" }, crushOnButton  { "Lo-Fi" };
    std::array<std::unique_ptr<ButtonAttachment>, 4> moduleAttach;

    // ---- MACHINES tab: UI for the four transform machines. Each gets an On
    // toggle, a Mix knob, and its key parameters. Plain knobs (no lock/mod ring).
    juce::Label machinesHeader;
    juce::Label machSpectralTitle, machPitchTitle, machDamageTitle, machTimeTitle;

    juce::ToggleButton machSpectralOn { "On" };
    juce::Slider       machSpectralMix, machSpectralAmount;
    juce::Label        machSpectralMixL, machSpectralAmountL;

    juce::ToggleButton machPitchOn { "On" };
    juce::ToggleButton machPitchFormant { "Formant" };
    juce::Slider       machPitchMix, machPitchShift;
    juce::Label        machPitchMixL, machPitchShiftL;

    juce::ToggleButton machDamageOn { "On" };
    juce::ComboBox     machDamageClip;
    juce::Slider       machDamageMix, machDamageAmount, machDamageBits, machDamageRate,
                       machDamageJitter, machDamageNoise, machDamageDropout, machDamageTone;
    juce::Label        machDamageMixL, machDamageAmountL, machDamageBitsL, machDamageRateL,
                       machDamageJitterL, machDamageNoiseL, machDamageDropoutL, machDamageToneL;

    juce::ToggleButton machTimeOn { "On" };
    juce::ToggleButton machTimeSync { "Sync" };
    juce::ComboBox     machTimeDivision;
    juce::Slider       machTimeMix, machTimeRate, machTimeSize, machTimeChance, machTimeReverse;
    juce::Label        machTimeMixL, machTimeRateL, machTimeSizeL, machTimeChanceL, machTimeReverseL;
    // Time Breaker -> knob routing (2 slots: target + depth).
    juce::Label        machTimeRouteL;
    juce::ComboBox     machTimeRoute1Target, machTimeRoute2Target;
    juce::Slider       machTimeRoute1Depth, machTimeRoute2Depth;
    juce::Label        machTimeRoute1DepthL, machTimeRoute2DepthL;

    std::unique_ptr<ButtonAttachment> machSpectralOnAttach, machPitchOnAttach, machPitchFormantAttach,
                                      machDamageOnAttach, machTimeOnAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> machDamageClipAttach, machTimeDivisionAttach,
                                      machTimeRoute1TargetAttach, machTimeRoute2TargetAttach;
    std::unique_ptr<ButtonAttachment> machTimeSyncAttach;
    std::unique_ptr<SliderAttachment> machTimeRoute1DepthAttach, machTimeRoute2DepthAttach;
    std::unique_ptr<SliderAttachment> machSpectralMixAttach, machSpectralAmountAttach,
                                      machPitchMixAttach, machPitchShiftAttach,
                                      machDamageMixAttach, machDamageAmountAttach,
                                      machDamageBitsAttach, machDamageRateAttach, machDamageJitterAttach,
                                      machDamageNoiseAttach, machDamageDropoutAttach, machDamageToneAttach,
                                      machTimeMixAttach, machTimeRateAttach, machTimeSizeAttach,
                                      machTimeChanceAttach, machTimeReverseAttach;

    // Mix tab labels.
    juce::Label mixHeader;
    juce::Label routingLabel;
    std::array<juce::Label, 10> mixLabels;

    void layoutDialCell (juce::Rectangle<int>& row, juce::Label& label, juce::Slider& slider, int width);
    void switchTab (int tabIndex);

    std::unique_ptr<juce::MidiKeyboardComponent> keyboard;
    FadeOverlay fadeOverlay;

    // macOS 26 (Tahoe) CoreGraphics glyph-cache orientation bug renders some
    // text glyphs 180-degree-rotated. Routing the editor through the OpenGL
    // renderer bypasses the broken CoreGraphics text path. Remove this member
    // (and the attach/detach calls + juce_opengl link) once Apple/JUCE ship a
    // fix and CoreGraphics renders text correctly again.
    juce::OpenGLContext openGLContext;

    static constexpr int kNumKnobs = 9;
    std::array<LabeledKnob, kNumKnobs> knobs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GrainFreezeEditor)
};
