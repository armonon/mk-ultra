#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "GrainFreeze/ModMatrix.h"

// A compact panel of modulation controls + per-knob options for a single
// parameter, shown inside a CallOutBox when the user clicks a knob's "M" button.
// Binds directly to the APVTS mod parameters so edits are automatable and saved.
class ModPanel : public juce::Component
{
public:
    ModPanel (juce::AudioProcessorValueTreeState& state,
              gf::ParamId id,
              const juce::String& paramID,
              const juce::String& knobName);

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Row
    {
        juce::Label  label;
        juce::Slider slider;
        std::unique_ptr<SA> attach;
    };

    void addRow (Row& r, const juce::String& paramID, const juce::String& name);

    juce::AudioProcessorValueTreeState& apvts;
    juce::String mainParamID;
    juce::String title;

    Row lfoRate, lfoDepth, shRate, shDepth, globalAmt, rangeMin, rangeMax, skew;

    juce::Label    lfoShapeLabel;
    juce::ComboBox lfoShape;
    std::unique_ptr<CA> lfoShapeAttach;

    juce::ToggleButton bipolar { "Bipolar" };
    std::unique_ptr<BA> bipolarAttach;

    juce::TextButton resetButton { "Reset knob to default" };

    int dividerY = 200; // y of the OPTIONS divider, computed in resized()

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModPanel)
};
