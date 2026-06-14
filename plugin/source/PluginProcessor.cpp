#include "GrainFreeze/PluginProcessor.h"
#include "GrainFreeze/PluginEditor.h"
#include <array>

namespace pid
{
constexpr auto grainSize = "grainSize";
constexpr auto density = "density";
constexpr auto pitch = "pitch";
constexpr auto spray = "spray";
constexpr auto spread = "spread";
constexpr auto position = "position";
constexpr auto pitchJitter = "pitchJitter";
constexpr auto reverbMix = "reverbMix";
constexpr auto output = "output";

template <typename T>
T param (juce::AudioProcessorValueTreeState& apvts, const char* id)
{
    return (T) apvts.getRawParameterValue (id)->load();
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
    cacheModPointers();
    apvts.addParameterListener ("pitchLockFormant", this);
}

GrainFreezeProcessor::~GrainFreezeProcessor()
{
    apvts.removeParameterListener ("pitchLockFormant", this);
}

void GrainFreezeProcessor::cacheModPointers()
{
    for (int i = 0; i < gf::kNumModParams; ++i)
    {
        const juce::String base (gf::paramIdString ((gf::ParamId) i));
        auto& p = modPtrs[(size_t) i];
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
        "macroMotion", "macroSpace", "macroGlue", "macroMorph"
    };
    static constexpr const char* macroNames[] = {
        "Macro Beauty", "Macro Chaos", "Macro Emotion", "Macro Damage",
        "Macro Motion", "Macro Space", "Macro Glue", "Macro Morph"
    };
    for (size_t i = 0; i < std::size (macroIds); ++i)
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { macroIds[i], 1 }, macroNames[i],
            NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "randomMode", 1 }, "Random Mode",
        StringArray { "Subtle", "Musical", "Glitch", "Ambient", "Horror", "Destroyed", "Cinematic", "Beautiful", "Dream", "Angel", "Vintage" }, 1));
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
    formantLatencyActive.store (apvts.getRawParameterValue ("pitchLockFormant")->load() > 0.5f,
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

float GrainFreezeProcessor::modulated (gf::ParamId id, const char* paramID)
{
    const float base   = apvts.getRawParameterValue (paramID)->load();
    const float offset = modMatrix.getOffset (id);            // -1..1
    const auto& range  = apvts.getParameterRange (paramID);
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
    modMatrix.setGlobalRate    (apvts.getRawParameterValue ("globalRate")->load());
    modMatrix.setGlobalShape   ((int) apvts.getRawParameterValue ("globalShape")->load());
    modMatrix.setGlobalEnabled (apvts.getRawParameterValue ("globalModOn")->load() > 0.5f);
}

void GrainFreezeProcessor::pullParameters()
{
    using PI = gf::ParamId;
    juce::ignoreUnused (modulated (PI::grainSize,   pid::grainSize));
    juce::ignoreUnused (modulated (PI::density,     pid::density));
    juce::ignoreUnused (modulated (PI::pitch,       pid::pitch));
    juce::ignoreUnused (modulated (PI::spray,       pid::spray));
    juce::ignoreUnused (modulated (PI::spread,      pid::spread));
    juce::ignoreUnused (modulated (PI::position,    pid::position));
    juce::ignoreUnused (modulated (PI::pitchJitter, pid::pitchJitter));
    juce::ignoreUnused (modulated (PI::output,      pid::output));
}

void GrainFreezeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int channels = buffer.getNumChannels();
    const int n = buffer.getNumSamples();
    keyboardState.processNextMidiBuffer (midi, 0, n, true);

    if (apvts.getRawParameterValue ("panic")->load() > 0.5f)
    {
        panicKillTail();
        if (auto* p = apvts.getParameter ("panic"))
            p->setValueNotifyingHost (0.0f);
    }

    dryInBuffer.makeCopyOf (buffer, true);
    entropyBuffer.makeCopyOf (buffer, true);
    prettifierBuffer.makeCopyOf (buffer, true);

    // Sample Mode records the raw input continuously so Freeze can grab it.
    const bool sampleModeOn = apvts.getRawParameterValue ("sampleMode")->load() > 0.5f;
    sampleEngine.pushInput (dryInBuffer);

    // Advance the mod matrix in control-rate steps across the block. We tick at
    // least once per block, and pull modulated parameters after each tick so the
    // engine tracks modulation without per-sample overhead.
    int processed = 0;
    float noteOffset = 0.0f;
    float midiVelocity = 1.0f;
    const bool midiEnabled = apvts.getRawParameterValue ("midiEnable")->load() > 0.5f;
    // The keyboard drives the grains (when MIDI is enabled) and/or the frozen
    // sample (when Sample Mode is on), so run the note controller for either.
    const bool keyboardActive = midiEnabled || sampleModeOn;
    if (keyboardActive)
    {
        midiCtrl.setRoot ((int) apvts.getRawParameterValue ("midiRoot")->load());
        midiCtrl.setGlideMs (apvts.getRawParameterValue ("glideTime")->load());
        midiCtrl.updateGlideTime();
        for (const auto meta : midi)
            midiCtrl.handleMessage (meta.getMessage());
        noteOffset = midiCtrl.nextOffsetSemis();
        midiVelocity = midiCtrl.getVelocity();
    }

    do
    {
        if (controlRateDivider <= 0)
        {
            syncModMatrix();
            modMatrix.tick();
            const float gmv = modMatrix.getGlobalValue();
            globalModValue.store (gmv, std::memory_order_relaxed);

            // Push the mod value into the scope ring at the true control rate.
            int mh = modScopeHead.load (std::memory_order_relaxed);
            modScopeHistory[(size_t) mh].store (gmv, std::memory_order_relaxed);
            modScopeHead.store ((mh + 1) % 256, std::memory_order_relaxed);

            pullParameters();
            controlRateDivider = controlRateSamples;
            if (keyboardActive)
                noteOffset = midiCtrl.nextOffsetSemis();
        }
        const int step = juce::jmin (controlRateDivider, n - processed);
        controlRateDivider -= step;
        processed += step;
    } while (processed < n);

    gf::entropy::Params entropyParams;
    const float macroBeauty = apvts.getRawParameterValue ("macroBeauty")->load();
    const float macroChaos = apvts.getRawParameterValue ("macroChaos")->load();
    const float macroGlue = apvts.getRawParameterValue ("macroGlue")->load();
    entropyParams.frozen = apvts.getRawParameterValue ("frozen")->load() > 0.5f;
    entropyParams.grainSize = modulated (gf::ParamId::grainSize, pid::grainSize);
    entropyParams.density = modulated (gf::ParamId::density, pid::density) * (1.0f + macroChaos * 0.5f);
    entropyParams.pitch = modulated (gf::ParamId::pitch, pid::pitch);
    entropyParams.noteOffset = midiEnabled ? noteOffset : 0.0f; // grains only follow MIDI when enabled
    entropyParams.spray = modulated (gf::ParamId::spray, pid::spray) * (1.0f + macroChaos * 0.6f);
    entropyParams.spread = modulated (gf::ParamId::spread, pid::spread);
    entropyParams.position = modulated (gf::ParamId::position, pid::position);
    entropyParams.pitchJitter = modulated (gf::ParamId::pitchJitter, pid::pitchJitter) + macroChaos * 2.0f;
    entropyParams.output = modulated (gf::ParamId::output, pid::output);
    entropyParams.velocity = midiVelocity;
    entropyParams.velToAmp = apvts.getRawParameterValue ("velToAmp")->load();
    entropyParams.spectralFreeze = apvts.getRawParameterValue ("specFreeze")->load() > 0.5f;
    entropyParams.spectralMix = apvts.getRawParameterValue ("specMix")->load();
    entropyParams.spectralShimmer = apvts.getRawParameterValue ("specShimmer")->load();
    entropyParams.reverbMix = apvts.getRawParameterValue (pid::reverbMix)->load();
    entropyParams.satOn = apvts.getRawParameterValue ("satOn")->load() > 0.5f;
    entropyParams.satType = (int) apvts.getRawParameterValue ("satType")->load();
    entropyParams.satDrive = apvts.getRawParameterValue ("satDrive")->load();
    entropyParams.satMix = apvts.getRawParameterValue ("satMix")->load();

    entropyEngine.pushInput (entropyBuffer);
    const int routingModeValue = (int) apvts.getRawParameterValue ("routingMode")->load();
    entropyEngine.process (entropyBuffer, entropyParams);

    gf::pretty::Params prettyParams;
    prettyParams.enabled = apvts.getRawParameterValue ("prettifierEnabled")->load() > 0.5f;
    prettyParams.inputTrim = apvts.getRawParameterValue ("prettifierInTrim")->load();
    prettyParams.outputTrim = apvts.getRawParameterValue ("prettifierOutTrim")->load();
    prettyParams.echoOn = apvts.getRawParameterValue ("echoOn")->load() > 0.5f;
    prettyParams.echoTimeMs = modulated (gf::ParamId::echoTime, "echoTimeMs");
    prettyParams.echoFeedback = modulated (gf::ParamId::echoFeedback, "echoFeedback");
    prettyParams.echoMix = modulated (gf::ParamId::echoMix, "echoMix");
    prettyParams.reverbOn = apvts.getRawParameterValue ("prettyReverbOn")->load() > 0.5f;
    prettyParams.reverbSize = apvts.getRawParameterValue ("prettyReverbSize")->load();
    prettyParams.reverbDamping = apvts.getRawParameterValue ("prettyReverbDamping")->load();
    prettyParams.reverbMix = modulated (gf::ParamId::prettyReverbMix, "prettyReverbMix");
    prettyParams.chorusOn = apvts.getRawParameterValue ("chorusOn")->load() > 0.5f;
    prettyParams.chorusRate = modulated (gf::ParamId::chorusRate, "chorusRate");
    prettyParams.chorusDepth = modulated (gf::ParamId::chorusDepth, "chorusDepth");
    prettyParams.chorusMix = apvts.getRawParameterValue ("chorusMix")->load();
    prettyParams.beautyOn = apvts.getRawParameterValue ("beautyOn")->load() > 0.5f;
    prettyParams.beautyAmount = juce::jlimit (0.0f, 1.0f, modulated (gf::ParamId::beautyAmount, "beautyAmount") + macroBeauty * 0.5f);
    prettyParams.beautyAir = apvts.getRawParameterValue ("beautyAir")->load();
    prettyParams.beautyWarmth = apvts.getRawParameterValue ("beautyWarmth")->load();
    prettyParams.polishOn = apvts.getRawParameterValue ("polishOn")->load() > 0.5f;
    prettyParams.width = modulated (gf::ParamId::polishWidth, "polishWidth");
    prettyParams.crushOn = apvts.getRawParameterValue ("crushOn")->load() > 0.5f;
    prettyParams.crushBits = modulated (gf::ParamId::bitCrush, "crushBits");
    prettyParams.crushMix = apvts.getRawParameterValue ("crushMix")->load();
    prettyParams.air = apvts.getRawParameterValue ("polishAir")->load();
    prettyParams.warmth = apvts.getRawParameterValue ("polishWarmth")->load();
    prettyParams.harshnessTame = apvts.getRawParameterValue ("polishHarshnessTame")->load();
    prettyParams.polishMix = apvts.getRawParameterValue ("polishMix")->load();
    prettyParams.dnaCharacter = apvts.getRawParameterValue ("dnaCharacter")->load();
    prettyParams.dnaAge = apvts.getRawParameterValue ("dnaAge")->load();
    prettyParams.dnaWarmth = apvts.getRawParameterValue ("dnaWarmth")->load();
    prettyParams.dnaWidth = apvts.getRawParameterValue ("dnaWidth")->load();
    prettyParams.dnaRandomness = apvts.getRawParameterValue ("dnaRandomness")->load();
    prettyParams.dnaAnalog = apvts.getRawParameterValue ("dnaAnalog")->load();
    prettyParams.dnaDigital = apvts.getRawParameterValue ("dnaDigital")->load();
    prettyParams.dnaSmoothness = apvts.getRawParameterValue ("dnaSmoothness")->load();
    prettyParams.dnaMotion = apvts.getRawParameterValue ("dnaMotion")->load();
    prettyParams.dnaShine = apvts.getRawParameterValue ("dnaShine")->load();

    double hostBpm = 120.0;
    if (auto* ph = getPlayHead())
    {
        juce::AudioPlayHead::CurrentPositionInfo info;
        if (ph->getCurrentPosition (info) && info.bpm > 0.0)
            hostBpm = info.bpm;
    }
    const bool tempoLockOn = apvts.getRawParameterValue ("tempoLockOn")->load() > 0.5f;
    if (routingModeValue == 1) // Entropy -> Prettifier
    {
        prettifierBuffer.makeCopyOf (entropyBuffer, true);
        prettifierEngine.process (prettifierBuffer, prettyParams, hostBpm, tempoLockOn);
    }
    else if (routingModeValue == 2) // Prettifier -> Entropy
    {
        prettifierEngine.process (prettifierBuffer, prettyParams, hostBpm, tempoLockOn);
        entropyBuffer.makeCopyOf (prettifierBuffer, true);
        entropyEngine.pushInput (entropyBuffer);
        entropyEngine.process (entropyBuffer, entropyParams);
    }
    else
    {
        prettifierEngine.process (prettifierBuffer, prettyParams, hostBpm, tempoLockOn);
    }

    gf::mix::Params mixParams;
    mixParams.pluginOn = apvts.getRawParameterValue ("pluginOn")->load() > 0.5f;
    mixParams.entropyOn = apvts.getRawParameterValue ("entropyOn")->load() > 0.5f;
    mixParams.prettifierOn = apvts.getRawParameterValue ("prettifierOn")->load() > 0.5f;
    mixParams.mixEqOn = apvts.getRawParameterValue ("mixEqOn")->load() > 0.5f;
    mixParams.pitchMatchOn = apvts.getRawParameterValue ("pitchMatchOn")->load() > 0.5f;
    mixParams.tempoLockOn = tempoLockOn;
    mixParams.limiterOn = apvts.getRawParameterValue ("limiterOn")->load() > 0.5f;
    mixParams.dryLevel = apvts.getRawParameterValue ("dryLevel")->load();
    mixParams.entropySend = apvts.getRawParameterValue ("entropySend")->load();
    mixParams.entropyReturn = apvts.getRawParameterValue ("entropyReturn")->load();
    mixParams.prettifierSend = apvts.getRawParameterValue ("prettifierSend")->load();
    mixParams.prettifierReturn = apvts.getRawParameterValue ("prettifierReturn")->load();
    mixParams.outputLevel = apvts.getRawParameterValue ("mixOutput")->load();
    mixParams.chaosBeauty = apvts.getRawParameterValue ("chaosBeauty")->load();
    mixParams.routing = (gf::mix::RoutingMode) (int) apvts.getRawParameterValue ("routingMode")->load();
    mixParams.ceilingDb = apvts.getRawParameterValue ("ceilingDb")->load() - macroGlue * 2.0f;
    mixParams.eqLow  = apvts.getRawParameterValue ("eqLow")->load();
    mixParams.eqMid  = apvts.getRawParameterValue ("eqMid")->load();
    mixParams.eqHigh = apvts.getRawParameterValue ("eqHigh")->load();
    mixParams.eqLoFi = apvts.getRawParameterValue ("eqLoFi")->load();
    mixParams.width  = apvts.getRawParameterValue ("mixWidth")->load();
    mixParams.pitchLockOn     = apvts.getRawParameterValue ("pitchLockOn")->load() > 0.5f;
    mixParams.pitchLockMode   = (int) apvts.getRawParameterValue ("pitchLockMode")->load();
    mixParams.pitchLockKey    = (int) apvts.getRawParameterValue ("pitchLockKey")->load();
    mixParams.pitchLockScale  = (int) apvts.getRawParameterValue ("pitchLockScale")->load();
    mixParams.pitchLockAmount = apvts.getRawParameterValue ("pitchLockAmount")->load();
    mixParams.pitchLockFormant = apvts.getRawParameterValue ("pitchLockFormant")->load() > 0.5f;


    // Master bus, minus the final dynamics (deferred until after Sample Mode).
    mixEngine.processParallel (buffer, dryInBuffer, entropyBuffer, prettifierBuffer, mixParams);

    // Sample Mode: capture the clean master (before our own playback, so the loop
    // can't feed back), service a pending Freeze, then layer the looping sample.
    sampleEngine.pushOutput (buffer);
    if (sampleFreezeRequested.exchange (false, std::memory_order_acquire))
        sampleEngine.freeze (apvts.getRawParameterValue ("sampleWindow")->load(),
                             (int) apvts.getRawParameterValue ("sampleSource")->load());
    if (sampleModeOn)
    {
        const float rate  = std::pow (2.0f, noteOffset / 12.0f); // keyboard transposes the loop
        const float level = apvts.getRawParameterValue ("sampleLevel")->load();
        sampleEngine.render (buffer, rate, level);
    }

    // Final master dynamics over the whole program (mix + Sample Mode): glue
    // bus-comp then the ceiling limiter, so nothing escapes the ceiling.
    const float glueAmount = apvts.getRawParameterValue ("mixGlue")->load();
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

    if (channels > 0 && n > 0)
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
    if (channels > 0 && n > 0)
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
