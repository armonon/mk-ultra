#include "GrainFreeze/PluginEditor.h"
#include "BinaryData.h"
#include <cmath>
#include <functional>

namespace
{
    // Pixels of vertical drag to sweep a rotary knob through its full range.
    // JUCE's default is 250; tightening it makes the knobs feel snappier.
    constexpr int kKnobDragSensitivity = 150;

    struct KnobDef { gf::ParamId id; const char* paramID; const char* name; };

    const KnobDef kKnobDefs[] = {
        { gf::ParamId::grainSize,   "grainSize",   "Grain Size" },
        { gf::ParamId::density,     "density",     "Density" },
        { gf::ParamId::pitch,       "pitch",       "Grain Pitch" },
        { gf::ParamId::spray,       "spray",       "Spray" },
        { gf::ParamId::spread,      "spread",      "Spread" },
        { gf::ParamId::position,    "position",    "Position" },
        { gf::ParamId::pitchJitter, "pitchJitter", "Pitch Jitter" },
        { gf::ParamId::reverbMix,   "reverbMix",   "Grain Space" },
        { gf::ParamId::output,      "output",      "Output" },
    };

    const KnobDef kPrettyDefs[] = {
        { gf::ParamId::echoTime,        "echoTimeMs",      "Echo Time" },
        { gf::ParamId::echoFeedback,    "echoFeedback",    "Feedback" },
        { gf::ParamId::echoMix,         "echoMix",         "Echo Mix" },
        { gf::ParamId::prettyReverbMix, "prettyReverbMix", "Reverb" },
        { gf::ParamId::chorusRate,      "chorusRate",      "Chorus Rate" },
        { gf::ParamId::chorusDepth,     "chorusDepth",     "Chorus Depth" },
        { gf::ParamId::beautyAmount,    "beautyAmount",    "Beauty" },
        { gf::ParamId::polishWidth,     "polishWidth",     "Width" },
        { gf::ParamId::bitCrush,        "crushBits",       "Bit Crush" },
    };

    // Standard JUCE rotary arc: ~7 o'clock to ~5 o'clock.
    constexpr float kStartAngle = juce::MathConstants<float>::pi * 1.2f;
    constexpr float kEndAngle   = juce::MathConstants<float>::pi * 2.8f;

    // Shared header geometry so paint() (logo) and resized() (tabs) stay aligned.
    constexpr int kHeaderH  = 56;
    constexpr int kLogoSlot = 60;

    gf::BiohazardLookAndFeel* bioLnF (const juce::Component& c)
    {
        return static_cast<gf::BiohazardLookAndFeel*> (&c.getLookAndFeel());
    }

    // The logo art ships on a solid black background (no alpha), which would draw
    // as a visible box. Rescale to a sane size and derive an alpha mask from
    // luminance so the emblem sits on the UI with no border/box.
    juce::Image prepLogo (const juce::Image& src, int maxDim)
    {
        if (! src.isValid())
            return src;

        int w = src.getWidth();
        int h = src.getHeight();
        const float scale = (float) maxDim / (float) juce::jmax (w, h);
        if (scale < 1.0f)
        {
            w = juce::jmax (1, juce::roundToInt ((float) w * scale));
            h = juce::jmax (1, juce::roundToInt ((float) h * scale));
        }

        auto img = src.rescaled (w, h, juce::Graphics::highResamplingQuality)
                       .convertedToFormat (juce::Image::ARGB);

        juce::Image::BitmapData data (img, juce::Image::BitmapData::readWrite);
        for (int y = 0; y < img.getHeight(); ++y)
            for (int x = 0; x < img.getWidth(); ++x)
            {
                const auto c = data.getPixelColour (x, y);
                float a = juce::jmax (c.getFloatRed(), c.getFloatGreen(), c.getFloatBlue());
                a = juce::jlimit (0.0f, 1.0f, a * 1.25f); // lift mid-glows, keep edges soft
                data.setPixelColour (x, y, c.withAlpha (a));
            }

        return img;
    }

    // Recolour a prepped logo to a flat tint while keeping its alpha shape, so the
    // emblem can render as e.g. solid white on a tab that needs it.
    juce::Image recolourLogo (const juce::Image& src, juce::Colour tint)
    {
        if (! src.isValid())
            return src;

        auto img = src.createCopy();
        juce::Image::BitmapData data (img, juce::Image::BitmapData::readWrite);
        for (int y = 0; y < img.getHeight(); ++y)
            for (int x = 0; x < img.getWidth(); ++x)
                data.setPixelColour (x, y, tint.withAlpha (data.getPixelColour (x, y).getFloatAlpha()));

        return img;
    }
}

void FadeOverlay::paint (juce::Graphics& g)
{
    g.fillAll (gf::BiohazardLookAndFeel::bg.withAlpha (juce::jlimit (0.0f, 1.0f, amount)));
}

PresetBrowser::PresetBrowser (juce::StringArray names, juce::String currentName,
                              std::function<void (juce::String)> onChoose)
    : allNames (std::move (names)), current (std::move (currentName)), choose (std::move (onChoose))
{
    title.setText ("PRESETS", juce::dontSendNotification);
    title.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)).withExtraKerningFactor (0.12f));
    title.setColour (juce::Label::textColourId, gf::BiohazardLookAndFeel::textCol);
    addAndMakeVisible (title);

    search.setTextToShowWhenEmpty ("search presets...", gf::BiohazardLookAndFeel::textCol.withAlpha (0.4f));
    search.setColour (juce::TextEditor::backgroundColourId, gf::BiohazardLookAndFeel::metal);
    search.onTextChange = [this] { applyFilter(); };
    search.onReturnKey  = [this] { commit (0); };
    addAndMakeVisible (search);

    list.setRowHeight (26);
    list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (list);

    applyFilter();
    setSize (320, 420);
}

void PresetBrowser::applyFilter()
{
    const auto q = search.getText().trim().toLowerCase();
    filtered.clear();
    for (const auto& n : allNames)
        if (q.isEmpty() || n.toLowerCase().contains (q))
            filtered.add (n);
    list.updateContent();
    list.repaint();
}

void PresetBrowser::commit (int row)
{
    if (juce::isPositiveAndBelow (row, filtered.size()))
    {
        if (choose) choose (filtered[row]);
        if (auto* cb = findParentComponentOfClass<juce::CallOutBox>())
            cb->dismiss();
    }
}

int PresetBrowser::getNumRows() { return filtered.size(); }

void PresetBrowser::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (! juce::isPositiveAndBelow (row, filtered.size())) return;
    using LF = gf::BiohazardLookAndFeel;
    auto r = juce::Rectangle<int> (0, 0, w, h).reduced (3, 2).toFloat();
    const bool isCurrent = filtered[row] == current;

    if (selected)
    {
        g.setColour (LF::toxic.withAlpha (0.22f));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (LF::toxic.withAlpha (0.5f));
        g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
    }
    else if (isCurrent)
    {
        g.setColour (LF::panel.brighter (0.10f));
        g.fillRoundedRectangle (r, 5.0f);
    }

    g.setColour (isCurrent ? LF::toxic : LF::textCol.withAlpha (0.9f));
    g.setFont (juce::Font (juce::FontOptions (14.0f, isCurrent ? juce::Font::bold : juce::Font::plain)));
    g.drawText (filtered[row], r.withTrimmedLeft (10), juce::Justification::centredLeft, true);
}

void PresetBrowser::listBoxItemClicked (int row, const juce::MouseEvent&) { commit (row); }
void PresetBrowser::returnKeyPressed (int lastRowSelected) { commit (lastRowSelected); }

void PresetBrowser::paint (juce::Graphics& g)
{
    if (auto* lnf = static_cast<gf::BiohazardLookAndFeel*> (&getLookAndFeel()))
        lnf->drawPanelRaised (g, getLocalBounds().toFloat().reduced (1.0f), 8.0f);
    else
        g.fillAll (gf::BiohazardLookAndFeel::panel);
}

void PresetBrowser::resized()
{
    auto area = getLocalBounds().reduced (12);
    title.setBounds (area.removeFromTop (24));
    area.removeFromTop (6);
    search.setBounds (area.removeFromTop (30));
    area.removeFromTop (8);
    list.setBounds (area);
}

void ModRing::paint (juce::Graphics& g)
{
    // Plain: nothing at all when a knob isn't modulated (clean like old Entropy);
    // a simple static arc + dot when it is. No background track, no fading trail.
    if (std::abs (offset) < 0.005f)
        return;

    auto bounds = getLocalBounds().toFloat().reduced (4.0f);
    const float size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    auto square = juce::Rectangle<float> (size, size).withCentre (bounds.getCentre());
    const float radius = size * 0.5f - 2.0f;
    const auto  centre = square.getCentre();
    const float thickness = 3.0f;

    const double base01 = slider.valueToProportionOfLength (slider.getValue());
    const float  baseAngle = kStartAngle + (float) base01 * (kEndAngle - kStartAngle);
    const float  span = (kEndAngle - kStartAngle);
    const auto colour = offset >= 0.0f ? bioLnF (*this)->accent()
                                       : gf::BiohazardLookAndFeel::coral;

    const float modAngle = baseAngle + offset * span * 0.5f;
    juce::Path arc;
    arc.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                       juce::jmin (baseAngle, modAngle), juce::jmax (baseAngle, modAngle), true);
    g.setColour (colour.withAlpha (0.9f));
    g.strokePath (arc, juce::PathStrokeType (thickness, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));

    g.fillEllipse (centre.x + std::cos (modAngle) * radius - 2.5f,
                   centre.y + std::sin (modAngle) * radius - 2.5f, 5.0f, 5.0f);
}

void SaturationCurve::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (2.0f);
    if (auto* lnf = bioLnF (*this))
        lnf->drawPanelInset (g, b, 5.0f);
    else
    {
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.fillRoundedRectangle (b, 4.0f);
    }

    // Axes (centre cross).
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawHorizontalLine ((int) b.getCentreY(), b.getX(), b.getRight());
    g.drawVerticalLine   ((int) b.getCentreX(), b.getY(), b.getBottom());

    const int   type  = (int) apvts.getRawParameterValue ("satType")->load();
    const float drive = apvts.getRawParameterValue ("satDrive")->load();

    // Plot y = transfer(x) for x in [-1,1]; map to the box (y up).
    juce::Path curve;
    const int N = 96;
    for (int i = 0; i <= N; ++i)
    {
        const float x = -1.0f + 2.0f * (float) i / (float) N;
        float y = gf::Saturator::transfer (type, drive, x);
        y = juce::jlimit (-1.2f, 1.2f, y) / 1.2f; // headroom so it doesn't clip the box edge

        const float px = b.getX() + (x * 0.5f + 0.5f) * b.getWidth();
        const float py = b.getCentreY() - y * (b.getHeight() * 0.5f);
        if (i == 0) curve.startNewSubPath (px, py);
        else        curve.lineTo (px, py);
    }
    const auto acc = bioLnF (*this) ? bioLnF (*this)->accent() : gf::BiohazardLookAndFeel::toxic;
    g.setColour (acc.withAlpha (0.95f));
    g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved));
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    if (auto* lnf = bioLnF (*this))
        lnf->drawPanelInset (g, b, 4.0f);

    const float gap = 5.0f;
    const float barW = (b.getWidth() - gap - 8.0f) * 0.5f;
    const float inset = 4.0f;

    for (int ch = 0; ch < 2; ++ch)
    {
        auto bar = juce::Rectangle<float> (b.getX() + inset + ch * (barW + gap),
                                           b.getY() + inset,
                                           barW, b.getHeight() - inset * 2.0f);
        if (auto* lnf = bioLnF (*this))
            lnf->drawPanelInset (g, bar, 3.0f);
        else
        {
            g.setColour (juce::Colours::black.withAlpha (0.3f));
            g.fillRoundedRectangle (bar, 2.0f);
        }

        const float lin = juce::jlimit (0.0f, 1.0f, proc.getOutputLevel (ch));
        // Map linear magnitude to a roughly -48..0 dB visual scale.
        const float db   = juce::Decibels::gainToDecibels (lin, -48.0f);
        const float norm = juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 48.0f);
        const float h    = bar.getHeight() * norm;
        auto fill = bar.withTop (bar.getBottom() - h).reduced (2.0f, 1.0f);

        // Green below ~-6 dB, amber approaching 0, red at/over 0.
        juce::Colour c = norm > 0.95f ? juce::Colour (0xffe24b4a)
                       : norm > 0.80f ? juce::Colour (0xffef9f27)
                                      : juce::Colour (0xff5dcaa5);
        juce::ColourGradient barGrad (c.brighter (0.25f), fill.getX(), fill.getY(),
                                       c.darker (0.15f), fill.getRight(), fill.getBottom(), false);
        g.setGradientFill (barGrad);
        g.fillRoundedRectangle (fill, 2.0f);
        g.setColour (c.brighter (0.35f).withAlpha (0.5f));
        g.drawRoundedRectangle (fill, 2.0f, 1.0f);
    }
}

void ModScope::paint (juce::Graphics& g)
{
    using LF = gf::BiohazardLookAndFeel;
    auto b = getLocalBounds().toFloat().reduced (2.0f);
    if (auto* lnf = bioLnF (*this))
        lnf->drawPanelInset (g, b, 5.0f);
    else
    {
        g.setColour (juce::Colours::black.withAlpha (0.3f));
        g.fillRoundedRectangle (b, 4.0f);
    }

    const auto acc = bioLnF (*this) ? bioLnF (*this)->accent() : LF::toxic;
    const auto accDim = bioLnF (*this) ? bioLnF (*this)->accentDim() : LF::toxicDim;

    // Centre line.
    g.setColour (accDim.withAlpha (0.55f));
    g.drawHorizontalLine ((int) b.getCentreY(), b.getX() + 4.0f, b.getRight() - 4.0f);

    std::array<float, kPoints> history {};
    proc.getModScopeSnapshot (history); // oldest -> newest, left -> right
    juce::Path p;
    for (int i = 0; i < kPoints; ++i)
    {
        const float v = juce::jlimit (-1.0f, 1.0f, history[(size_t) i]);
        const float px = b.getX() + (float) i / (float) (kPoints - 1) * b.getWidth();
        const float py = b.getCentreY() - v * (b.getHeight() * 0.45f);
        if (i == 0) p.startNewSubPath (px, py);
        else        p.lineTo (px, py);
    }
    g.setColour (acc.withAlpha (0.35f));
    g.strokePath (p, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (acc);
    g.strokePath (p, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void WaveformMemoryDisplay::paint (juce::Graphics& g)
{
    using LF = gf::BiohazardLookAndFeel;
    auto b = getLocalBounds().toFloat().reduced (2.0f);
    if (auto* lnf = bioLnF (*this))
        lnf->drawPanelInset (g, b, 5.0f);
    else
    {
        g.setColour (juce::Colours::black.withAlpha (0.28f));
        g.fillRoundedRectangle (b, 4.0f);
    }

    const auto acc = bioLnF (*this) ? bioLnF (*this)->accent() : LF::toxic;
    const auto accDim = bioLnF (*this) ? bioLnF (*this)->accentDim() : LF::toxicDim;

    g.setColour (accDim.withAlpha (0.45f));
    g.drawHorizontalLine ((int) b.getCentreY(), b.getX() + 4.0f, b.getRight() - 4.0f);

    std::array<float, 256> samples {};
    proc.getWaveformSnapshot (samples);
    juce::Path p;
    for (int i = 0; i < (int) samples.size(); ++i)
    {
        const float x = b.getX() + ((float) i / (float) (samples.size() - 1)) * b.getWidth();
        const float y = b.getCentreY() - juce::jlimit (-1.0f, 1.0f, samples[(size_t) i]) * b.getHeight() * 0.45f;
        if (i == 0) p.startNewSubPath (x, y);
        else p.lineTo (x, y);
    }
    g.setColour (acc.withAlpha (0.3f));
    g.strokePath (p, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (acc.withAlpha (0.95f));
    g.strokePath (p, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void SpectrumDisplay::paint (juce::Graphics& g)
{
    using LF = gf::BiohazardLookAndFeel;
    auto b = getLocalBounds().toFloat().reduced (2.0f);
    if (auto* lnf = bioLnF (*this))
        lnf->drawPanelInset (g, b, 5.0f);
    else
    {
        g.setColour (juce::Colours::black.withAlpha (0.28f));
        g.fillRoundedRectangle (b, 4.0f);
    }

    const auto acc = bioLnF (*this) ? bioLnF (*this)->accent() : LF::toxic;

    std::array<float, GrainFreezeProcessor::kSpectrumBins> bins {};
    proc.getSpectrumSnapshot (bins);

    auto inner = b.reduced (5.0f, 4.0f);
    const int n = (int) bins.size();
    const float bw = inner.getWidth() / (float) n;

    // Filled spectrum: brighter near the top of each bar, fading to the floor.
    juce::Path fill;
    fill.startNewSubPath (inner.getX(), inner.getBottom());
    for (int i = 0; i < n; ++i)
    {
        const float v = juce::jlimit (0.0f, 1.0f, bins[(size_t) i]);
        const float x = inner.getX() + (float) i * bw;
        const float y = inner.getBottom() - v * inner.getHeight();
        fill.lineTo (x, y);
    }
    fill.lineTo (inner.getRight(), inner.getBottom());
    fill.closeSubPath();

    g.setColour (acc.withAlpha (0.22f));
    g.fillPath (fill);
    g.setColour (acc.withAlpha (0.9f));
    g.strokePath (fill, juce::PathStrokeType (1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

GrainFreezeEditor::GrainFreezeEditor (GrainFreezeProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    // paint() fills the entire bounds with an opaque gradient every frame, so the
    // editor IS opaque. Declaring it lets the OpenGL renderer clear/refill the whole
    // framebuffer instead of leaving stale construction-time pixels in the corner.
    setOpaque (true);
    setLookAndFeel (&lnf);

    logoImage = prepLogo (juce::ImageCache::getFromMemory (BinaryData::logo_png, BinaryData::logo_pngSize), 600);
    whiteLogoImage = recolourLogo (logoImage, juce::Colours::white);
    blackLogoImage = recolourLogo (logoImage, juce::Colours::black);
    greenLogoImage = recolourLogo (logoImage, gf::BiohazardLookAndFeel::toxic);
    bgImage   = juce::ImageCache::getFromMemory (BinaryData::background_png, BinaryData::background_pngSize);
    mixBgImage = juce::ImageCache::getFromMemory (BinaryData::mix_background_png, BinaryData::mix_background_pngSize);
    prettifierBgImage = juce::ImageCache::getFromMemory (BinaryData::prettifier_png, BinaryData::prettifier_pngSize);
    prettifierLogoImage = prepLogo (juce::ImageCache::getFromMemory (BinaryData::prettifier_logo_png, BinaryData::prettifier_logo_pngSize), 600);

    addAndMakeVisible (freezeButton);
    freezeAttachment = std::make_unique<ButtonAttachment> (proc.apvts, "frozen", freezeButton);

    for (auto* tab : { &tabHome, &tabEntropy, &tabMachines, &tabMix, &tabPrettifier })
        addAndMakeVisible (*tab);
    tabHome.onClick = [this] { switchTab (4); };
    tabEntropy.onClick = [this] { switchTab (0); };
    tabMix.onClick = [this] { switchTab (1); };
    tabPrettifier.onClick = [this] { switchTab (2); };
    tabMachines.onClick = [this] { switchTab (3); };
    tabHome.setClickingTogglesState (true);
    tabEntropy.setClickingTogglesState (true);
    tabMix.setClickingTogglesState (true);
    tabPrettifier.setClickingTogglesState (true);
    tabMachines.setClickingTogglesState (true);

    for (auto* b : { &buttonA, &buttonB, &copyAToBButton, &copyBToAButton, &resetBButton,
                     &undoButton, &redoButton, &initButton, &randomizeAllButton, &panicButton })
        addAndMakeVisible (*b);
    // Plain click recalls a snapshot; shift-click stores the current sound into
    // it. Capture two sounds this way, then use the Morph knob to crossfade them.
    buttonA.onClick = [this] { if (juce::ModifierKeys::getCurrentModifiers().isShiftDown()) proc.storeSlotA(); else proc.loadSlotA(); };
    buttonB.onClick = [this] { if (juce::ModifierKeys::getCurrentModifiers().isShiftDown()) proc.storeSlotB(); else proc.loadSlotB(); };
    copyAToBButton.onClick = [this] { proc.copyAToB(); };
    copyBToAButton.onClick = [this] { proc.copyBToA(); };
    resetBButton.onClick = [this] { proc.resetSlotB(); };
    panicButton.onClick = [this] { proc.panicKillTail(); };

    buttonA.setTooltip ("Click: recall A   |   Shift-click: store current sound as A");
    buttonB.setTooltip ("Click: recall B   |   Shift-click: store current sound as B");
    copyAToBButton.setTooltip ("Copy A's settings into B");
    copyBToAButton.setTooltip ("Copy B's settings into A");
    resetBButton.setTooltip ("Reset the B snapshot to defaults");
    panicButton.setTooltip ("Silence the engine and kill any reverb/delay tail");

    // Extra tools: undo / redo (APVTS UndoManager) and init-to-defaults.
    undoButton.setTooltip ("Undo last parameter change");
    redoButton.setTooltip ("Redo");
    initButton.setTooltip ("Reset all parameters to their defaults");
    randomizeAllButton.setTooltip ("Randomize every Texture / Grain + Beauty & Space parameter at once");
    randomizeAllButton.onClick = [this]
    {
        const int modeIdx  = (int) proc.apvts.getRawParameterValue ("randomMode")->load();
        const float amount = proc.apvts.getRawParameterValue ("mutationAmount")->load();
        const auto mode    = (gf::Randomizer::Mode) juce::jlimit (0, (int) gf::Randomizer::Mode::identityLoss, modeIdx);
        proc.undoManager.beginNewTransaction ("Randomize All");
        // Whole ParamId span = Texture / Grain (grainSize..output) + Beauty & Space (echoTime..bitCrush).
        proc.randomizer.randomize (mode, amount, (int) gf::ParamId::grainSize, (int) gf::ParamId::bitCrush);
    };
    undoButton.onClick = [this] { proc.undoManager.undo(); };
    redoButton.onClick = [this] { proc.undoManager.redo(); };
    initButton.onClick = [this]
    {
        proc.undoManager.beginNewTransaction ("Init");
        proc.preserveLocked ([this]
        {
            for (auto* p : proc.getParameters())
                if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
                    rp->setValueNotifyingHost (rp->getDefaultValue());
        });
    };

    for (int i = 0; i < kNumKnobs; ++i)
        addKnob (knobs[(size_t) i], kKnobDefs[i].id, kKnobDefs[i].paramID, kKnobDefs[i].name);

    for (int i = 0; i < kNumPrettyKnobs; ++i)
        addKnob (prettyKnobs[(size_t) i], kPrettyDefs[i].id, kPrettyDefs[i].paramID, kPrettyDefs[i].name);

    addAndMakeVisible (randomizeButton);
    randomizeButton.setTooltip ("Randomize this tab's controls (Transform / Out / Beauty & Space independently)");
    randomizeButton.onClick = [this]
    {
        const int modeIdx = (int) proc.apvts.getRawParameterValue ("randomMode")->load();
        const float amount = proc.apvts.getRawParameterValue ("mutationAmount")->load();
        const auto mode = (gf::Randomizer::Mode) juce::jlimit (0, (int) gf::Randomizer::Mode::identityLoss, modeIdx);
        proc.undoManager.beginNewTransaction ("Randomize");

        // Each tab rolls only its own controls, so the dice are independent.
        if (currentTab == 1)        // Mix console
            proc.randomizer.randomizeMix (amount);
        else if (currentTab == 2)   // Beauty & Space knobs
            proc.randomizer.randomize (mode, amount, (int) gf::ParamId::echoTime, (int) gf::ParamId::bitCrush);
        else                        // Texture / Grain knobs
            proc.randomizer.randomize (mode, amount, (int) gf::ParamId::grainSize, (int) gf::ParamId::output);
    };

    addAndMakeVisible (saveButton);
    saveButton.onClick = [this]
    {
        auto name = presetName.getText().trim();
        if (name.isEmpty()) name = "Preset " + juce::String (proc.presets.getPresetNames().size() + 1);
        if (proc.presets.savePreset (name))
            refreshPresetList();
    };

    addAndMakeVisible (prevButton);
    prevButton.onClick = [this] { proc.preserveLocked ([this] { proc.presets.loadPrevious(); }); refreshPresetList(); };
    addAndMakeVisible (nextButton);
    nextButton.onClick = [this] { proc.preserveLocked ([this] { proc.presets.loadNext(); });     refreshPresetList(); };

    addAndMakeVisible (browseButton);
    browseButton.onClick = [this]
    {
        auto browser = std::make_unique<PresetBrowser> (
            proc.presets.getPresetNames(),
            proc.presets.getCurrentPresetName(),
            [this] (juce::String name)
            {
                proc.preserveLocked ([this, name] { proc.presets.loadPreset (name); });
                refreshPresetList();
            });
        browser->setLookAndFeel (&lnf);
        juce::CallOutBox::launchAsynchronously (std::move (browser),
                                                browseButton.getScreenBounds(), nullptr);
    };

    addAndMakeVisible (presetBox);
    presetBox.setTextWhenNothingSelected ("Presets");
    presetBox.onChange = [this]
    {
        auto name = presetBox.getText();
        if (name.isNotEmpty()) proc.preserveLocked ([this, name] { proc.presets.loadPreset (name); });
    };

    addAndMakeVisible (presetName);
    presetName.setTextToShowWhenEmpty ("preset name", juce::Colours::grey);

    addAndMakeVisible (pluginOnButton);
    addAndMakeVisible (entropyOnButton);
    addAndMakeVisible (prettifierOnButton);
    addAndMakeVisible (limiterOnButton);
    addAndMakeVisible (mixEqOnButton);
    addAndMakeVisible (pitchMatchOnButton);
    addAndMakeVisible (tempoLockOnButton);
    pluginOnAttach = std::make_unique<ButtonAttachment> (proc.apvts, "pluginOn", pluginOnButton);
    entropyOnAttach = std::make_unique<ButtonAttachment> (proc.apvts, "entropyOn", entropyOnButton);
    prettifierOnAttach = std::make_unique<ButtonAttachment> (proc.apvts, "prettifierOn", prettifierOnButton);
    limiterOnAttach = std::make_unique<ButtonAttachment> (proc.apvts, "limiterOn", limiterOnButton);
    mixEqOnAttach = std::make_unique<ButtonAttachment> (proc.apvts, "mixEqOn", mixEqOnButton);
    pitchMatchOnAttach = std::make_unique<ButtonAttachment> (proc.apvts, "pitchMatchOn", pitchMatchOnButton);
    tempoLockOnAttach = std::make_unique<ButtonAttachment> (proc.apvts, "tempoLockOn", tempoLockOnButton);

    // Global mod source.
    addAndMakeVisible (globalModOnButton);
    globalModOnButton.setTooltip ("Enable the global modulation source");
    globalModOnAttach = std::make_unique<ButtonAttachment> (proc.apvts, "globalModOn", globalModOnButton);
    globalRate.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    globalRate.setMouseDragSensitivity (kKnobDragSensitivity);
    globalRate.setTextBoxStyle (juce::Slider::NoTextBox, false, 54, 16);
    addAndMakeVisible (globalRate);
    globalRateAttach = std::make_unique<SliderAttachment> (proc.apvts, "globalRate", globalRate);
    globalShape.addItemList ({ "Sine", "Triangle", "Saw", "Square" }, 1);
    addAndMakeVisible (globalShape);
    globalShapeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        proc.apvts, "globalShape", globalShape);

    // Tempo-sync selectors for the LFO rate and the Echo time ("Free" = the knob).
    {
        const juce::StringArray divs { "Free", "1/1", "1/2", "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" };
        lfoSyncBox.addItemList (divs, 1);  addAndMakeVisible (lfoSyncBox);
        echoSyncBox.addItemList (divs, 1); addAndMakeVisible (echoSyncBox);
        lfoSyncAttach  = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "lfoDivision", lfoSyncBox);
        echoSyncAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "echoDivision", echoSyncBox);
    }

    // Saturation.
    addAndMakeVisible (satOnButton);
    satOnButton.setTooltip ("Enable the saturation stage");
    satOnAttach = std::make_unique<ButtonAttachment> (proc.apvts, "satOn", satOnButton);
    satType.addItemList ({ "Tube", "Tape", "Hard" }, 1);
    addAndMakeVisible (satType);
    satTypeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        proc.apvts, "satType", satType);
    satDrive.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    satDrive.setMouseDragSensitivity (kKnobDragSensitivity);
    satDrive.setTextBoxStyle (juce::Slider::NoTextBox, false, 54, 16);
    addAndMakeVisible (satDrive);
    satDriveAttach = std::make_unique<SliderAttachment> (proc.apvts, "satDrive", satDrive);
    // The timer no longer repaints the curve continuously, so refresh it on change.
    satDrive.onValueChange = [this] { if (satCurve != nullptr) satCurve->repaint(); };
    satType.onChange       = [this] { if (satCurve != nullptr) satCurve->repaint(); };
    satMix.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    satMix.setMouseDragSensitivity (kKnobDragSensitivity);
    satMix.setTextBoxStyle (juce::Slider::NoTextBox, false, 54, 16);
    addAndMakeVisible (satMix);
    satMixAttach = std::make_unique<SliderAttachment> (proc.apvts, "satMix", satMix);
    // Per-control labels so the saturation stage reads clearly (Type / Drive / Mix).
    for (auto* l : { &satTypeLabel, &satDriveLabel, &satMixLabel })
    {
        l->setJustificationType (juce::Justification::centred);
        l->setFont (juce::Font (juce::FontOptions (11.0f)));
        addAndMakeVisible (*l);
    }
    satTypeLabel.setText ("Type", juce::dontSendNotification);
    satDriveLabel.setText ("Drive", juce::dontSendNotification);
    satMixLabel.setText ("Mix", juce::dontSendNotification);

    satCurve = std::make_unique<SaturationCurve> (proc.apvts);
    addAndMakeVisible (*satCurve);
    meter = std::make_unique<LevelMeter> (proc);
    addAndMakeVisible (*meter);
    modScope = std::make_unique<ModScope> (proc);
    addAndMakeVisible (*modScope);
    waveformDisplay = std::make_unique<WaveformMemoryDisplay> (proc);
    addAndMakeVisible (*waveformDisplay);
    spectrumDisplay = std::make_unique<SpectrumDisplay> (proc);
    addChildComponent (*spectrumDisplay); // shown only on the Mix tab

    addAndMakeVisible (specFreezeButton);
    specFreezeAttach = std::make_unique<ButtonAttachment> (proc.apvts, "specFreeze", specFreezeButton);
    specLabel.setText ("Spectral", juce::dontSendNotification);
    specLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    addAndMakeVisible (specLabel);
    specMix.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    specMix.setMouseDragSensitivity (kKnobDragSensitivity);
    specMix.setTextBoxStyle (juce::Slider::NoTextBox, false, 54, 16);
    addAndMakeVisible (specMix);
    specMixAttach = std::make_unique<SliderAttachment> (proc.apvts, "specMix", specMix);
    specShimmer.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    specShimmer.setMouseDragSensitivity (kKnobDragSensitivity);
    specShimmer.setTextBoxStyle (juce::Slider::NoTextBox, false, 54, 16);
    addAndMakeVisible (specShimmer);
    specShimmerAttach = std::make_unique<SliderAttachment> (proc.apvts, "specShimmer", specShimmer);

    auto setupMixKnob = [this] (juce::Slider& s)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 64, 18);
        s.setMouseDragSensitivity (kKnobDragSensitivity); // shorter sweep = snappier feel
        s.setPopupDisplayEnabled (true, false, this); // drag-only: hover popups get stuck as orphaned bubbles
    };
    auto setupSectionLabel = [this] (juce::Label& l, const juce::String& text, float size = 13.0f)
    {
        l.setText (text, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centredLeft);
        l.setFont (juce::Font (juce::FontOptions (size, juce::Font::bold)));
        addAndMakeVisible (l);
    };
    auto setupDialLabel = [this] (juce::Label& l, const juce::String& text)
    {
        l.setText (text, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centred);
        l.setFont (juce::Font (juce::FontOptions (12.0f)));
        addAndMakeVisible (l);
    };
    for (auto* s : { &dryLevel, &entropySend, &entropyReturn, &prettifierSend, &prettifierReturn, &mixOutput, &chaosBeauty, &mixWidth, &mixGlue, &mixCeiling })
    {
        setupMixKnob (*s);
        addAndMakeVisible (*s);
    }
    mixCeiling.textFromValueFunction = [] (double v) { return juce::String (v, 1) + " dB"; };
    mixCeiling.updateText();
    routingMode.addItemList ({ "Parallel", "Texture->Beauty", "Beauty->Texture", "Multiband" }, 1);
    addAndMakeVisible (routingMode);

    dryLevelAttach = std::make_unique<SliderAttachment> (proc.apvts, "dryLevel", dryLevel);
    entropySendAttach = std::make_unique<SliderAttachment> (proc.apvts, "entropySend", entropySend);
    entropyReturnAttach = std::make_unique<SliderAttachment> (proc.apvts, "entropyReturn", entropyReturn);
    prettifierSendAttach = std::make_unique<SliderAttachment> (proc.apvts, "prettifierSend", prettifierSend);
    prettifierReturnAttach = std::make_unique<SliderAttachment> (proc.apvts, "prettifierReturn", prettifierReturn);
    mixOutputAttach = std::make_unique<SliderAttachment> (proc.apvts, "mixOutput", mixOutput);
    chaosBeautyAttach = std::make_unique<SliderAttachment> (proc.apvts, "chaosBeauty", chaosBeauty);
    mixWidthAttach = std::make_unique<SliderAttachment> (proc.apvts, "mixWidth", mixWidth);
    mixGlueAttach = std::make_unique<SliderAttachment> (proc.apvts, "mixGlue", mixGlue);
    mixCeilingAttach = std::make_unique<SliderAttachment> (proc.apvts, "ceilingDb", mixCeiling);
    routingModeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "routingMode", routingMode);

    setupSectionLabel (prettifierHeader, "BEAUTY & SPACE", 14.0f);

    setupSectionLabel (mixHeader, "MASTER", 14.0f);
    routingLabel.setText ("Routing", juce::dontSendNotification);
    routingLabel.setJustificationType (juce::Justification::centredLeft);
    routingLabel.setFont (juce::Font (juce::FontOptions (12.0f)));
    addAndMakeVisible (routingLabel);
    const char* mixNames[] = { "Dry", "Texture Send", "Texture Return", "Beauty Send",
                               "Beauty Return", "Output", "Chaos <-> Beauty", "Width", "Glue", "Ceiling" };
    for (size_t i = 0; i < mixLabels.size(); ++i)
        setupDialLabel (mixLabels[i], mixNames[i]);

    // Mix EQ section: 4 plain knobs gated by the EQ toggle.
    setupSectionLabel (eqHeader, "EQ", 13.0f);
    const char* eqNames[] = { "Low", "Mid", "High", "Lo-Fi" };
    for (size_t i = 0; i < eqLabels.size(); ++i)
        setupDialLabel (eqLabels[i], eqNames[i]);
    for (auto* s : { &eqLowKnob, &eqMidKnob, &eqHighKnob, &eqLoFiKnob })
    {
        setupMixKnob (*s);
        addAndMakeVisible (*s);
    }
    eqLowAttach  = std::make_unique<SliderAttachment> (proc.apvts, "eqLow",  eqLowKnob);
    eqMidAttach  = std::make_unique<SliderAttachment> (proc.apvts, "eqMid",  eqMidKnob);
    eqHighAttach = std::make_unique<SliderAttachment> (proc.apvts, "eqHigh", eqHighKnob);
    eqLoFiAttach = std::make_unique<SliderAttachment> (proc.apvts, "eqLoFi", eqLoFiKnob);

    // Sample Mode (Mix tab): freeze a moment of audio and loop/play it.
    setupSectionLabel (sampleHeader, "SAMPLE MODE", 13.0f);
    addAndMakeVisible (sampleModeButton);
    sampleModeButton.setTooltip ("Play the frozen sample: it loops and the keyboard transposes it");
    sampleModeAttach = std::make_unique<ButtonAttachment> (proc.apvts, "sampleMode", sampleModeButton);

    addAndMakeVisible (sampleFreezeButton);
    sampleFreezeButton.setTooltip ("Capture the most recent window of audio into the sample");
    sampleFreezeButton.onClick = [this] { proc.triggerSampleFreeze(); };

    sampleSourceBox.addItemList ({ "Input", "Output" }, 1);
    addAndMakeVisible (sampleSourceBox);
    sampleSourceAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        proc.apvts, "sampleSource", sampleSourceBox);
    setupDialLabel (sampleSourceLabel, "Source");

    setupMixKnob (sampleWindowSlider);
    sampleWindowSlider.textFromValueFunction = [] (double v) { return juce::String (v, 1) + " s"; };
    sampleWindowSlider.updateText();
    addAndMakeVisible (sampleWindowSlider);
    sampleWindowAttach = std::make_unique<SliderAttachment> (proc.apvts, "sampleWindow", sampleWindowSlider);
    setupDialLabel (sampleWindowLabel, "Window");

    setupMixKnob (sampleLevelSlider);
    addAndMakeVisible (sampleLevelSlider);
    sampleLevelAttach = std::make_unique<SliderAttachment> (proc.apvts, "sampleLevel", sampleLevelSlider);
    setupDialLabel (sampleLevelLabel, "Level");

    // Master Pitch Lock (Mix tab): tune the whole output to a key/scale.
    setupSectionLabel (pitchLockHeader, "PITCH LOCK", 13.0f);
    addAndMakeVisible (pitchLockButton);
    pitchLockButton.setTooltip ("Force the whole output into the chosen key/scale (overrides Pitch Match)");
    pitchLockAttach = std::make_unique<ButtonAttachment> (proc.apvts, "pitchLockOn", pitchLockButton);

    addAndMakeVisible (pitchLockFormantButton);
    pitchLockFormantButton.setTooltip ("Preserve formants while tuning (phase vocoder, adds ~17 ms latency)");
    pitchLockFormantAttach = std::make_unique<ButtonAttachment> (proc.apvts, "pitchLockFormant", pitchLockFormantButton);

    pitchLockModeBox.addItemList ({ "Chromatic", "Scale", "Root" }, 1);
    addAndMakeVisible (pitchLockModeBox);
    pitchLockModeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        proc.apvts, "pitchLockMode", pitchLockModeBox);
    setupDialLabel (pitchLockModeLabel, "Mode");

    pitchLockKeyBox.addItemList ({ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 1);
    addAndMakeVisible (pitchLockKeyBox);
    pitchLockKeyAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        proc.apvts, "pitchLockKey", pitchLockKeyBox);
    setupDialLabel (pitchLockKeyLabel, "Key");

    pitchLockScaleBox.addItemList ({ "Major", "Minor", "Harmonic Minor", "Melodic Minor", "Dorian", "Phrygian", "Lydian",
                                     "Mixolydian", "Locrian", "Major Pentatonic", "Minor Pentatonic", "Chromatic" }, 1);
    addAndMakeVisible (pitchLockScaleBox);
    pitchLockScaleAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        proc.apvts, "pitchLockScale", pitchLockScaleBox);
    setupDialLabel (pitchLockScaleLabel, "Scale");

    setupMixKnob (pitchLockAmountSlider);
    addAndMakeVisible (pitchLockAmountSlider);
    pitchLockAmountAttach = std::make_unique<SliderAttachment> (proc.apvts, "pitchLockAmount", pitchLockAmountSlider);
    setupDialLabel (pitchLockAmountLabel, "Amount");

    // Prettifier output gain knob (plain rotary, like the mix knobs).
    setupDialLabel (prettyOutputLabel, "Output");
    setupMixKnob (prettyOutputKnob);
    addAndMakeVisible (prettyOutputKnob);
    prettyOutputAttach = std::make_unique<SliderAttachment> (proc.apvts, "prettifierOutTrim", prettyOutputKnob);

    // Prettifier "DNA" character bank + per-module on/off toggles.
    setupSectionLabel (dnaHeader, "DNA", 13.0f);
    {
        static const char* dnaIds[kNumDna] = { "dnaCharacter", "dnaAge", "dnaWarmth", "dnaWidth", "dnaRandomness",
                                               "dnaAnalog", "dnaDigital", "dnaSmoothness", "dnaMotion", "dnaShine" };
        static const char* dnaNames[kNumDna] = { "Character", "Age", "Warmth", "Width", "Random",
                                                 "Analog", "Digital", "Smooth", "Motion", "Shine" };
        for (int i = 0; i < kNumDna; ++i)
        {
            setupMixKnob (dnaKnobs[(size_t) i]);
            addAndMakeVisible (dnaKnobs[(size_t) i]);
            dnaAttach[(size_t) i] = std::make_unique<SliderAttachment> (proc.apvts, dnaIds[i], dnaKnobs[(size_t) i]);
            setupDialLabel (dnaLabels[(size_t) i], dnaNames[i]);
        }
    }
    {
        juce::ToggleButton* mods[6] = { &echoOnButton, &reverbOnButton, &chorusOnButton, &crushOnButton,
                                        &phaserOnButton, &flangerOnButton };
        const char* modIds[6] = { "echoOn", "prettyReverbOn", "chorusOn", "crushOn", "phaserOn", "flangerOn" };
        for (int i = 0; i < 6; ++i)
        {
            mods[i]->setTooltip ("Enable/disable this Beauty & Space module");
            addAndMakeVisible (*mods[i]);
            moduleAttach[(size_t) i] = std::make_unique<ButtonAttachment> (proc.apvts, modIds[i], *mods[i]);
        }
    }

    for (auto* s : { &globalRate, &satDrive, &satMix, &specMix, &specShimmer })
        s->setPopupDisplayEnabled (true, false, this); // drag-only: hover popups get stuck as orphaned bubbles

    // MIDI play controls — make the shared piano roll actually drive the grains.
    addAndMakeVisible (midiEnableButton);
    midiEnableButton.setTooltip ("Let the keyboard / incoming MIDI transpose the grains");
    midiEnableAttach = std::make_unique<ButtonAttachment> (proc.apvts, "midiEnable", midiEnableButton);

    setupMixKnob (midiRootSlider);
    // Show the root as a note name (e.g. C3) in the drag popup.
    midiRootSlider.textFromValueFunction = [] (double v)
        { return juce::MidiMessage::getMidiNoteName ((int) std::round (v), true, true, 3); };
    midiRootSlider.updateText();
    addAndMakeVisible (midiRootSlider);
    midiRootAttach = std::make_unique<SliderAttachment> (proc.apvts, "midiRoot", midiRootSlider);
    setupDialLabel (midiRootLabel, "Root");

    setupMixKnob (midiGlideSlider);
    addAndMakeVisible (midiGlideSlider);
    midiGlideAttach = std::make_unique<SliderAttachment> (proc.apvts, "glideTime", midiGlideSlider);
    setupDialLabel (midiGlideLabel, "Glide");

    setupMixKnob (midiVelAmpSlider);
    addAndMakeVisible (midiVelAmpSlider);
    midiVelAmpAttach = std::make_unique<SliderAttachment> (proc.apvts, "velToAmp", midiVelAmpSlider);
    setupDialLabel (midiVelAmpLabel, juce::String (juce::CharPointer_UTF8 ("Vel\xe2\x86\x92" "Amp")));

    keyboard = std::make_unique<juce::MidiKeyboardComponent> (proc.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard);
    addAndMakeVisible (*keyboard);

    addChildComponent (fadeOverlay); // on top, hidden until a tab switch

    // ---- MACHINES tab controls (Spectral, Pitch/Formant, Damage, Time Breaker) ----
    setupSectionLabel (machinesHeader, "MACHINES", 14.0f);
    setupSectionLabel (machSpectralTitle, "SPECTRAL", 13.0f);
    setupSectionLabel (machPitchTitle,    "PITCH / FORMANT", 13.0f);
    setupSectionLabel (machDamageTitle,   "DAMAGE", 13.0f);
    setupSectionLabel (machTimeTitle,     "TIME BREAKER", 13.0f);

    auto machKnob = [&] (juce::Slider& s, juce::Label& l, const juce::String& name,
                         const char* pid, std::unique_ptr<SliderAttachment>& att)
    {
        setupMixKnob (s);
        addAndMakeVisible (s);
        setupDialLabel (l, name);
        att = std::make_unique<SliderAttachment> (proc.apvts, pid, s);
    };
    auto machToggle = [&] (juce::ToggleButton& b, const char* pid, std::unique_ptr<ButtonAttachment>& att)
    {
        addAndMakeVisible (b);
        att = std::make_unique<ButtonAttachment> (proc.apvts, pid, b);
    };

    machToggle (machSpectralOn, "spectralOn", machSpectralOnAttach);
    machKnob (machSpectralMix,    machSpectralMixL,    "Mix",    "spectralMix",    machSpectralMixAttach);
    machKnob (machSpectralAmount, machSpectralAmountL, "Amount", "spectralAmount", machSpectralAmountAttach);

    machToggle (machPitchOn,      "pitchFormantOn",   machPitchOnAttach);
    machToggle (machPitchFormant, "pitchLockFormant", machPitchFormantAttach);
    machKnob (machPitchMix,   machPitchMixL,   "Mix",   "pitchFormantMix", machPitchMixAttach);
    machKnob (machPitchShift, machPitchShiftL, "Pitch", "pitch",           machPitchShiftAttach);

    machToggle (machDamageOn, "damageOn", machDamageOnAttach);
    machDamageClip.addItemList ({ "Tube", "Tape", "Hard", "Fold", "Diode" }, 1);
    addAndMakeVisible (machDamageClip);
    machDamageClipAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "damageClip", machDamageClip);
    machKnob (machDamageAmount,  machDamageAmountL,  "Drive",   "damageAmount",  machDamageAmountAttach);
    machKnob (machDamageBits,    machDamageBitsL,    "Bits",    "damageBits",    machDamageBitsAttach);
    machKnob (machDamageRate,    machDamageRateL,    "Rate",    "damageRate",    machDamageRateAttach);
    machKnob (machDamageJitter,  machDamageJitterL,  "Jitter",  "damageJitter",  machDamageJitterAttach);
    machKnob (machDamageNoise,   machDamageNoiseL,   "Noise",   "damageNoise",   machDamageNoiseAttach);
    machKnob (machDamageDropout, machDamageDropoutL, "Dropout", "damageDropout", machDamageDropoutAttach);
    machKnob (machDamageTone,    machDamageToneL,    "Tone",    "damageTone",    machDamageToneAttach);
    machKnob (machDamageMix,     machDamageMixL,     "Mix",     "damageMix",     machDamageMixAttach);

    machToggle (machTimeOn, "timeBreakerOn", machTimeOnAttach);
    machToggle (machTimeSync, "timeBreakerSync", machTimeSyncAttach);
    machTimeDivision.addItemList ({ "1/1", "1/2", "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" }, 1);
    addAndMakeVisible (machTimeDivision);
    machTimeDivisionAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "timeBreakerDivision", machTimeDivision);
    machKnob (machTimeMix,     machTimeMixL,     "Mix",     "timeBreakerMix", machTimeMixAttach);
    machKnob (machTimeRate,    machTimeRateL,    "Rate",    "stutterRate",    machTimeRateAttach);
    machKnob (machTimeSize,    machTimeSizeL,    "Size",    "stutterSize",    machTimeSizeAttach);
    machKnob (machTimeChance,  machTimeChanceL,  "Chance",  "stutterChance",  machTimeChanceAttach);
    machKnob (machTimeReverse, machTimeReverseL, "Reverse", "reverseChance",  machTimeReverseAttach);
    setupDialLabel (machTimeRouteL, "Routes to");
    machTimeRouteL.setJustificationType (juce::Justification::centredLeft);
    auto setupRouteCombo = [&] (juce::ComboBox& b, const char* pid,
                                std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>& att)
    {
        b.addItemList ({ "None", "Grain Size", "Density", "Pitch", "Spray", "Pitch Jitter", "Reverb", "Echo Time", "Crush" }, 1);
        addAndMakeVisible (b);
        att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, pid, b);
    };
    setupRouteCombo (machTimeRoute1Target, "timeBreakerMod1Target", machTimeRoute1TargetAttach);
    setupRouteCombo (machTimeRoute2Target, "timeBreakerMod2Target", machTimeRoute2TargetAttach);
    machKnob (machTimeRoute1Depth, machTimeRoute1DepthL, "Depth", "timeBreakerMod1Depth", machTimeRoute1DepthAttach);
    machKnob (machTimeRoute2Depth, machTimeRoute2DepthL, "Depth", "timeBreakerMod2Depth", machTimeRoute2DepthAttach);

    // "Advanced" expanders: collapsed by default so each machine shows only its
    // essentials; toggling reveals the deep params and re-lays out the tab.
    auto setupMoreButton = [this] (juce::TextButton& b)
    {
        b.setClickingTogglesState (true);
        b.setToggleState (false, juce::dontSendNotification);
        addAndMakeVisible (b);
        b.onClick = [this, &b]
        {
            b.setButtonText (b.getToggleState() ? "Less" : "Advanced");
            updateTabVisibility();
            resized();
        };
    };
    setupMoreButton (machDamageMore);
    setupMoreButton (machTimeMore);

    // ---- HOME cockpit: macro knobs + signal-flow stage strip ----
    setupSectionLabel (homeTitle, "MK-ULTRA", 16.0f);
    setupSectionLabel (homeFlowLabel, "SIGNAL FLOW", 12.0f);
    {
        const char* mIds[kNumMacros]   = { "macroTexture", "macroBeauty", "macroSpace",
                                           "macroChaos", "macroMotion", "macroDamage", "macroEmotion" };
        const char* mNames[kNumMacros] = { "Texture", "Beauty", "Space", "Chaos", "Motion", "Damage", "Emotion" };
        for (int i = 0; i < kNumMacros; ++i)
        {
            setupMixKnob (macroKnobs[(size_t) i]);
            addAndMakeVisible (macroKnobs[(size_t) i]);
            setupDialLabel (macroLabels[(size_t) i], mNames[i]);
            macroAttach[(size_t) i] = std::make_unique<SliderAttachment> (proc.apvts, mIds[i], macroKnobs[(size_t) i]);
        }
    }

    // Morph pad: drag the puck to blend the four captured corners.
    addAndMakeVisible (morphPad);
    setupSectionLabel (morphPadLabel, "MORPH PAD", 12.0f);
    morphPad.setTooltip ("Drag to blend the four corner sounds. Capture each corner with the A/B/C/D buttons below.");
    morphCapA.onClick = [this] { proc.storeSlotA(); };
    morphCapB.onClick = [this] { proc.storeSlotB(); };
    morphCapC.onClick = [this] { proc.storeSlotC(); };
    morphCapD.onClick = [this] { proc.storeSlotD(); };
    for (auto* b : { &morphCapA, &morphCapB, &morphCapC, &morphCapD })
    {
        b->setTooltip ("Capture the current sound into this morph-pad corner");
        addAndMakeVisible (*b);
    }

    for (auto* b : { &homeTextureOn, &homeMachinesLabel, &homeSpaceOn, &homeMasterLabel })
        addAndMakeVisible (*b);
    homeMachinesLabel.setClickingTogglesState (false);  // display-only stage labels
    homeMasterLabel.setClickingTogglesState (false);
    homeTextureAttach = std::make_unique<ButtonAttachment> (proc.apvts, "textureGrainOn", homeTextureOn);
    homeSpaceAttach   = std::make_unique<ButtonAttachment> (proc.apvts, "beautySpaceOn", homeSpaceOn);

    // ---- Hover tooltips for the less-obvious controls ----
    machSpectralOn.setTooltip ("Spectral: freeze the spectrum into a sustained glassy pad");
    machPitchOn.setTooltip ("Master pitch shift (formant-preserving). Grain Pitch (Texture) pitches the grains; Pitch Lock (Master) snaps to a scale");
    machPitchFormant.setTooltip ("Preserve formants so shifted pitch stays natural instead of chipmunk");
    machDamageOn.setTooltip ("Damage: the full destruction unit (drive, SR reduction, bit crush, noise, dropouts, tone). Warmth + Lo-Fi are the gentle versions");
    machDamageClip.setTooltip ("Waveshaper character: Tube, Tape, Hard clip, Wavefold or Diode");
    machDamageBits.setTooltip ("Bit depth (1 = wrecked, 16 = clean)");
    machDamageRate.setTooltip ("Sample-rate reduction amount");
    machDamageJitter.setTooltip ("Digital instability: sample-rate wobble + dither");
    machDamageNoise.setTooltip ("Added hiss / noise");
    machDamageDropout.setTooltip ("Random sample dropouts / glitches");
    machDamageTone.setTooltip ("Post tone: dark to open");
    machTimeOn.setTooltip ("Time Breaker: beat-repeat / stutter / reverse glitch");
    machTimeSync.setTooltip ("Lock the stutter clock and slice length to host tempo");
    machTimeDivision.setTooltip ("Tempo division for the stutter clock when Sync is on");
    machTimeChance.setTooltip ("How often the clock triggers a stutter");
    machTimeRoute1Target.setTooltip ("Route the Time Breaker's tempo gate onto this knob");
    machTimeRoute2Target.setTooltip ("Route the Time Breaker's tempo gate onto this knob");
    machTimeRoute1Depth.setTooltip ("How far the routed knob moves with the gate");
    machTimeRoute2Depth.setTooltip ("How far the routed knob moves with the gate");
    lfoSyncBox.setTooltip ("Lock the Global Mod LFO rate to host tempo (Free = use the knob)");
    echoSyncBox.setTooltip ("Lock the Echo time to host tempo (Free = use the Echo knob)");
    globalModOnButton.setTooltip ("Global modulation LFO that sweeps the engine knobs");
    satOnButton.setTooltip ("Warmth - gentle tube/tape colour on the grains. For heavy destruction use the Damage machine");
    pitchLockButton.setTooltip ("Pitch Lock - snap the whole output to a key/scale. (Grain Pitch = grains; Pitch/Formant = master shift)");
    crushOnButton.setTooltip ("Lo-Fi - gentle bit-depth reduction. For full degradation use the Damage machine");
    knobs[2].slider.setTooltip ("Pitches the grains. Master shift = Pitch/Formant machine; scale snap = Pitch Lock (Master)");

    refreshPresetList();
    setSize (1020, 860);
    updateTabVisibility();

    // macOS render workaround: drive the editor through OpenGL to bypass the
    // broken native CoreGraphics path inside some hosts' plugin wrappers (FL
    // Studio etc.). Without it the layout renders scrambled / glyphs flip.
    // Attach last, after children exist and the size is set; detached in the dtor.
    // (The earlier Catalina "access violation on insert" was NOT this — it was the
    // missing CMAKE_OSX_DEPLOYMENT_TARGET stamping minos=26, now fixed in CMake.)
   #if JUCE_MAC
    openGLContext.attachTo (*this);
   #endif

    startTimerHz (30); // light: only updates rings/meter when something is moving
}

GrainFreezeEditor::~GrainFreezeEditor()
{
    openGLContext.detach(); // tear down GL before children/look-and-feel go away
    stopTimer();
    setLookAndFeel (nullptr);
}

void GrainFreezeEditor::initSpores()
{
    const float w = (float) juce::jmax (getWidth(), 1);
    const float h = (float) juce::jmax (getHeight() - 130, 1);
    for (auto& s : spores)
    {
        s.x = animRng.nextFloat() * w;
        s.y = animRng.nextFloat() * h;
        const float ang = animRng.nextFloat() * juce::MathConstants<float>::twoPi;
        const float spd = 0.06f + animRng.nextFloat() * 0.19f;       // px/frame at 60 Hz, slow drift
        s.vx = std::cos (ang) * spd;
        s.vy = std::sin (ang) * spd - 0.06f;                          // slight upward bias
        s.size = 1.4f + animRng.nextFloat() * 3.6f;
        s.phase = animRng.nextFloat() * juce::MathConstants<float>::twoPi;
        s.twinkle = 0.5f + animRng.nextFloat() * 1.6f;
    }
    sporesReady = true;
}

void GrainFreezeEditor::timerCallback()
{
    // Group parameter edits into ~0.4 s undo steps so Undo/Redo have a sensible
    // granularity (a burst of knob moves collapses into one undoable action).
    // 24 ticks at the 60 Hz timer rate ≈ 0.4 s.
    if (++undoTxnCounter >= 24)
    {
        undoTxnCounter = 0;
        proc.undoManager.beginNewTransaction();
    }

    // Lightweight: only do ambient repaints when something is actually moving
    // (audio level, the global mod LFO, or the Time Breaker gate). When idle, the
    // timer does nothing -> the UI is fully static and knob drags stay snappy.
    const float lvl = juce::jmax (proc.getOutputLevel (0), proc.getOutputLevel (1));
    const bool globalModOn = proc.apvts.getRawParameterValue ("globalModOn")->load() > 0.5f;
    const bool animating = lvl > 0.001f || globalModOn || proc.getTimeBreakerGate() > 0.001f;

    if (animating)
    {
        auto updateKnobRing = [this] (LabeledKnob& k)
        {
            if (k.ring == nullptr) return;
            const float off = proc.getModOffset (k.id) + proc.getTimeBreakerModOffset (k.id);
            k.ring->setOffset (off);
            k.ring->repaint();
            const bool active = std::abs (off) > 0.01f;
            k.modButton.setColour (juce::TextButton::textColourOffId,
                                   active ? lnf.accent() : gf::BiohazardLookAndFeel::textCol);
        };
        for (auto& k : knobs)       updateKnobRing (k);
        for (auto& k : prettyKnobs) updateKnobRing (k);
        if (meter != nullptr) meter->repaint();
        // Live visualizers — repaint only while there's signal/mod to show, so the
        // UI still goes fully static (zero repaints) when idle.
        if (modScope != nullptr && modScope->isVisible())               modScope->repaint();
        if (waveformDisplay != nullptr && waveformDisplay->isVisible())  waveformDisplay->repaint();
        if (spectrumDisplay != nullptr && spectrumDisplay->isVisible())  spectrumDisplay->repaint();
    }

    // FREEZE lamp — repaints only on state change.
    if (const bool ready = proc.sampleFreezeReady(); ready != sampleReadyShown)
    {
        sampleReadyShown = ready;
        sampleFreezeButton.setColour (juce::TextButton::textColourOffId,
                                      ready ? lnf.accent() : gf::BiohazardLookAndFeel::textCol);
        sampleFreezeButton.repaint();
    }
}

void GrainFreezeEditor::addKnob (LabeledKnob& k, gf::ParamId id, const juce::String& paramID, const juce::String& name)
{
    k.id = id;
    k.paramID = paramID;
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 70, 18);
    k.slider.setMouseDragSensitivity (kKnobDragSensitivity); // shorter sweep = snappier feel
    k.slider.setPopupDisplayEnabled (true, false, this); // drag-only: hover popups get stuck as orphaned bubbles
    addAndMakeVisible (k.slider);

    // Ring sits behind the slider so the knob stays fully interactive.
    k.ring = std::make_unique<ModRing> (k.slider);
    addAndMakeVisible (*k.ring);
    k.ring->toBack();

    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (k.label);

    k.lock.setComponentID ("lock"); // drawn as a padlock by the LookAndFeel
    k.lock.setTooltip ("Lock this control — keeps it fixed when you Randomize");
    k.lock.onClick = [this, &k] { proc.randomizer.setLocked (k.id, k.lock.getToggleState()); };
    addAndMakeVisible (k.lock);

    k.modButton.setComponentID ("settings"); // drawn as a bare gear icon
    k.modButton.setColour (juce::TextButton::textColourOffId, gf::BiohazardLookAndFeel::textCol);
    k.modButton.setTooltip ("Modulation settings");
    k.modButton.onClick = [this, &k, name] { openModPanel (k, name); };
    addAndMakeVisible (k.modButton);

    k.attachment = std::make_unique<SliderAttachment> (proc.apvts, paramID, k.slider);
}

void GrainFreezeEditor::openModPanel (LabeledKnob& k, const juce::String& name)
{
    auto panel = std::make_unique<ModPanel> (proc.apvts, k.id, k.paramID, name);
    panel->setLookAndFeel (&lnf);
    juce::CallOutBox::launchAsynchronously (
        std::move (panel),
        k.modButton.getScreenBounds(),
        nullptr);
}

void GrainFreezeEditor::refreshPresetList()
{
    presetBox.clear (juce::dontSendNotification);
    auto names = proc.presets.getPresetNames();
    for (int i = 0; i < names.size(); ++i)
        presetBox.addItem (names[i], i + 1);

    auto current = proc.presets.getCurrentPresetName();
    if (current.isNotEmpty())
        presetBox.setText (current, juce::dontSendNotification);
}

void GrainFreezeEditor::switchTab (int tabIndex)
{
    currentTab = juce::jlimit (0, 4, tabIndex);
    updateTabVisibility();
    resized();
    repaint(); // instant switch (no cross-fade) for snappiness
}

void GrainFreezeEditor::layoutDialCell (juce::Rectangle<int>& row, juce::Label& label,
                                        juce::Slider& slider, int width)
{
    auto cell = row.removeFromLeft (width).reduced (8, 6);
    label.setBounds (cell.removeFromTop (20));
    slider.setBounds (cell);
}

void GrainFreezeEditor::updateTabVisibility()
{
    const bool entropyTab = currentTab == 0;
    const bool mixTab = currentTab == 1;
    const bool prettifierTab = currentTab == 2;
    const bool machinesTab = currentTab == 3;
    const bool homeTab = currentTab == 4;
    constexpr bool inputToolsVisible = kMkUltraExperimentalInputTools;

    tabHome.setToggleState (homeTab, juce::dontSendNotification);
    tabEntropy.setToggleState (entropyTab, juce::dontSendNotification);
    tabMix.setToggleState (mixTab, juce::dontSendNotification);
    tabPrettifier.setToggleState (prettifierTab, juce::dontSendNotification);
    tabMachines.setToggleState (machinesTab, juce::dontSendNotification);

    lnf.setAccentTheme ((entropyTab || machinesTab || homeTab) ? gf::BiohazardLookAndFeel::AccentTheme::entropy
                        : mixTab     ? gf::BiohazardLookAndFeel::AccentTheme::mix
                                     : gf::BiohazardLookAndFeel::AccentTheme::prettifier);
    sendLookAndFeelChange();

    // HOME cockpit controls.
    homeTitle.setVisible (homeTab);
    homeFlowLabel.setVisible (homeTab);
    for (auto& s : macroKnobs)  s.setVisible (homeTab);
    for (auto& l : macroLabels) l.setVisible (homeTab);
    morphPad.setVisible (homeTab);
    morphPadLabel.setVisible (homeTab);
    for (auto* b : { &morphCapA, &morphCapB, &morphCapC, &morphCapD })
        b->setVisible (homeTab);
    for (auto* b : { &homeTextureOn, &homeMachinesLabel, &homeSpaceOn, &homeMasterLabel })
        b->setVisible (homeTab);

    freezeButton.setVisible (entropyTab);
    for (auto& k : knobs)
    {
        k.slider.setVisible (entropyTab);
        k.label.setVisible (entropyTab);
        k.lock.setVisible (entropyTab);
        k.modButton.setVisible (entropyTab);
        if (k.ring != nullptr) k.ring->setVisible (entropyTab);
    }
    globalModOnButton.setVisible (entropyTab);
    globalRate.setVisible (entropyTab);
    globalShape.setVisible (entropyTab);
    lfoSyncBox.setVisible (entropyTab);
    satOnButton.setVisible (entropyTab);
    satType.setVisible (entropyTab);
    satDrive.setVisible (entropyTab);
    satMix.setVisible (entropyTab);
    satTypeLabel.setVisible (entropyTab);
    satDriveLabel.setVisible (entropyTab);
    satMixLabel.setVisible (entropyTab);
    if (satCurve != nullptr) satCurve->setVisible (entropyTab);
    if (meter != nullptr) meter->setVisible (entropyTab);
    if (modScope != nullptr) modScope->setVisible (entropyTab);
    // The Mix tab swaps the bottom output scope for the master spectrum analyzer.
    if (waveformDisplay != nullptr) waveformDisplay->setVisible (! mixTab && ! machinesTab && ! homeTab);
    if (spectrumDisplay != nullptr) spectrumDisplay->setVisible (mixTab);
    // Legacy spectral freeze retired from the UI — the Spectral machine (Machines
    // tab) is the single home for spectral now.
    specFreezeButton.setVisible (false);
    specLabel.setVisible (false);
    specMix.setVisible (false);
    specShimmer.setVisible (false);

    midiEnableButton.setVisible (entropyTab && inputToolsVisible);
    for (auto* c : { &midiRootSlider, &midiGlideSlider, &midiVelAmpSlider })
        c->setVisible (entropyTab && inputToolsVisible);
    for (auto* l : { &midiRootLabel, &midiGlideLabel, &midiVelAmpLabel })
        l->setVisible (entropyTab && inputToolsVisible);

    for (auto* c : { &dryLevel, &entropySend, &entropyReturn, &prettifierSend, &prettifierReturn, &mixOutput, &chaosBeauty, &mixWidth, &mixGlue, &mixCeiling })
        c->setVisible (mixTab);
    routingMode.setVisible (mixTab);
    mixHeader.setVisible (mixTab);
    routingLabel.setVisible (mixTab);
    for (auto& l : mixLabels) l.setVisible (mixTab);
    eqHeader.setVisible (mixTab);
    for (auto& l : eqLabels) l.setVisible (mixTab);
    for (auto* c : { &eqLowKnob, &eqMidKnob, &eqHighKnob, &eqLoFiKnob })
        c->setVisible (mixTab);
    for (auto* c : { &pluginOnButton, &entropyOnButton, &limiterOnButton, &mixEqOnButton,
                     &pitchMatchOnButton, &tempoLockOnButton })
        c->setVisible (mixTab);

    sampleHeader.setVisible (mixTab && inputToolsVisible);
    sampleModeButton.setVisible (mixTab && inputToolsVisible);
    sampleFreezeButton.setVisible (mixTab && inputToolsVisible);
    sampleSourceBox.setVisible (mixTab && inputToolsVisible);
    for (auto* c : { &sampleWindowSlider, &sampleLevelSlider })
        c->setVisible (mixTab && inputToolsVisible);
    for (auto* l : { &sampleSourceLabel, &sampleWindowLabel, &sampleLevelLabel })
        l->setVisible (mixTab && inputToolsVisible);

    pitchLockHeader.setVisible (false); // cluster now lives on the routing band
    pitchLockButton.setVisible (mixTab);
    pitchLockFormantButton.setVisible (mixTab);
    pitchLockAmountSlider.setVisible (mixTab);
    for (auto* c : { &pitchLockModeBox, &pitchLockKeyBox, &pitchLockScaleBox })
        c->setVisible (mixTab);
    for (auto* l : { &pitchLockModeLabel, &pitchLockKeyLabel, &pitchLockScaleLabel, &pitchLockAmountLabel })
        l->setVisible (mixTab);

    prettifierHeader.setVisible (prettifierTab);
    prettifierOnButton.setVisible (prettifierTab || mixTab);
    for (auto& k : prettyKnobs)
    {
        k.slider.setVisible (prettifierTab);
        k.label.setVisible (prettifierTab);
        k.lock.setVisible (prettifierTab);
        k.modButton.setVisible (prettifierTab);
        if (k.ring != nullptr) k.ring->setVisible (prettifierTab);
    }
    prettyOutputKnob.setVisible (prettifierTab);
    prettyOutputLabel.setVisible (prettifierTab);

    dnaHeader.setVisible (prettifierTab);
    for (auto& s : dnaKnobs)  s.setVisible (prettifierTab);
    for (auto& l : dnaLabels) l.setVisible (prettifierTab);
    for (auto* b : { &echoOnButton, &reverbOnButton, &chorusOnButton, &crushOnButton, &phaserOnButton, &flangerOnButton })
        b->setVisible (prettifierTab);
    echoSyncBox.setVisible (prettifierTab);

    // MACHINES tab. Damage and Time Breaker collapse to essentials; their deep
    // params appear only when the per-machine "Advanced" toggle is on.
    const bool damageMore = machDamageMore.getToggleState();
    const bool timeMore   = machTimeMore.getToggleState();
    machinesHeader.setVisible (machinesTab);
    for (auto* l : { &machSpectralTitle, &machPitchTitle, &machDamageTitle, &machTimeTitle })
        l->setVisible (machinesTab);
    for (auto* b : { &machSpectralOn, &machPitchOn, &machPitchFormant, &machDamageOn, &machTimeOn, &machTimeSync })
        b->setVisible (machinesTab);
    for (auto* b : { &machDamageMore, &machTimeMore })
        b->setVisible (machinesTab);
    // Always-visible essentials (Spectral + Pitch are already minimal).
    for (auto* s : { &machSpectralMix, &machSpectralAmount, &machPitchMix, &machPitchShift,
                     &machDamageAmount, &machDamageMix, &machTimeChance, &machTimeMix })
        s->setVisible (machinesTab);
    for (auto* l : { &machSpectralMixL, &machSpectralAmountL, &machPitchMixL, &machPitchShiftL,
                     &machDamageAmountL, &machDamageMixL, &machTimeChanceL, &machTimeMixL })
        l->setVisible (machinesTab);
    // Damage advanced knobs.
    for (auto* s : { &machDamageBits, &machDamageRate, &machDamageJitter,
                     &machDamageNoise, &machDamageDropout, &machDamageTone })
        s->setVisible (machinesTab && damageMore);
    for (auto* l : { &machDamageBitsL, &machDamageRateL, &machDamageJitterL,
                     &machDamageNoiseL, &machDamageDropoutL, &machDamageToneL })
        l->setVisible (machinesTab && damageMore);
    // Time Breaker advanced knobs + routing.
    for (auto* s : { &machTimeRate, &machTimeSize, &machTimeReverse })
        s->setVisible (machinesTab && timeMore);
    for (auto* l : { &machTimeRateL, &machTimeSizeL, &machTimeReverseL })
        l->setVisible (machinesTab && timeMore);
    machDamageClip.setVisible (machinesTab);
    machTimeDivision.setVisible (machinesTab);
    machTimeRouteL.setVisible (machinesTab && timeMore);
    machTimeRoute1Target.setVisible (machinesTab && timeMore);
    machTimeRoute2Target.setVisible (machinesTab && timeMore);
    for (auto* s : { &machTimeRoute1Depth, &machTimeRoute2Depth })
        s->setVisible (machinesTab && timeMore);
    for (auto* l : { &machTimeRoute1DepthL, &machTimeRoute2DepthL })
        l->setVisible (machinesTab && timeMore);

    if (keyboard != nullptr)
        keyboard->setVisible (inputToolsVisible);
}

void GrainFreezeEditor::paint (juce::Graphics& g)
{
    using LF = gf::BiohazardLookAndFeel;

    // Fully static, lightweight background: no boot fade, no glow breathe, no
    // flicker, no drifting bloom, no spores. paint() only runs on real repaints.
    const float pulse = 0.12f;
    const float boot = 1.0f;   // no boot fade now; logo halo still scales by this
    const auto bounds = getLocalBounds().toFloat();
    const int tab = currentTab;

    auto drawBgImage = [&g, &bounds] (const juce::Image& img, float opacity)
    {
        if (! img.isValid()) return;
        g.setOpacity (opacity);
        g.drawImage (img, bounds, juce::RectanglePlacement::fillDestination);
        g.setOpacity (1.0f);
    };

    const auto acc = lnf.accent();

    // Static base gradient per tab tint.
    {
        const bool pretty = (tab == 2);
        const juce::Colour topCol = pretty   ? juce::Colour (0xff3b3322)
                                  : tab == 1 ? juce::Colour (0xff13171e)
                                             : juce::Colour (0xff111711);
        const juce::Colour midCol = pretty ? LF::bg.brighter (0.14f) : LF::bg;
        const juce::Colour botCol = pretty ? LF::bg.darker (0.22f) : LF::bg.darker (0.55f);
        juce::ColourGradient base (topCol, bounds.getCentreX(), 0.0f,
                                   botCol, bounds.getCentreX(), bounds.getHeight(), false);
        base.addColour (0.45, midCol);
        g.setGradientFill (base);
        g.fillRect (bounds);
    }

    // Background artwork (static, subtle).
    if (tab == 0 || tab == 3 || tab == 4)
        drawBgImage (bgImage, 0.16f);
    else if (tab == 2)
        drawBgImage (prettifierBgImage, 0.34f);
    else
        drawBgImage (mixBgImage, 0.42f);

    // Mix tab: static warm aura.
    if (tab == 1)
    {
        const float auraA = 0.10f;
        juce::ColourGradient aura (LF::gold.withAlpha (auraA),
                                   bounds.getCentreX(), bounds.getHeight() * 0.40f,
                                   juce::Colours::transparentBlack,
                                   bounds.getCentreX(), bounds.getHeight() * 0.95f, true);
        aura.addColour (0.5, LF::gold.withAlpha (auraA * 0.5f));
        g.setGradientFill (aura);
        g.fillRect (bounds);
    }

    // Soft top accent wash (static).
    {
        juce::ColourGradient topGlow (acc.withAlpha (pulse * 0.24f), bounds.getCentreX(), -40.0f,
                                      juce::Colours::transparentBlack, bounds.getCentreX(), bounds.getHeight() * 0.55f, false);
        g.setGradientFill (topGlow);
        g.fillRect (bounds);
    }

    // Logo watermark on the Texture tab (static, subtle).
    if (tab == 0 && logoImage.isValid() && ! watermark.isEmpty())
    {
        auto wmArea = watermark.getBounds();
        g.setOpacity (0.06f);
        g.drawImage (logoImage, wmArea, juce::RectanglePlacement::centred);
        g.setOpacity (1.0f);
    }

    // Header: brand logo only (switches per tab). No title text, no box -- the
    // logo's black background is keyed out in prepLogo() so it blends cleanly.
    // Tabs are real buttons positioned beside the logo in resized().
    {
        // Per tab: Entropy = native green emblem, Mix = white emblem, Prettifier = its own art.
        const bool entropyTab = (tab == 0);
        const bool mixTab     = (tab == 1);
        const juce::Colour orange (0xffff8a1e);
        const juce::Image& brandLogo = (tab == 2 && prettifierLogoImage.isValid()) ? prettifierLogoImage
                                     : (mixTab && whiteLogoImage.isValid())        ? whiteLogoImage
                                     : (entropyTab && greenLogoImage.isValid())    ? greenLogoImage
                                                                                   : logoImage;
        if (brandLogo.isValid())
        {
            auto logoBox = getLocalBounds().reduced (20).removeFromTop (kHeaderH)
                               .removeFromLeft (kLogoSlot).toFloat().reduced (2.0f);

            // Soft halo behind the emblem: Entropy = large breathing green, Mix =
            // large breathing orange (both clearly lit), other tabs keep accent.
            const bool tinted = entropyTab || mixTab;
            const juce::Colour glowCol = entropyTab ? LF::toxic : mixTab ? orange : acc;
            const float breathe   = 1.0f + 0.05f * std::sin (animPhase * 1.7f);
            const float haloScale = tinted ? 0.42f * breathe : 0.35f;
            const float haloAlpha = (entropyTab ? 0.16f : mixTab ? 0.18f : 0.18f) + glowLevel * 0.05f;
            const auto halo = logoBox.expanded (logoBox.getWidth() * haloScale);

            // On the busy/light Mix backdrop, seat the emblem on a soft dark disc so
            // the white logo and orange glow separate from the background.
            if (mixTab)
            {
                const auto seat = halo.reduced (halo.getWidth() * 0.18f);
                juce::ColourGradient dark (juce::Colours::black.withAlpha (0.45f * boot),
                                           seat.getCentreX(), seat.getCentreY(),
                                           juce::Colours::transparentBlack, seat.getRight(), seat.getBottom(), true);
                g.setGradientFill (dark);
                g.fillEllipse (seat);
            }

            // Mix keeps only its dark seat disc (drawn above) — no coloured glow.
            if (! mixTab)
            {
                juce::ColourGradient glow (glowCol.withAlpha (haloAlpha * boot),
                                           halo.getCentreX(), halo.getCentreY(),
                                           juce::Colours::transparentBlack, halo.getRight(), halo.getBottom(), true);
                if (tinted) // gentle inner core that fades out
                    glow.addColour (0.32, glowCol.brighter (0.1f).withAlpha (haloAlpha * 0.6f * boot));
                g.setGradientFill (glow);
                g.fillEllipse (halo);
            }

            g.setOpacity (boot);
            g.drawImage (brandLogo, logoBox, juce::RectanglePlacement::centred);
            g.setOpacity (1.0f);
        }
    }

    // Soft vignette for focus (gentle, modern).
    {
        const float vigAlpha = tab == 2 ? 0.16f : 0.26f;
        juce::ColourGradient vig (juce::Colours::transparentBlack,
                                  bounds.getCentreX(), bounds.getCentreY(),
                                  juce::Colours::black.withAlpha (vigAlpha),
                                  bounds.getCentreX(), bounds.getBottom(), true);
        vig.addColour (0.7, juce::Colours::transparentBlack);
        g.setGradientFill (vig);
        g.fillRect (bounds);
    }

    if (bootPhase < 1.0f)
    {
        g.setColour (LF::bg.withAlpha ((1.0f - bootPhase) * 0.9f));
        g.fillRect (bounds);
    }
}

void GrainFreezeEditor::resized()
{
    constexpr int pad = 20;
    constexpr int gap = 10;
    constexpr bool inputToolsVisible = kMkUltraExperimentalInputTools;
    fadeOverlay.setBounds (getLocalBounds());
    auto area = getLocalBounds().reduced (pad);

    // Header band: brand logo (painted in paint()) on the left, tabs aligned
    // beside it on the same row.
    auto header = area.removeFromTop (kHeaderH);
    header.removeFromLeft (kLogoSlot + 16);   // reserve room for the painted logo
    constexpr int tabH = 32;
    auto placeTab = [&] (juce::TextButton& b, int w)
    {
        b.setBounds (header.removeFromLeft (w).withSizeKeepingCentre (w, tabH));
        header.removeFromLeft (gap);
    };
    // Visual order follows the signal chain: Home, Texture, Machines, Space, Master.
    placeTab (tabHome, 78);
    placeTab (tabEntropy, 104);
    placeTab (tabMachines, 120);
    placeTab (tabPrettifier, 92);
    placeTab (tabMix, 102);

    area.removeFromTop (gap);

    // Utility row, grouped: [A/B compare] | [tools] ........ [Freeze] [Panic].
    // The left cluster starts at the same x as Randomize below, so the two rows
    // line up. Freeze (Entropy-only) sits on the right next to Panic.
    auto util = area.removeFromTop (36);

    // A/B compare cluster — left edge aligned with the Randomize button.
    buttonA.setBounds (util.removeFromLeft (32).reduced (2, 4));
    util.removeFromLeft (4);
    buttonB.setBounds (util.removeFromLeft (32).reduced (2, 4));
    util.removeFromLeft (gap);
    copyAToBButton.setBounds (util.removeFromLeft (66).reduced (2, 4));
    util.removeFromLeft (5);
    copyBToAButton.setBounds (util.removeFromLeft (66).reduced (2, 4));
    util.removeFromLeft (5);
    resetBButton.setBounds (util.removeFromLeft (74).reduced (2, 4));
    util.removeFromLeft (gap * 2);

    // Tools cluster: undo / redo / init.
    undoButton.setBounds (util.removeFromLeft (34).reduced (2, 4));
    util.removeFromLeft (4);
    redoButton.setBounds (util.removeFromLeft (34).reduced (2, 4));
    util.removeFromLeft (5);
    initButton.setBounds (util.removeFromLeft (56).reduced (2, 4));
    util.removeFromLeft (gap);
    randomizeAllButton.setBounds (util.removeFromLeft (120).reduced (2, 4));

    // Right side: Panic, with Freeze just to its left.
    panicButton.setBounds (util.removeFromRight (104).reduced (2, 4));
    util.removeFromRight (gap);
    freezeButton.setBounds (util.removeFromRight (104).reduced (2, 4));

    area.removeFromTop (gap);

    // Row 2: preset bar.
    auto bar = area.removeFromTop (36);
    randomizeButton.setBounds (bar.removeFromLeft (120).reduced (2, 4));
    bar.removeFromLeft (gap);
    prevButton.setBounds  (bar.removeFromLeft (36).reduced (2, 4));
    nextButton.setBounds  (bar.removeFromLeft (36).reduced (2, 4));
    bar.removeFromLeft (gap);
    presetBox.setBounds   (bar.removeFromLeft (200).reduced (2, 4));
    bar.removeFromLeft (gap);
    browseButton.setBounds (bar.removeFromLeft (84).reduced (2, 4));
    bar.removeFromLeft (gap);
    saveButton.setBounds  (bar.removeFromRight (88).reduced (2, 4));
    presetName.setBounds  (bar.reduced (2, 4));

    area.removeFromTop (gap + 4);

    if (inputToolsVisible)
    {
        auto keyboardArea = area.removeFromBottom (76);
        if (currentTab == 0)
        {
            auto midiStrip = keyboardArea.removeFromLeft (372);
            keyboardArea.removeFromLeft (gap);
            midiEnableButton.setBounds (midiStrip.removeFromLeft (88).withSizeKeepingCentre (84, 30));
            auto midiCell = [&] (juce::Slider& s, juce::Label& l)
            {
                auto c = midiStrip.removeFromLeft (92).reduced (4, 4);
                l.setBounds (c.removeFromBottom (16));
                s.setBounds (c);
            };
            midiCell (midiRootSlider, midiRootLabel);
            midiCell (midiGlideSlider, midiGlideLabel);
            midiCell (midiVelAmpSlider, midiVelAmpLabel);
        }
        if (keyboard != nullptr)
            keyboard->setBounds (keyboardArea.reduced (4));
    }

    auto waveArea = area.removeFromBottom (66);
    if (waveformDisplay != nullptr)
        waveformDisplay->setBounds (waveArea.reduced (4));
    if (spectrumDisplay != nullptr)
        spectrumDisplay->setBounds (waveArea.reduced (4));
    area.removeFromBottom (gap);

    // Entropy-only support rows (saturation / spectral) carved from the bottom.
    auto bottom = area.removeFromBottom (currentTab == 0 ? 122 : 0);
    if (currentTab == 0) area.removeFromBottom (gap);
    auto specRow = area.removeFromBottom (currentTab == 0 ? 74 : 0);
    if (currentTab == 0) area.removeFromBottom (gap);

    // Shared knob-grid layout used by both Entropy and Prettifier so the two
    // sections share the same look and spacing.
    auto layoutKnobGrid = [] (juce::Rectangle<int> gridArea, int cols, int rows,
                              int count, const std::function<void (int, juce::Rectangle<int>)>& perCell)
    {
        const int cellW = gridArea.getWidth() / cols;
        const int cellH = gridArea.getHeight() / juce::jmax (1, rows);
        for (int idx = 0; idx < count; ++idx)
        {
            const int r = idx / cols, c = idx % cols;
            auto cell = juce::Rectangle<int> (gridArea.getX() + c * cellW,
                                              gridArea.getY() + r * cellH,
                                              cellW, cellH).reduced (10, 8);
            perCell (idx, cell);
        }
    };

    if (currentTab == 0)
    {
        const float wmSize = (float) juce::jmin (area.getWidth(), area.getHeight()) * 1.05f;
        auto wmBounds = juce::Rectangle<float> (wmSize, wmSize).withCentre (area.toFloat().getCentre());
        watermark = gf::makeBiohazardPath (wmBounds);

        layoutKnobGrid (area, 5, 2, kNumKnobs, [this] (int idx, juce::Rectangle<int> cell)
        {
            auto top = cell.removeFromTop (20);
            knobs[(size_t) idx].modButton.setBounds (top.removeFromRight (26).reduced (1, 0));
            knobs[(size_t) idx].lock.setBounds (top.removeFromRight (28));
            knobs[(size_t) idx].label.setBounds (top);
            knobs[(size_t) idx].slider.setBounds (cell);
            if (knobs[(size_t) idx].ring != nullptr)
                knobs[(size_t) idx].ring->setBounds (cell.withTrimmedBottom (20));
        });
    }
    else if (currentTab == 2)
    {
        prettifierHeader.setBounds (area.removeFromTop (24).reduced (4, 0));
        prettifierOnButton.setBounds (area.removeFromTop (34).removeFromLeft (150).reduced (2, 4));
        area.removeFromTop (gap);

        // Reserve the DNA panel (module toggles + 10 character knobs) at the bottom.
        auto dnaSection = area.removeFromBottom (132);
        area.removeFromBottom (gap);

        layoutKnobGrid (area, 5, 2, kNumPrettyKnobs, [this] (int idx, juce::Rectangle<int> cell)
        {
            auto top = cell.removeFromTop (20);
            prettyKnobs[(size_t) idx].modButton.setBounds (top.removeFromRight (26).reduced (1, 0));
            prettyKnobs[(size_t) idx].lock.setBounds (top.removeFromRight (28));
            prettyKnobs[(size_t) idx].label.setBounds (top);
            prettyKnobs[(size_t) idx].slider.setBounds (cell);
            if (prettyKnobs[(size_t) idx].ring != nullptr)
                prettyKnobs[(size_t) idx].ring->setBounds (cell.withTrimmedBottom (20));
        });

        // Prettifier output knob fills the empty bottom-right cell of the 5x2 grid.
        const int pCellW = area.getWidth() / 5;
        const int pCellH = area.getHeight() / 2;
        auto outCell = juce::Rectangle<int> (area.getX() + 4 * pCellW, area.getY() + pCellH,
                                             pCellW, pCellH).reduced (10, 8);
        prettyOutputLabel.setBounds (outCell.removeFromTop (20));
        prettyOutputKnob.setBounds (outCell);

        // DNA panel: a row of module on/off pills, then a row of 10 character knobs.
        dnaHeader.setBounds (dnaSection.removeFromTop (16).reduced (4, 0));
        auto modRow = dnaSection.removeFromTop (28);
        echoSyncBox.setBounds (modRow.removeFromLeft (80).withSizeKeepingCentre (76, 22));
        juce::ToggleButton* mods[6] = { &echoOnButton, &reverbOnButton, &chorusOnButton, &crushOnButton,
                                        &phaserOnButton, &flangerOnButton };
        const int modW = juce::jmin (112, modRow.getWidth() / 6);
        modRow = modRow.withSizeKeepingCentre (modW * 6, modRow.getHeight());
        for (auto* m : mods)
            m->setBounds (modRow.removeFromLeft (modW).reduced (3, 1));

        dnaSection.removeFromTop (4);
        const int dnaW = dnaSection.getWidth() / kNumDna;
        for (int i = 0; i < kNumDna; ++i)
        {
            auto cell = dnaSection.removeFromLeft (dnaW).reduced (3, 2);
            dnaLabels[(size_t) i].setBounds (cell.removeFromBottom (14));
            dnaKnobs[(size_t) i].setBounds (cell);
        }
    }
    else if (currentTab == 1)
    {
        // Signal-flow layout: input -> two send/return loops on the LEFT (each
        // headed by its engine on/off) -> MASTER + EQ on the RIGHT. Routing and
        // Pitch Lock sit in a band along the bottom.
        auto topToggles = area.removeFromTop (34);
        pluginOnButton.setBounds (topToggles.removeFromLeft (92).reduced (2, 4));
        topToggles.removeFromLeft (gap);
        limiterOnButton.setBounds (topToggles.removeFromLeft (92).reduced (2, 4));
        topToggles.removeFromLeft (gap);
        mixEqOnButton.setBounds (topToggles.removeFromLeft (72).reduced (2, 4));
        topToggles.removeFromLeft (gap);
        pitchMatchOnButton.setBounds (topToggles.removeFromLeft (118).reduced (2, 4));
        topToggles.removeFromLeft (gap);
        tempoLockOnButton.setBounds (topToggles.removeFromLeft (118).reduced (2, 4));

        if (inputToolsVisible)
        {
            area.removeFromTop (gap);
            sampleHeader.setBounds (area.removeFromTop (18).reduced (4, 0));
            area.removeFromTop (2);
            auto sampleRow = area.removeFromTop (48);
            sampleModeButton.setBounds (sampleRow.removeFromLeft (130).withSizeKeepingCentre (126, 30));
            sampleRow.removeFromLeft (gap);
            sampleFreezeButton.setBounds (sampleRow.removeFromLeft (96).withSizeKeepingCentre (96, 32));
            sampleRow.removeFromLeft (gap * 2);
            {
                auto c = sampleRow.removeFromLeft (110);
                sampleSourceLabel.setBounds (c.removeFromTop (14));
                sampleSourceBox.setBounds (c.withSizeKeepingCentre (106, 28));
            }
            sampleRow.removeFromLeft (gap);
            auto sampleKnob = [&] (juce::Slider& s, juce::Label& l)
            {
                auto c = sampleRow.removeFromLeft (80);
                l.setBounds (c.removeFromBottom (14));
                s.setBounds (c.reduced (2, 0));
            };
            sampleKnob (sampleWindowSlider, sampleWindowLabel);
            sampleKnob (sampleLevelSlider, sampleLevelLabel);
        }

        area.removeFromTop (gap + 4);

        // Routing + Pitch Lock band reserved along the bottom.
        auto routeRow = area.removeFromBottom (46);
        area.removeFromBottom (gap);
        routingLabel.setBounds (routeRow.removeFromLeft (66).withSizeKeepingCentre (66, 26));
        routingMode.setBounds (routeRow.removeFromLeft (168).withSizeKeepingCentre (164, 28));
        routeRow.removeFromLeft (gap * 2);
        pitchLockButton.setBounds (routeRow.removeFromLeft (116).withSizeKeepingCentre (112, 28));
        routeRow.removeFromLeft (gap);
        auto plCombo = [&] (juce::Label& l, juce::ComboBox& b, int w)
        {
            auto c = routeRow.removeFromLeft (w);
            l.setBounds (c.removeFromTop (14));
            b.setBounds (c.removeFromTop (28));
            routeRow.removeFromLeft (gap);
        };
        plCombo (pitchLockModeLabel,  pitchLockModeBox,  104);
        plCombo (pitchLockKeyLabel,   pitchLockKeyBox,   58);
        plCombo (pitchLockScaleLabel, pitchLockScaleBox, 130);
        {
            auto c = routeRow.removeFromLeft (62);
            pitchLockAmountLabel.setBounds (c.removeFromBottom (13));
            pitchLockAmountSlider.setBounds (c.reduced (2, 0));
        }
        routeRow.removeFromLeft (gap);
        pitchLockFormantButton.setBounds (routeRow.removeFromLeft (104).withSizeKeepingCentre (100, 28));

        // Split the body: LEFT loops | RIGHT master.
        auto leftCol = area.removeFromLeft (juce::jmax (300, area.getWidth() * 42 / 100));
        area.removeFromLeft (gap * 2);
        auto rightCol = area;

        // ---- LEFT: one send/return loop per engine, headed by its on/off ----
        auto loopPanel = [&] (juce::Rectangle<int> r, juce::ToggleButton& onBtn,
                              juce::Slider& send, juce::Label& sendL,
                              juce::Slider& ret,  juce::Label& retL)
        {
            onBtn.setBounds (r.removeFromTop (30).withTrimmedLeft (6).withWidth (180));
            r.removeFromTop (2);
            const int half = r.getWidth() / 2;
            { auto c = r.removeFromLeft (half).reduced (10, 6); sendL.setBounds (c.removeFromTop (16)); send.setBounds (c); }
            { auto c = r.reduced (10, 6);                        retL.setBounds (c.removeFromTop (16)); ret.setBounds (c); }
        };
        const int loopH = leftCol.getHeight() / 2;
        loopPanel (leftCol.removeFromTop (loopH).reduced (2), entropyOnButton,
                   entropySend, mixLabels[1], entropyReturn, mixLabels[2]);
        loopPanel (leftCol.reduced (2), prettifierOnButton,
                   prettifierSend, mixLabels[3], prettifierReturn, mixLabels[4]);

        // ---- RIGHT: master knobs (2x3) then EQ ----
        mixHeader.setBounds (rightCol.removeFromTop (22).reduced (4, 0));
        rightCol.removeFromTop (2);
        auto masterArea = rightCol.removeFromTop (rightCol.getHeight() * 58 / 100);
        std::pair<juce::Slider*, juce::Label*> mk[] = {
            { &dryLevel, &mixLabels[0] }, { &mixWidth, &mixLabels[7] },  { &mixGlue, &mixLabels[8] },
            { &mixCeiling, &mixLabels[9] }, { &mixOutput, &mixLabels[5] }, { &chaosBeauty, &mixLabels[6] } };
        const int mkW = masterArea.getWidth() / 3;
        auto mrow1 = masterArea.removeFromTop (masterArea.getHeight() / 2);
        for (int i = 0; i < 3; ++i) layoutDialCell (mrow1, *mk[i].second, *mk[i].first, mkW);
        auto mrow2 = masterArea;
        for (int i = 3; i < 6; ++i) layoutDialCell (mrow2, *mk[i].second, *mk[i].first, mkW);

        rightCol.removeFromTop (gap);
        eqHeader.setBounds (rightCol.removeFromTop (20).reduced (4, 0));
        rightCol.removeFromTop (2);
        const int eqW = rightCol.getWidth() / 4;
        juce::Slider* eqSliders[] = { &eqLowKnob, &eqMidKnob, &eqHighKnob, &eqLoFiKnob };
        for (size_t i = 0; i < eqLabels.size(); ++i)
            layoutDialCell (rightCol, eqLabels[i], *eqSliders[i], eqW);
    }
    else if (currentTab == 3)
    {
        machinesHeader.setBounds (area.removeFromTop (24).reduced (4, 0));
        area.removeFromTop (gap);

        // One stacked block per machine: [title + On (+extra toggle)] over a knob row.
        auto machineRow = [&] (juce::Label& title, juce::ToggleButton& on, juce::ToggleButton* extra,
                               std::initializer_list<std::pair<juce::Slider*, juce::Label*>> knobs)
        {
            auto block = area.removeFromTop (116);
            auto head  = block.removeFromTop (26);
            title.setBounds (head.removeFromLeft (180).withSizeKeepingCentre (180, 22));
            on.setBounds (head.removeFromLeft (70).withSizeKeepingCentre (66, 24));
            if (extra != nullptr)
            {
                head.removeFromLeft (gap);
                extra->setBounds (head.removeFromLeft (100).withSizeKeepingCentre (96, 24));
            }
            block.removeFromTop (4);
            const int kw = 96;
            auto krow = block.withSizeKeepingCentre (juce::jmin (block.getWidth(), kw * (int) knobs.size()),
                                                     block.getHeight());
            for (auto& kv : knobs)
                layoutDialCell (krow, *kv.second, *kv.first, kw);
            area.removeFromTop (gap);
        };

        machineRow (machSpectralTitle, machSpectralOn, nullptr,
                    { { &machSpectralMix, &machSpectralMixL }, { &machSpectralAmount, &machSpectralAmountL } });
        machineRow (machPitchTitle, machPitchOn, &machPitchFormant,
                    { { &machPitchMix, &machPitchMixL }, { &machPitchShift, &machPitchShiftL } });
        // Damage: Clip type + Advanced toggle in the header. Collapsed shows just
        // Drive + Mix; expanded reveals the full lo-fi knob row.
        {
            const bool more = machDamageMore.getToggleState();
            auto block = area.removeFromTop (116);
            auto head  = block.removeFromTop (26);
            machDamageTitle.setBounds (head.removeFromLeft (180).withSizeKeepingCentre (180, 22));
            machDamageOn.setBounds (head.removeFromLeft (70).withSizeKeepingCentre (66, 24));
            head.removeFromLeft (gap);
            machDamageClip.setBounds (head.removeFromLeft (110).withSizeKeepingCentre (106, 26));
            machDamageMore.setBounds (head.removeFromRight (96).withSizeKeepingCentre (92, 24));
            block.removeFromTop (4);
            const int kw = 96;
            std::vector<std::pair<juce::Slider*, juce::Label*>> dk { { &machDamageAmount, &machDamageAmountL } };
            if (more)
            {
                dk.push_back ({ &machDamageBits,    &machDamageBitsL });
                dk.push_back ({ &machDamageRate,    &machDamageRateL });
                dk.push_back ({ &machDamageJitter,  &machDamageJitterL });
                dk.push_back ({ &machDamageNoise,   &machDamageNoiseL });
                dk.push_back ({ &machDamageDropout, &machDamageDropoutL });
                dk.push_back ({ &machDamageTone,    &machDamageToneL });
            }
            dk.push_back ({ &machDamageMix, &machDamageMixL });
            auto krow = block.withSizeKeepingCentre (juce::jmin (block.getWidth(), kw * (int) dk.size()), block.getHeight());
            for (auto& kv : dk)
                layoutDialCell (krow, *kv.second, *kv.first, kw);
            area.removeFromTop (gap);
        }

        // Time Breaker: Sync + Division + Advanced in the header. Collapsed shows
        // just Chance + Mix; expanded reveals the stutter knobs and routing slots.
        {
            const bool more = machTimeMore.getToggleState();
            auto block = area.removeFromTop (more ? 172 : 116);
            auto head  = block.removeFromTop (26);
            machTimeTitle.setBounds (head.removeFromLeft (180).withSizeKeepingCentre (180, 22));
            machTimeOn.setBounds (head.removeFromLeft (70).withSizeKeepingCentre (66, 24));
            head.removeFromLeft (gap);
            machTimeSync.setBounds (head.removeFromLeft (92).withSizeKeepingCentre (88, 24));
            head.removeFromLeft (gap);
            machTimeDivision.setBounds (head.removeFromLeft (88).withSizeKeepingCentre (84, 26));
            machTimeMore.setBounds (head.removeFromRight (96).withSizeKeepingCentre (92, 24));

            auto knobRow = block.removeFromTop (96);
            knobRow.removeFromTop (4);
            const int kw = 96;
            std::vector<std::pair<juce::Slider*, juce::Label*>> tk;
            if (more)
                tk = { { &machTimeMix, &machTimeMixL },   { &machTimeRate, &machTimeRateL },
                       { &machTimeSize, &machTimeSizeL }, { &machTimeChance, &machTimeChanceL },
                       { &machTimeReverse, &machTimeReverseL } };
            else
                tk = { { &machTimeChance, &machTimeChanceL }, { &machTimeMix, &machTimeMixL } };
            auto krow = knobRow.withSizeKeepingCentre (juce::jmin (knobRow.getWidth(), kw * (int) tk.size()), knobRow.getHeight());
            for (auto& kv : tk)
                layoutDialCell (krow, *kv.second, *kv.first, kw);

            if (more)
            {
                // Routing slots: [Routes to]  [target 1][depth]   [target 2][depth]
                auto routeRow2 = block;
                machTimeRouteL.setBounds (routeRow2.removeFromLeft (88).withSizeKeepingCentre (84, 22));
                auto slot = [&] (juce::ComboBox& cb, juce::Slider& dk, juce::Label& dl)
                {
                    routeRow2.removeFromLeft (gap);
                    cb.setBounds (routeRow2.removeFromLeft (132).withSizeKeepingCentre (128, 28));
                    routeRow2.removeFromLeft (gap);
                    auto c = routeRow2.removeFromLeft (64);
                    dl.setBounds (c.removeFromTop (14));
                    dk.setBounds (c.reduced (2, 0));
                };
                slot (machTimeRoute1Target, machTimeRoute1Depth, machTimeRoute1DepthL);
                routeRow2.removeFromLeft (gap * 2);
                slot (machTimeRoute2Target, machTimeRoute2Depth, machTimeRoute2DepthL);
            }
            area.removeFromTop (gap);
        }
    }
    else if (currentTab == 4)   // HOME cockpit
    {
        homeTitle.setBounds (area.removeFromTop (30).reduced (4, 0));
        area.removeFromTop (gap);

        // Signal-flow strip: Texture -> Machines -> Space -> Master.
        homeFlowLabel.setBounds (area.removeFromTop (18).reduced (4, 0));
        auto flow = area.removeFromTop (40);
        const int stageW = 150;
        auto centred = flow.withSizeKeepingCentre (juce::jmin (flow.getWidth(), stageW * 4 + 90), flow.getHeight());
        auto stageCell = [&] (juce::Button& b) { b.setBounds (centred.removeFromLeft (stageW).withSizeKeepingCentre (stageW - 8, 30)); };
        stageCell (homeTextureOn);     centred.removeFromLeft (30);
        stageCell (homeMachinesLabel); centred.removeFromLeft (30);
        stageCell (homeSpaceOn);       centred.removeFromLeft (30);
        stageCell (homeMasterLabel);

        area.removeFromTop (gap * 2);
        // The macros that drive the whole chain.
        auto macroRow = area.removeFromTop (juce::jmin (150, area.getHeight() - 20));
        const int mW = juce::jmax (96, macroRow.getWidth() / kNumMacros);
        macroRow = macroRow.withSizeKeepingCentre (juce::jmin (macroRow.getWidth(), mW * kNumMacros), macroRow.getHeight());
        for (int i = 0; i < kNumMacros; ++i)
            layoutDialCell (macroRow, macroLabels[(size_t) i], macroKnobs[(size_t) i], mW);

        // Morph pad below the macros: the square pad + a row of corner-capture buttons.
        area.removeFromTop (gap * 2);
        morphPadLabel.setBounds (area.removeFromTop (18).withSizeKeepingCentre (200, 18));
        area.removeFromTop (4);
        auto padBlock = area.removeFromTop (juce::jmin (190, juce::jmax (120, area.getHeight())));
        auto pad = padBlock.withSizeKeepingCentre (200, juce::jmax (90, padBlock.getHeight() - 34));
        pad.setY (padBlock.getY());
        morphPad.setBounds (pad);
        auto capRow = juce::Rectangle<int> (pad.getX(), pad.getBottom() + 6, pad.getWidth(), 24);
        const int cw = (capRow.getWidth() - 18) / 4;
        morphCapA.setBounds (capRow.removeFromLeft (cw)); capRow.removeFromLeft (6);
        morphCapB.setBounds (capRow.removeFromLeft (cw)); capRow.removeFromLeft (6);
        morphCapC.setBounds (capRow.removeFromLeft (cw)); capRow.removeFromLeft (6);
        morphCapD.setBounds (capRow.removeFromLeft (cw));
    }

    if (currentTab == 0)
    {
        auto meterArea = bottom.removeFromRight (48).reduced (4);
        if (meter != nullptr) meter->setBounds (meterArea);

        auto curveArea = bottom.removeFromRight (128).reduced (4);
        if (satCurve != nullptr) satCurve->setBounds (curveArea);

        auto left  = bottom.removeFromLeft (bottom.getWidth() / 2).reduced (4);
        auto right = bottom.reduced (4);

        // Spectral freeze controls (moved here so Global Mod can sit by its scope).
        specLabel.setBounds (left.removeFromTop (18));
        {
            auto row = left;
            specFreezeButton.setBounds (row.removeFromLeft (96).withSizeKeepingCentre (96, 28));
            row.removeFromLeft (gap);
            specMix.setBounds     (row.removeFromLeft (68));
            row.removeFromLeft (gap);
            specShimmer.setBounds (row.removeFromLeft (68));
        }

        satOnButton.setBounds (right.removeFromTop (20).withWidth (150));
        {
            auto row = right;
            auto labeled = [&] (juce::Component& c, juce::Label& l, int w, bool combo)
            {
                auto cell = row.removeFromLeft (w);
                l.setBounds (cell.removeFromTop (14));
                c.setBounds (combo ? cell.withSizeKeepingCentre (w, 28) : cell);
                row.removeFromLeft (gap);
            };
            labeled (satType,  satTypeLabel,  98, true);
            labeled (satDrive, satDriveLabel, 82, false);
            labeled (satMix,   satMixLabel,   82, false);
        }

        // Global Mod controls, with their scope graph immediately to the right.
        auto gmArea = specRow.removeFromLeft (224).reduced (4);
        {
            auto gmTop = gmArea.removeFromTop (20);
            globalModOnButton.setBounds (gmTop.removeFromLeft (112));
            gmTop.removeFromLeft (gap);
            lfoSyncBox.setBounds (gmTop.withSizeKeepingCentre (gmTop.getWidth(), 18));
        }
        {
            auto row = gmArea;
            globalRate.setBounds  (row.removeFromLeft (80));
            row.removeFromLeft (gap);
            globalShape.setBounds (row.removeFromLeft (118).withSizeKeepingCentre (118, 28));
        }
        specRow.removeFromLeft (gap);
        auto scopeArea = specRow.removeFromLeft (320).reduced (4);
        if (modScope != nullptr) modScope->setBounds (scopeArea);
    }

}
