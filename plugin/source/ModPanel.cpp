#include "GrainFreeze/ModPanel.h"
#include "GrainFreeze/Randomizer.h" // for gf::paramIdString
#include "GrainFreeze/BiohazardLookAndFeel.h"

ModPanel::ModPanel (juce::AudioProcessorValueTreeState& state,
                    gf::ParamId id,
                    const juce::String& paramID,
                    const juce::String& knobName)
    : apvts (state), mainParamID (paramID), title (knobName + " - Modulation")
{
    const auto base = juce::String (gf::paramIdString (id));

    addRow (lfoRate,  base + "_lfoRate",  "LFO Rate");
    addRow (lfoDepth, base + "_lfoDepth", "LFO Depth");

    lfoShapeLabel.setText ("LFO Shape", juce::dontSendNotification);
    lfoShapeLabel.setFont (juce::Font (juce::FontOptions (13.0f)));
    addAndMakeVisible (lfoShapeLabel);
    lfoShape.addItemList ({ "Sine", "Triangle", "Saw", "Square" }, 1);
    addAndMakeVisible (lfoShape);
    lfoShapeAttach = std::make_unique<CA> (apvts, base + "_lfoShape", lfoShape);

    addRow (shRate,    base + "_shRate",    "S&H Rate");
    addRow (shDepth,   base + "_shDepth",   "S&H Depth");
    addRow (globalAmt, base + "_globalAmt", "Global Amt");

    addAndMakeVisible (bipolar);
    bipolarAttach = std::make_unique<BA> (apvts, base + "_bipolar", bipolar);

    addRow (rangeMin, base + "_rangeMin", "Mod Min");
    addRow (rangeMax, base + "_rangeMax", "Mod Max");
    addRow (skew,     base + "_skew",     "Skew");

    addAndMakeVisible (resetButton);
    resetButton.onClick = [this]
    {
        if (auto* p = apvts.getParameter (mainParamID))
            p->setValueNotifyingHost (p->getDefaultValue());
    };

    setSize (282, 396);
}

void ModPanel::addRow (Row& r, const juce::String& paramID, const juce::String& name)
{
    r.label.setText (name, juce::dontSendNotification);
    r.label.setFont (juce::Font (juce::FontOptions (13.0f)));
    addAndMakeVisible (r.label);

    r.slider.setSliderStyle (juce::Slider::LinearHorizontal);
    r.slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 18);
    addAndMakeVisible (r.slider);

    r.attach = std::make_unique<SA> (apvts, paramID, r.slider);
}

void ModPanel::paint (juce::Graphics& g)
{
    using LF = gf::BiohazardLookAndFeel;
    auto bounds = getLocalBounds().toFloat();
    if (auto* lnf = static_cast<LF*> (&getLookAndFeel()))
        lnf->drawPanelRaised (g, bounds.reduced (1.0f), 8.0f);
    else
        g.fillAll (LF::panel);

    g.setColour (LF::textCol);
    g.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
    g.drawText (title, getLocalBounds().removeFromTop (24).reduced (10, 4),
                juce::Justification::centredLeft);

    const auto acc = static_cast<LF*> (&getLookAndFeel()) != nullptr
                   ? static_cast<LF*> (&getLookAndFeel())->accent()
                   : LF::toxic;
    g.setColour (acc.withAlpha (0.18f));
    g.drawHorizontalLine (dividerY, 10.0f, (float) getWidth() - 10.0f);
    g.setColour (LF::textCol.withAlpha (0.65f));
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.drawText ("OPTIONS", 10, dividerY + 4, getWidth() - 20, 14, juce::Justification::centredLeft);
}

void ModPanel::resized()
{
    auto area = getLocalBounds().reduced (10);
    area.removeFromTop (22); // title

    auto layoutRow = [&area] (juce::Label& lab, juce::Component& ctrl)
    {
        auto row = area.removeFromTop (26);
        lab.setBounds  (row.removeFromLeft (78));
        ctrl.setBounds (row);
        area.removeFromTop (3);
    };

    layoutRow (lfoRate.label,  lfoRate.slider);
    layoutRow (lfoDepth.label, lfoDepth.slider);
    layoutRow (lfoShapeLabel,  lfoShape);
    layoutRow (shRate.label,   shRate.slider);
    layoutRow (shDepth.label,  shDepth.slider);
    layoutRow (globalAmt.label, globalAmt.slider);

    // Section divider: record its y so paint() draws the line + "OPTIONS" label
    // exactly here instead of at a hardcoded position that drifts out of sync.
    area.removeFromTop (8);
    dividerY = area.getY();
    area.removeFromTop (18); // room for the OPTIONS caption

    bipolar.setBounds (area.removeFromTop (24));
    area.removeFromTop (4);
    layoutRow (rangeMin.label, rangeMin.slider);
    layoutRow (rangeMax.label, rangeMax.slider);
    layoutRow (skew.label,     skew.slider);
    area.removeFromTop (6);
    resetButton.setBounds (area.removeFromTop (28));
}
