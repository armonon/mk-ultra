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
        { gf::ParamId::grainShape,  "grainShape",  "Shape" },
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
    constexpr int kHeaderH  = 44;
    // One knob scale for the whole page. Macros are the hero (larger); every
    // drawer knob is kKnob inside a kCell-wide cell; stage rows share one
    // grammar so knobs line up at the same x on every row.
    constexpr int kMacroCell = 124, kMacroKnob = 84;
    constexpr int kCell = 96,  kKnob = 62,  kRowH = 86;
    constexpr int kStageTitleW = 150, kStageOnW = 56, kStageExtraW = 150;
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

    for (auto* tab : { &tabEntropy, &tabMachines, &tabMix, &tabPrettifier, &tabAir })
    {
        tab->setComponentID ("chain");   // drawn as signal-chain tiles, not boxed buttons
        addAndMakeVisible (*tab);
    }
    tabMachines.setTooltip ("Machines: Spectral, Pitch/Formant, Damage, Time Breaker. Always in the chain; switch each machine inside.");
    tabMix.setTooltip ("Master: bus EQ, width, glue, ceiling, output, pitch lock. Always in the chain.");
    addChildComponent (tabHome);   // retired: HOME is no longer a tab (kept only so nothing dangles)
    tabEntropy.onClick = [this] { switchTab (0); };
    tabMix.onClick = [this] { switchTab (1); };
    tabPrettifier.onClick = [this] { switchTab (2); };
    tabMachines.onClick = [this] { switchTab (3); };
    tabAir.onClick = [this] { switchTab (5); };
    tabHome.setClickingTogglesState (true);
    tabEntropy.setClickingTogglesState (true);
    tabMix.setClickingTogglesState (true);
    tabPrettifier.setClickingTogglesState (true);
    tabMachines.setClickingTogglesState (true);
    tabAir.setClickingTogglesState (true);

    // Global Advanced toggle + the "..." menu that absorbs the old 3-row toolbar.
    advancedButton.setClickingTogglesState (true);
    advancedButton.setTooltip ("Reveal the deep controls: per-knob locks + modulation, mod matrix, ducker, routing, sends.");
    advancedButton.onClick = [this]
    {
        advancedMode = advancedButton.getToggleState();
        updateTabVisibility();
        resized();
        repaint();
    };
    addAndMakeVisible (advancedButton);
    moreButton.setTooltip ("More: init, randomize all, A/B copy, undo/redo, browse, share, delete, load IR, freeze");
    moreButton.onClick = [this] { showMoreMenu(); };
    addAndMakeVisible (moreButton);

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

    // Share: a single button that exports the current sound to a .mkultra file
    // (or imports one if cmd/ctrl-clicked).  Drop the file in Slack/Discord/etc
    // to trade sounds.
    addAndMakeVisible (shareButton);
    shareButton.setTooltip (juce::String ("Click: export the current sound to a .mkultra file.   ")
                            + juce::String (juce::CharPointer_UTF8 ("\xe2\x8c\x98")) + "/Ctrl-click: import one.");
    shareButton.onClick = [this]
    {
        const bool importMode = juce::ModifierKeys::getCurrentModifiers().isCommandDown();
        const auto musicDir = juce::File::getSpecialLocation (juce::File::userMusicDirectory);
        if (importMode)
        {
            shareFileChooser = std::make_unique<juce::FileChooser> (
                "Import MK-ULTRA preset", musicDir, "*.mkultra;*.preset");
            shareFileChooser->launchAsync (
                juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [this] (const juce::FileChooser& fc)
                {
                    const auto f = fc.getResult();
                    if (f.existsAsFile() && proc.presets.importPresetFromFile (f))
                        refreshPresetList();
                });
        }
        else
        {
            const auto stem = presetName.getText().isNotEmpty() ? presetName.getText() : juce::String ("MK Ultra Patch");
            shareFileChooser = std::make_unique<juce::FileChooser> (
                "Share MK-ULTRA preset", musicDir.getChildFile (stem + ".mkultra"), "*.mkultra");
            shareFileChooser->launchAsync (
                juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                [this] (const juce::FileChooser& fc)
                {
                    const auto f = fc.getResult();
                    if (f.getFullPathName().isNotEmpty())
                        proc.presets.exportPresetToFile (f);
                });
        }
    };

    // Delete preset: removes the currently-loaded user preset file (does nothing
    // for factory presets, which live in code). Confirms first.
    deletePresetButton.setTooltip ("Delete the currently-selected user preset (factory presets are protected).");
    addAndMakeVisible (deletePresetButton);
    deletePresetButton.onClick = [this]
    {
        const auto cur = proc.presets.getCurrentPresetName();
        // Don't delete factory presets -- they live in code, not on disk.
        const auto factoryNames = []
        {
            juce::StringArray a;
            for (auto& fp : gf::factoryPresets()) a.add (fp.name);
            return a;
        }();
        if (cur.isEmpty() || factoryNames.contains (cur))
            return;
        juce::AlertWindow::showOkCancelBox (
            juce::AlertWindow::QuestionIcon, "Delete preset?",
            "Permanently delete \"" + cur + "\"?", "Delete", "Cancel", this,
            juce::ModalCallbackFunction::create ([this, cur] (int r)
            {
                if (r == 1 && proc.presets.deletePreset (cur))
                    refreshPresetList();
            }));
    };

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
    setupSectionLabel (dnaHeader, "MODULES", 13.0f);
    setupSectionLabel (colorHeader, "COLOR", 13.0f);
    addAndMakeVisible (colorHeader);
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
        juce::ToggleButton* mods[10] = { &echoOnButton, &reverbOnButton, &chorusOnButton, &crushOnButton,
                                         &phaserOnButton, &flangerOnButton, &dreamOnButton, &angelOnButton, &harmonyOnButton,
                                         &convolutionOnButton };
        const char* modIds[10] = { "echoOn", "prettyReverbOn", "chorusOn", "crushOn", "phaserOn", "flangerOn", "dreamOn", "angelOn", "harmonyOn", "convolutionOn" };
        for (int i = 0; i < 10; ++i)
        {
            mods[i]->setTooltip ("Enable/disable this Beauty & Space module");
            addAndMakeVisible (*mods[i]);
            moduleAttach[(size_t) i] = std::make_unique<ButtonAttachment> (proc.apvts, modIds[i], *mods[i]);
        }
    }

    // Convolution IR loader: opens a native file chooser for .wav/.aif IRs.
    convolutionLoadButton.setTooltip ("Load any IR file (cathedrals, springs, weird spaces) for the Convolve machine");
    convolutionLoadButton.onClick = [this]
    {
        const juce::File start (proc.getConvolutionIRPath().isNotEmpty()
                                ? juce::File (proc.getConvolutionIRPath()).getParentDirectory()
                                : juce::File::getSpecialLocation (juce::File::userMusicDirectory));
        convolutionFileChooser = std::make_unique<juce::FileChooser> (
            "Load Impulse Response", start, "*.wav;*.aif;*.aiff;*.flac;*.ogg");
        convolutionFileChooser->launchAsync (
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                const auto f = fc.getResult();
                if (f.existsAsFile())
                {
                    proc.loadConvolutionIR (f);
                    convolutionIRLabel.setText (f.getFileNameWithoutExtension(), juce::dontSendNotification);
                }
            });
    };
    addAndMakeVisible (convolutionLoadButton);
    convolutionIRBox.addItemList ({ "Custom", "Hall", "Plate", "Room", "Cavern", "Spring" }, 1);
    convolutionIRBox.setTooltip ("Which space Convolve uses: five built-in synthesised spaces, or Custom for a file you loaded.");
    addAndMakeVisible (convolutionIRBox);
    convolutionIRAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "convolutionIR", convolutionIRBox);
    {
        const juce::File savedIR (proc.getConvolutionIRPath());
        if (savedIR.getFullPathName().isEmpty())
            convolutionIRLabel.setText ("", juce::dontSendNotification);
        else if (savedIR.existsAsFile())
            convolutionIRLabel.setText (savedIR.getFileNameWithoutExtension(), juce::dontSendNotification);
        else
            convolutionIRLabel.setText ("IR not found: " + savedIR.getFileName(), juce::dontSendNotification);
    }
    convolutionIRLabel.setJustificationType (juce::Justification::centredLeft);
    convolutionIRLabel.setColour (juce::Label::textColourId, gf::BiohazardLookAndFeel::textCol);
    addAndMakeVisible (convolutionIRLabel);

    for (auto* s : { &globalRate, &satDrive, &satMix, &specMix, &specShimmer })
        s->setPopupDisplayEnabled (true, false, this); // drag-only: hover popups get stuck as orphaned bubbles

    // MIDI play controls — make the shared piano roll actually drive the grains.
    // ---- Granular source: live input, or an audio file dropped on the plugin ----
    setupSectionLabel (grainSourceTitle, "SOURCE", 13.0f);
    addAndMakeVisible (grainSourceTitle);
    grainSourceBox.addItemList ({ "Live", "Sample" }, 1);
    grainSourceBox.setTooltip ("What the grain cloud chews on: the live input, or an audio file you load. "
                               "Drop a file anywhere on the plugin to load one.");
    addAndMakeVisible (grainSourceBox);
    grainSourceAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        proc.apvts, "grainSource", grainSourceBox);

    grainSampleLoad.setTooltip ("Load an audio file for the grain cloud to granulate "
                                "(or just drop one onto the plugin)");
    grainSampleLoad.onClick = [this]
    {
        const juce::File start (proc.getGranularSamplePath().isNotEmpty()
                                ? juce::File (proc.getGranularSamplePath()).getParentDirectory()
                                : juce::File::getSpecialLocation (juce::File::userMusicDirectory));
        sampleChooser = std::make_unique<juce::FileChooser> (
            "Load Sample", start, "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
        sampleChooser->launchAsync (
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                if (const auto f = fc.getResult(); f.existsAsFile())
                    loadSampleFile (f);
            });
    };
    addAndMakeVisible (grainSampleLoad);
    grainSampleClear.setTooltip ("Forget the loaded sample and go back to granulating the live input");
    grainSampleClear.onClick = [this]
    {
        proc.clearGranularSample();
        updateGrainSampleLabel();
    };
    addAndMakeVisible (grainSampleClear);
    grainSampleName.setJustificationType (juce::Justification::centredLeft);
    grainSampleName.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (grainSampleName);
    updateGrainSampleLabel();

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

    polyGrainButton.setTooltip ("Polyphonic Grains: hold a chord -- each grain picks a random held note for its pitch, so the granulator becomes a polyphonic cloud. Requires MIDI on.");
    addAndMakeVisible (polyGrainButton);
    polyGrainAttach = std::make_unique<ButtonAttachment> (proc.apvts, "polyGrain", polyGrainButton);
    mpeOnButton.setTooltip ("MPE: each MPE voice's pitch bend, channel pressure (Y) and CC74 timbre (Z) are routed per-grain. Pressure scales grain amplitude, timbre scales grain size. Implies Poly Grains.");
    addAndMakeVisible (mpeOnButton);
    mpeOnAttach = std::make_unique<ButtonAttachment> (proc.apvts, "mpeOn", mpeOnButton);
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

    // Multiband Damage controls.
    machToggle (machDamageSplit, "damageSplitOn", machDamageSplitAttach);
    machKnob (machDamageSplitHz,    machDamageSplitHzL,    "Split",    "damageSplitHz",    machDamageSplitHzAttach);
    machKnob (machDamageHighAmount, machDamageHighAmountL, "High Drive", "damageHighAmount", machDamageHighAmountAttach);

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

    // Sidechain Ducker: lets the original input breathe through the wet texture.
    setupSectionLabel (machDuckerTitle, "DUCKER", 13.0f);
    machToggle (machDuckerOn, "duckOn", machDuckerOnAttach);
    machKnob (machDuckerAmount,    machDuckerAmountL,    "Amount",    "duckAmount",    machDuckerAmountAttach);
    machKnob (machDuckerThreshold, machDuckerThresholdL, "Threshold", "duckThreshold", machDuckerThresholdAttach);
    machKnob (machDuckerAttack,    machDuckerAttackL,    "Attack",    "duckAttack",    machDuckerAttackAttach);
    machKnob (machDuckerRelease,   machDuckerReleaseL,   "Release",   "duckRelease",   machDuckerReleaseAttach);
    machDuckerSource.addItemList ({ "Input", "Sidechain" }, 1);
    addAndMakeVisible (machDuckerSource);
    machDuckerSourceAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        proc.apvts, "duckSource", machDuckerSource);
    machDuckerSource.setTooltip ("Key source: Input self-sidechains off what you feed the plugin; "
                                 "Sidechain reads the external key bus, so a kick on a send can duck the texture "
                                 "(falls back to Input when nothing is routed)");
    machDuckerOn.setTooltip ("Self-sidechain Ducker: the input envelope attenuates the WET (entropy + Beauty/Space) so the original sound breathes through the texture");
    machDuckerAmount.setTooltip ("Maximum gain reduction at full trigger (0 = no duck, 1 = full silence at peak)");
    machDuckerThreshold.setTooltip ("Trigger level below which no ducking happens (so quiet passages keep the full wet)");

    // ---- AIR tab ----
    setupSectionLabel (airHeader,       "AIR  -  HIGH-BAND TONAL TOYS", 14.0f);
    setupSectionLabel (airSquelchTitle, "SQUELCH",   13.0f);
    setupSectionLabel (airExciterTitle, "EXCITER",   13.0f);
    setupSectionLabel (airShelfTitle,   "DYN SHELF", 13.0f);
    setupSectionLabel (airPhaserTitle,  "PHASER",    13.0f);
    setupSectionLabel (airDelayTitle,   "DELAY",     13.0f);
    for (auto* l : { &airHeader, &airSquelchTitle, &airExciterTitle, &airShelfTitle, &airPhaserTitle, &airDelayTitle })
        addAndMakeVisible (*l);

    machToggle (airOn,        "airOn",        airOnAttach);
    machToggle (airSquelchOn, "airSquelchOn", airSquelchOnAttach);
    machToggle (airExciterOn, "airExciterOn", airExciterOnAttach);
    machToggle (airShelfOn,   "airShelfOn",   airShelfOnAttach);
    machToggle (airPhaserOn,  "airPhaserOn",  airPhaserOnAttach);
    machToggle (airDelayOn,   "airDelayOn",   airDelayOnAttach);

    airSquelchMode.addItemList ({ "Low", "Band", "High" }, 1);
    addAndMakeVisible (airSquelchMode);
    airSquelchModeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "airSquelchMode", airSquelchMode);

    machKnob (airCrossover,      airCrossoverL,      "Crossover", "airCrossover",      airCrossoverAttach);
    machKnob (airMix,            airMixL,            "Mix",       "airMix",            airMixAttach);
    machKnob (airSquelchHz,      airSquelchHzL,      "Cutoff",    "airSquelchHz",      airSquelchHzAttach);
    machKnob (airSquelchRes,     airSquelchResL,     "Resonance", "airSquelchRes",     airSquelchResAttach);
    machKnob (airSquelchEnv,     airSquelchEnvL,     "Env",       "airSquelchEnv",     airSquelchEnvAttach);
    machKnob (airExciterDrive,   airExciterDriveL,   "Drive",     "airExciterDrive",   airExciterDriveAttach);
    machKnob (airExciterMix,     airExciterMixL,     "Mix",       "airExciterMix",     airExciterMixAttach);
    machKnob (airShelfHz,        airShelfHzL,        "Freq",      "airShelfHz",        airShelfHzAttach);
    machKnob (airShelfAmount,    airShelfAmountL,    "Amount",    "airShelfAmount",    airShelfAmountAttach);
    machKnob (airShelfThreshold, airShelfThresholdL, "Threshold", "airShelfThreshold", airShelfThresholdAttach);
    machKnob (airPhaserRate,     airPhaserRateL,     "Rate",      "airPhaserRate",     airPhaserRateAttach);
    machKnob (airPhaserDepth,    airPhaserDepthL,    "Depth",     "airPhaserDepth",    airPhaserDepthAttach);
    machKnob (airPhaserMix,      airPhaserMixL,      "Mix",       "airPhaserMix",      airPhaserMixAttach);
    machKnob (airDelayMs,        airDelayMsL,        "Time",      "airDelayMs",        airDelayMsAttach);
    machKnob (airDelayFeedback,  airDelayFeedbackL,  "Feedback",  "airDelayFeedback",  airDelayFeedbackAttach);
    machKnob (airDelayMix,       airDelayMixL,       "Mix",       "airDelayMix",       airDelayMixAttach);

    airOn.setTooltip ("AIR: split at the crossover, run only the TOP band through the toys below, blend back. The low band always passes clean.");
    airCrossover.setTooltip ("Where the top band starts. Everything below stays untouched.");
    airMix.setTooltip ("Processed top vs dry top. 0 = bypass the toys, 1 = fully processed top.");
    airSquelchOn.setTooltip ("Resonant filter on the top. Crank Resonance for a pitched ring at the cutoff -- the most direct way to add a tone to a note.");
    airSquelchEnv.setTooltip ("Envelope -> cutoff (auto-wah). + opens the filter on hits, - closes it. Up to 4 octaves.");
    airExciterOn.setTooltip ("Oversampled saturator on the top band only: adds harmonics above the crossover without touching the lows.");
    airShelfOn.setTooltip ("A high shelf whose gain follows the top band's envelope. Negative = tame spiky highs (de-ess). Positive = lift air when there's energy.");
    airShelfAmount.setTooltip ("-1 tames, +1 lifts, up to 12 dB above the Threshold.");
    airPhaserOn.setTooltip ("Phaser confined to the top: moving notches without smearing the low end.");
    airDelayOn.setTooltip ("Short feedback delay on the top only. Tune the time to the note for a comb-tone.");

    // ---- Universal Modulation Matrix ----
    setupSectionLabel (modMatrixTitle, "MOD MATRIX", 13.0f);
    addAndMakeVisible (modMatrixTitle);
    // Take the item lists straight off the parameters, so the dropdowns can never
    // drift out of step with what the processor actually routes.
    auto choicesOf = [this] (const char* paramId) -> juce::StringArray
    {
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (paramId)))
            return c->choices;
        jassertfalse;
        return {};
    };
    const juce::StringArray mmSourceNames = choicesOf ("modSlot1Source");
    const juce::StringArray mmTargetNames = choicesOf ("modSlot1Target");
    for (int i = 0; i < 4; ++i)
    {
        modMatrixSource[(size_t) i].addItemList (mmSourceNames, 1);
        modMatrixTarget[(size_t) i].addItemList (mmTargetNames, 1);
        addAndMakeVisible (modMatrixSource[(size_t) i]);
        addAndMakeVisible (modMatrixTarget[(size_t) i]);
        addAndMakeVisible (modMatrixDepth[(size_t) i]);
        addAndMakeVisible (modMatrixActivity[(size_t) i]);
        modMatrixActivity[(size_t) i].setInterceptsMouseClicks (false, false);
        modMatrixArrow[(size_t) i].setText (juce::String (juce::CharPointer_UTF8 ("\xe2\x86\x92")), juce::dontSendNotification);
        modMatrixArrow[(size_t) i].setJustificationType (juce::Justification::centred);
        addAndMakeVisible (modMatrixArrow[(size_t) i]);
        modMatrixDepth[(size_t) i].setSliderStyle (juce::Slider::LinearHorizontal);
        modMatrixDepth[(size_t) i].setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        modMatrixDepth[(size_t) i].setRange (-1.0, 1.0);
        modMatrixDepth[(size_t) i].setTooltip ("Mod depth (signed): how much this source moves the target. Drag negative to invert.");

        const juce::String s ("modSlot" + juce::String (i + 1));
        modMatrixSourceAttach[(size_t) i] = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            proc.apvts, s + "Source", modMatrixSource[(size_t) i]);
        modMatrixTargetAttach[(size_t) i] = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            proc.apvts, s + "Target", modMatrixTarget[(size_t) i]);
        modMatrixDepthAttach[(size_t) i] = std::make_unique<SliderAttachment> (
            proc.apvts, s + "Depth", modMatrixDepth[(size_t) i]);
    }

    // ---- MOD SOURCES: controls for the matrix's own generators ----
    setupSectionLabel (modSourcesTitle, "MOD SOURCES", 13.0f);
    setupSectionLabel (stepSeqTitle,    "STEP SEQ",    13.0f);
    addAndMakeVisible (modSourcesTitle);
    addAndMakeVisible (stepSeqTitle);

    auto setupModKnob = [this] (juce::Slider& k, juce::Label& l, const juce::String& name,
                                const juce::String& paramId, std::unique_ptr<SliderAttachment>& attach,
                                const juce::String& tooltip)
    {
        k.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        k.setTooltip (tooltip);
        addAndMakeVisible (k);
        l.setText (name, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centred);
        l.setFont (juce::FontOptions (11.0f));
        addAndMakeVisible (l);
        attach = std::make_unique<SliderAttachment> (proc.apvts, paramId, k);
    };
    setupModKnob (lfo2Rate, lfo2RateL, "Rate", "lfo2Rate", lfo2RateAttach,
                  "LFO 2 speed in Hz. Ignored while Sync is set to a note division.");
    setupModKnob (modRandomRate, modRandomRateL, "Random", "modRandomRate", modRandomRateAttach,
                  "How often the Random source picks a new value, in times per second");
    setupModKnob (modCcNumber, modCcNumberL, "CC #", "modCcNumber", modCcNumberAttach,
                  "Which MIDI CC the \"MIDI CC\" source follows (Mod Wheel is 1, Expression is 11)");
    setupModKnob (stepSeqLength, stepSeqLengthL, "Steps", "stepSeqLength", stepSeqLengthAttach,
                  "How many steps play before the pattern loops");
    setupModKnob (stepSeqSmooth, stepSeqSmoothL, "Glide", "stepSeqSmooth", stepSeqSmoothAttach,
                  "Slide between steps instead of jumping -- 0 is a hard step, 1 is a slow ramp");

    lfo2Shape.addItemList (choicesOf ("lfo2Shape"), 1);
    lfo2Sync.addItemList  (choicesOf ("lfo2Sync"), 1);
    stepSeqDivision.addItemList (choicesOf ("stepSeqDivision"), 1);
    addAndMakeVisible (lfo2Shape);
    addAndMakeVisible (lfo2Sync);
    addAndMakeVisible (stepSeqDivision);
    lfo2Shape.setTooltip ("LFO 2 waveform");
    lfo2Sync.setTooltip ("Free runs at the Rate knob; a note division locks LFO 2 to the host tempo");
    stepSeqDivision.setTooltip ("How long each step lasts, as a note division of the host tempo");
    lfo2ShapeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        proc.apvts, "lfo2Shape", lfo2Shape);
    lfo2SyncAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        proc.apvts, "lfo2Sync", lfo2Sync);
    stepSeqDivisionAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        proc.apvts, "stepSeqDivision", stepSeqDivision);

    for (int i = 0; i < 16; ++i)
    {
        auto& st = stepSeqSteps[(size_t) i];
        st.setSliderStyle (juce::Slider::LinearVertical);
        st.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        st.setRange (-1.0, 1.0);
        st.setComponentID ("step");   // the LookAndFeel draws these as vertical bars
        st.setTooltip ("Step " + juce::String (i + 1) + ": how far this step pushes the target "
                       "(negative pulls the other way)");
        addAndMakeVisible (st);
        stepSeqStepAttach[(size_t) i] = std::make_unique<SliderAttachment> (
            proc.apvts, "stepSeq" + juce::String (i + 1), st);
    }

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

    addAndMakeVisible (grainViz);
    grainViz.setInterceptsMouseClicks (false, false);
    grainViz.setTooltip ("Live grain cloud -- each dot is an active grain (x = pan, y = amplitude, fading by age).");
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
    // These three toggles now live IN the chain strip as the stage on/off dots.
    homeTextureOn.setButtonText ({});
    homeSpaceOn.setButtonText ({});
    airOn.setButtonText ({});
    homeTextureOn.setTooltip ("Texture stage on/off");
    homeSpaceOn.setTooltip ("Space stage on/off");
    airOn.setTooltip ("Air stage on/off");
    for (auto* b : { &machSpectralOn, &machPitchOn, &machDamageOn, &machTimeOn, &machDuckerOn,
                     &airSquelchOn, &airExciterOn, &airShelfOn, &airPhaserOn, &airDelayOn })
        b->setButtonText ({});   // dot-only: the row title names the stage

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
    machDamageSplit.setTooltip ("Multiband: split the signal at Split Hz and drive the low + high bands independently");
    machDamageSplitHz.setTooltip ("Crossover frequency (60-12kHz); the band the High Drive knob hammers");
    machDamageHighAmount.setTooltip ("Drive for the HIGH band when Multiband is on (the existing Drive knob runs the low band)");
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

    // Resizable editor: keep the 1020x860 design ratio so the layout never
    // distorts, but let the user scale 70% .. 140% to fit their monitor / DAW
    // window. The last size is persisted across sessions via APVTS state.
    setResizable (true, true);
    if (auto* c = getConstrainer())
    {
        c->setFixedAspectRatio ((double) kDesignW / (double) kDesignH);
        c->setSizeLimits (714, 728, 1428, 1456);
    }
    const int savedW = (int) proc.apvts.state.getProperty ("editorWidth",  1020);
    const int savedH = (int) proc.apvts.state.getProperty ("editorHeight", 1040);
    setSize (juce::jlimit (714, 1428, savedW), juce::jlimit (602, 1204, savedH));
    updateTabVisibility();

    // Update pill: hidden until a newer GitHub release is found, then it appears
    // and links to the download. The .pkg / Windows installer overrides the old
    // version on install, so users just download + run -- no separate Hub needed.
    // Tour button: a ? pill that opens the help overlay. Also auto-launches the
    // tour on first run (until the user finishes or skips it).
    tourButton.setTooltip ("Open the MK-ULTRA quick tour");
    tourButton.onClick = [this] { launchTour(); };
    addAndMakeVisible (tourButton);

    addAndMakeVisible (tourOverlay);
    tourOverlay.setVisible (false);
    tourOverlay.setWantsKeyboardFocus (true);
    // User-scoped settings file (not APVTS state) so the tour flag persists
    // across DAW sessions instead of replaying on every new project.
    auto getUserSettings = []
    {
        juce::PropertiesFile::Options opts;
        opts.applicationName     = "MK-ULTRA";
        opts.filenameSuffix      = ".settings";
        opts.osxLibrarySubFolder = "Application Support";
        opts.folderName          = "MK-ULTRA";
        static juce::ApplicationProperties props;
        props.setStorageParameters (opts);
        return props.getUserSettings();
    };
    tourOverlay.onDismiss ([getUserSettings]
    {
        if (auto* s = getUserSettings()) { s->setValue ("hasSeenTour", true); s->saveIfNeeded(); }
    });

    if (auto* s = getUserSettings(); s == nullptr || ! s->getBoolValue ("hasSeenTour", false))
    {
        // Defer the auto-launch with a SafePointer so it can't dereference a
        // freed editor if the host destroys it before the async fires (pluginval
        // and some DAWs cycle the editor rapidly during scan / state restore).
        juce::Component::SafePointer<GrainFreezeEditor> safeThis (this);
        juce::Timer::callAfterDelay (50, [safeThis]
        {
            if (safeThis != nullptr && safeThis->isShowing())
                safeThis->launchTour();
        });
    }

    updateButton.setVisible (false);
    updateButton.setTooltip ("Click to open the GitHub release page and download the installer");
    addChildComponent (updateButton);
    updateButton.onClick = [this] { if (updateUrl.isNotEmpty()) juce::URL (updateUrl).launchInDefaultBrowser(); };
    updateChecker.start (JucePlugin_VersionString,
                         [this] (juce::String tag, juce::String url)
                         {
                             updateUrl = url;
                             updateButton.setButtonText ("Update " + tag);
                             updateButton.setVisible (true);
                             resized();
                         });

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

    // ---- Drawer viewport: everything that isn't header / macros / chain / play
    // surface / overlay / scope is reparented into drawerContent so the open
    // drawer can scroll. Done once, generically, so no addAndMakeVisible call
    // above had to change. Visibility flags survive reparenting.
    {
        std::vector<juce::Component*> stay {
            &prevButton, &nextButton, &presetBox, &presetName, &saveButton, &randomizeButton,
            &buttonA, &buttonB, &advancedButton, &moreButton, &tourButton, &panicButton, &updateButton,
            &tabEntropy, &tabMachines, &tabPrettifier, &tabAir, &tabMix,
            &homeTextureOn, &homeSpaceOn, &airOn,
            &morphPad, &morphPadLabel, &grainViz, &morphCapA, &morphCapB, &morphCapC, &morphCapD,
            &fadeOverlay, &tourOverlay, &drawerView };
        for (auto& k : macroKnobs)  stay.push_back (&k);
        for (auto& l : macroLabels) stay.push_back (&l);
        if (waveformDisplay != nullptr) stay.push_back (waveformDisplay.get());
        if (spectrumDisplay != nullptr) stay.push_back (spectrumDisplay.get());
        if (keyboard != nullptr)        stay.push_back (keyboard.get());

        addAndMakeVisible (drawerView);
        drawerView.setViewedComponent (&drawerContent, false);
        drawerView.setScrollBarsShown (true, false);
        drawerView.setScrollBarThickness (8);

        juce::Array<juce::Component*> kids;
        for (int i = 0; i < getNumChildComponents(); ++i) kids.add (getChildComponent (i));
        for (auto* c : kids)
            if (std::find (stay.begin(), stay.end(), c) == stay.end())
                drawerContent.addChildComponent (*c);   // moves it; keeps its visible flag
        fadeOverlay.toFront (false);
        tourOverlay.toFront (false);
    }

    // ---- Scale root: move every child (including the drawer viewport and the
    // overlays) into `root`, which is laid out at kDesignW x kDesignH and scaled
    // to the editor. Children keep their visibility and z-order.
    {
        juce::Array<juce::Component*> kids;
        for (int i = 0; i < getNumChildComponents(); ++i) kids.add (getChildComponent (i));
        addAndMakeVisible (root);
        for (auto* c : kids)
            if (c != &root)
                root.addChildComponent (*c);
        fadeOverlay.toFront (false);
        tourOverlay.toFront (false);
    }
    // Everything is parented now; lay out once more so the drawer content size
    // and the scale transform reflect the final tree.
    resized();

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

    // Mod-matrix activity LEDs: read each slot's current source value x depth,
    // light the LED, then nudge the value toward 0 so it decays smoothly.
    if (currentTab == 3)   // only when MACHINES is showing
    {
        const std::array<const char*, 4> ids { "modSlot1", "modSlot2", "modSlot3", "modSlot4" };
        for (int i = 0; i < 4; ++i)
        {
            const auto srcIdx = (int) proc.apvts.getRawParameterValue (juce::String (ids[(size_t) i]) + "Source")->load();
            const auto tgtIdx = (int) proc.apvts.getRawParameterValue (juce::String (ids[(size_t) i]) + "Target")->load();
            const auto depth  = proc.apvts.getRawParameterValue (juce::String (ids[(size_t) i]) + "Depth")->load();
            // The processor publishes each slot's live source value, so the LED is
            // right for every source -- including the ones only it can see (LFO 2,
            // the step sequencer, the random S&H, MIDI).
            const float level = (srcIdx > 0 && tgtIdx > 0 && std::abs (depth) > 0.001f)
                                    ? juce::jlimit (0.0f, 1.0f, std::abs (proc.getModSlotSourceValue (i) * depth))
                                    : 0.0f;
            auto& led = modMatrixActivity[(size_t) i];
            led.level = juce::jmax (level, led.level * 0.85f);   // smooth decay
            if (led.level > 0.005f) led.repaint();
        }

        // Light whichever step the sequencer is playing.
        if (const int lit = proc.getModStepIndex(); lit != lastLitStep)
        {
            for (int i = 0; i < (int) stepSeqSteps.size(); ++i)
            {
                stepSeqSteps[(size_t) i].getProperties().set ("lit", i == lit);
                stepSeqSteps[(size_t) i].repaint();
            }
            lastLitStep = lit;
        }
    }

    // Grain cloud viz: only refresh when HOME is showing AND audio is active.
    if (currentTab == -1 && animating)
        grainViz.refresh();

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
    k.lock.setTooltip (juce::String::fromUTF8 ("Lock this control \xe2\x80\x94 keeps it fixed when you Randomize"));
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

void GrainFreezeEditor::launchTour()
{
    // Guard: hosts can invoke the editor's button onClick handlers before the
    // window is actually displayed (e.g. during scan/validation), in which case
    // grabbing keyboard focus or laying out against an empty bounds will crash.
    if (getLocalBounds().isEmpty())
        return;

    // Five stops walk through the experience: HOME tab -> macros -> Morph Pad
    // -> Browse (presets) -> MACHINES tab. Each step retargets a visible
    // component, switching tabs along the way so the target is in view.
    currentTab = -1;   // play surface
    updateTabVisibility();
    resized();
    std::vector<gf::TourOverlay::Step> steps;
    if (! macroKnobs.empty())
        steps.push_back ({ macroKnobs.front().getBounds().getUnion (macroKnobs.back().getBounds()),
                           "Start here. These seven macros drive the whole chain -- Texture, Beauty, Space, "
                           "Chaos, Motion, Damage, Emotion. Most sounds are finished right here." });
    steps.push_back ({ tabEntropy.getBounds().getUnion (tabMix.getBounds()),
                       "This is the signal chain, in order: Texture -> Machines -> Space -> Air -> Master. "
                       "The dots switch a stage on or off. Click a name to open that stage's controls; "
                       "click it again to close." });
    steps.push_back ({ morphPad.getBounds(),
                       "The Morph Pad blends FOUR captured snapshots. Capture A/B/C/D with the buttons "
                       "below, then drag the puck to move between them in real time." });
    steps.push_back ({ presetBox.getBounds().getUnion (saveButton.getBounds()),
                       "Presets live here. Pick a vibe and nudge from there. Type a name and Save to keep "
                       "your own; the ... menu has Share, Import, Browse and more." });
    steps.push_back ({ advancedButton.getBounds(),
                       "Advanced reveals the deep layer everywhere: per-knob locks and modulation, the "
                       "mod matrix, the ducker, sends and routing. Off by default so the page stays calm." });
    tourOverlay.setSteps (std::move (steps));
    tourOverlay.setBounds (root.getLocalBounds());
    tourOverlay.toFront (false);
    tourOverlay.setVisible (true);
    tourOverlay.grabKeyboardFocus();
}

void GrainFreezeEditor::switchTab (int tabIndex)
{
    // Clicking the open drawer's tile closes it (back to the play surface).
    currentTab = (tabIndex == currentTab) ? -1 : juce::jlimit (-1, 5, tabIndex);
    updateTabVisibility();
    resized();
    repaint(); // instant switch (no cross-fade) for snappiness
}

void GrainFreezeEditor::showMoreMenu()
{
    juce::PopupMenu m;
    m.addItem (1,  "Init patch");
    m.addItem (2,  "Randomize All");
    m.addSeparator();
    m.addItem (3,  "Copy A " + juce::String (juce::CharPointer_UTF8 ("\xe2\x86\x92")) + " B");
    m.addItem (4,  "Copy B " + juce::String (juce::CharPointer_UTF8 ("\xe2\x86\x92")) + " A");
    m.addItem (5,  "Reset B");
    m.addSeparator();
    m.addItem (6,  "Undo");
    m.addItem (7,  "Redo");
    m.addSeparator();
    m.addItem (8,  "Browse presets...");
    m.addItem (9,  "Share preset (.mkultra)...");
    m.addItem (10, "Delete preset");
    m.addItem (11, "Load convolution IR...");
    m.addSeparator();
    m.addItem (12, "Freeze grains", true, freezeButton.getToggleState());

    juce::Component::SafePointer<GrainFreezeEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&moreButton),
                     [safe] (int r)
    {
        if (safe == nullptr) return;
        auto& s = *safe;
        juce::Button* b = nullptr;
        switch (r)
        {
            case 1:  b = &s.initButton;            break;
            case 2:  b = &s.randomizeAllButton;    break;
            case 3:  b = &s.copyAToBButton;        break;
            case 4:  b = &s.copyBToAButton;        break;
            case 5:  b = &s.resetBButton;          break;
            case 6:  b = &s.undoButton;            break;
            case 7:  b = &s.redoButton;            break;
            case 8:  b = &s.browseButton;          break;
            case 9:  b = &s.shareButton;           break;
            case 10: b = &s.deletePresetButton;    break;
            case 11: b = &s.convolutionLoadButton; break;
            case 12: b = &s.freezeButton;          break;
            default: return;
        }
        b->triggerClick();
    });
}

void GrainFreezeEditor::layoutDialCell (juce::Rectangle<int>& row, juce::Label& label,
                                        juce::Slider& slider, int width, int knobD)
{
    auto cell = row.removeFromLeft (width).reduced (6, 4);
    label.setBounds (cell.removeFromTop (18));
    if (knobD > 0)
    {
        const int d = juce::jmin (knobD, cell.getWidth(), cell.getHeight());
        slider.setBounds (cell.withSizeKeepingCentre (d, d));
    }
    else
        slider.setBounds (cell);
}

void GrainFreezeEditor::updateTabVisibility()
{
    const bool entropyTab = currentTab == 0;
    const bool mixTab = currentTab == 1;
    const bool prettifierTab = currentTab == 2;
    const bool machinesTab = currentTab == 3;
    const bool playSurface = currentTab == -1;
    const bool homeTab = false;   // retired: macros are always visible, play surface replaces the rest
    constexpr bool inputToolsVisible = kMkUltraExperimentalInputTools;

    tabHome.setVisible (false);
    tabEntropy.setToggleState (entropyTab, juce::dontSendNotification);
    tabMix.setToggleState (mixTab, juce::dontSendNotification);
    tabPrettifier.setToggleState (prettifierTab, juce::dontSendNotification);
    tabMachines.setToggleState (machinesTab, juce::dontSendNotification);
    const bool airTab = currentTab == 5;
    tabAir.setToggleState (airTab, juce::dontSendNotification);
    for (auto* l : { &airSquelchTitle, &airExciterTitle, &airShelfTitle, &airPhaserTitle, &airDelayTitle })
        l->setVisible (airTab);
    airHeader.setText ("CROSSOVER", juce::dontSendNotification);   // reused as the first AIR row's title
    airHeader.setVisible (airTab);
    for (auto* b : { &airSquelchOn, &airExciterOn, &airShelfOn, &airPhaserOn, &airDelayOn })
        b->setVisible (airTab);
    airOn.setVisible (true);   // lives in the chain strip
    airSquelchMode.setVisible (airTab);
    for (auto* s : { &airCrossover, &airMix, &airSquelchHz, &airSquelchRes, &airSquelchEnv,
                     &airExciterDrive, &airExciterMix, &airShelfHz, &airShelfAmount, &airShelfThreshold,
                     &airPhaserRate, &airPhaserDepth, &airPhaserMix, &airDelayMs, &airDelayFeedback, &airDelayMix })
        s->setVisible (airTab);
    for (auto* l : { &airCrossoverL, &airMixL, &airSquelchHzL, &airSquelchResL, &airSquelchEnvL,
                     &airExciterDriveL, &airExciterMixL, &airShelfHzL, &airShelfAmountL, &airShelfThresholdL,
                     &airPhaserRateL, &airPhaserDepthL, &airPhaserMixL, &airDelayMsL, &airDelayFeedbackL, &airDelayMixL })
        l->setVisible (airTab);

    lnf.setAccentTheme (gf::BiohazardLookAndFeel::AccentTheme::entropy);   // one accent, everywhere
    sendLookAndFeelChange();

    // Always-on layer: macros + chain strip. Play surface only when no drawer is open.
    homeTitle.setVisible (false);
    homeFlowLabel.setVisible (false);
    homeMachinesLabel.setVisible (false);
    homeMasterLabel.setVisible (false);
    for (auto& s : macroKnobs)  s.setVisible (true);
    for (auto& l : macroLabels) l.setVisible (true);
    homeTextureOn.setVisible (true);
    homeSpaceOn.setVisible (true);
    morphPad.setVisible (playSurface);
    morphPadLabel.setVisible (playSurface);
    grainViz.setVisible (playSurface);
    for (auto* b : { &morphCapA, &morphCapB, &morphCapC, &morphCapD })
        b->setVisible (playSurface);

    // Old 3-row toolbar: these now live in the "..." menu (or the Texture drawer).
    for (auto* b : { &copyAToBButton, &copyBToAButton, &resetBButton, &undoButton, &redoButton,
                     &initButton, &randomizeAllButton, &browseButton, &shareButton, &deletePresetButton,
                     &buttonA, &buttonB })   // A/B recall == the morph pad's A/B corners; one system, not two
        b->setVisible (false);

    freezeButton.setVisible (entropyTab);
    const bool adv = advancedMode;
    for (auto& k : knobs)
    {
        k.slider.setVisible (entropyTab);
        k.label.setVisible (entropyTab);
        k.lock.setVisible (entropyTab && adv);
        k.modButton.setVisible (entropyTab && adv);
        if (k.ring != nullptr) k.ring->setVisible (entropyTab && adv);
    }
    globalModOnButton.setVisible (entropyTab && adv);
    globalRate.setVisible (entropyTab && adv);
    globalShape.setVisible (entropyTab && adv);
    lfoSyncBox.setVisible (entropyTab && adv);
    satOnButton.setVisible (entropyTab);
    satType.setVisible (entropyTab);
    satDrive.setVisible (entropyTab);
    satMix.setVisible (entropyTab);
    satTypeLabel.setVisible (entropyTab);
    satDriveLabel.setVisible (entropyTab);
    satMixLabel.setVisible (entropyTab);
    if (satCurve != nullptr) satCurve->setVisible (entropyTab);
    if (meter != nullptr) meter->setVisible (entropyTab);
    if (modScope != nullptr) modScope->setVisible (entropyTab && adv);
    // The Mix tab swaps the bottom output scope for the master spectrum analyzer.
    if (waveformDisplay != nullptr) waveformDisplay->setVisible (! mixTab && ! machinesTab);
    if (spectrumDisplay != nullptr) spectrumDisplay->setVisible (mixTab);
    // Legacy spectral freeze retired from the UI — the Spectral machine (Machines
    // tab) is the single home for spectral now.
    specFreezeButton.setVisible (false);
    specLabel.setVisible (false);
    specMix.setVisible (false);
    specShimmer.setVisible (false);

    // MIDI on-ramp: always on the Texture drawer (it used to hide behind the
    // experimental-input-tools flag, which also made acceptsMidi() false).
    grainSourceTitle.setVisible (entropyTab);
    grainSourceBox.setVisible (entropyTab);
    grainSampleLoad.setVisible (entropyTab);
    grainSampleClear.setVisible (entropyTab);
    grainSampleName.setVisible (entropyTab);
    midiEnableButton.setVisible (entropyTab);
    for (auto* c : { &midiRootSlider, &midiGlideSlider, &midiVelAmpSlider })
        c->setVisible (entropyTab);
    for (auto* l : { &midiRootLabel, &midiGlideLabel, &midiVelAmpLabel })
        l->setVisible (entropyTab);
    polyGrainButton.setVisible (entropyTab);
    mpeOnButton.setVisible (entropyTab);

    for (auto* c : { &dryLevel, &entropySend, &entropyReturn, &prettifierSend, &prettifierReturn, &mixOutput, &chaosBeauty, &mixWidth, &mixGlue, &mixCeiling })
        c->setVisible (mixTab);
    routingMode.setVisible (mixTab && adv);
    routingLabel.setVisible (mixTab && adv);
    for (auto& l : mixLabels) l.setVisible (mixTab);
    for (int i = 1; i <= 4; ++i) mixLabels[(size_t) i].setVisible (mixTab && adv);   // send/return labels
    for (auto* c : { &entropySend, &entropyReturn, &prettifierSend, &prettifierReturn })
        c->setVisible (mixTab && adv);
    eqHeader.setVisible (mixTab);
    for (auto& l : eqLabels) l.setVisible (mixTab);
    for (auto* c : { &eqLowKnob, &eqMidKnob, &eqHighKnob, &eqLoFiKnob })
        c->setVisible (mixTab);
    for (auto* c : { &pluginOnButton, &limiterOnButton, &mixEqOnButton,
                     &pitchMatchOnButton, &tempoLockOnButton })
        c->setVisible (mixTab);
    entropyOnButton.setVisible (false);   // chain strip owns stage on/off now

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

    prettifierOnButton.setVisible (false);   // chain strip owns stage on/off now
    for (auto& k : prettyKnobs)
    {
        k.slider.setVisible (prettifierTab);
        k.label.setVisible (prettifierTab);
        k.lock.setVisible (prettifierTab && adv);
        k.modButton.setVisible (prettifierTab && adv);
        if (k.ring != nullptr) k.ring->setVisible (prettifierTab && adv);
    }
    prettyOutputKnob.setVisible (prettifierTab);
    prettyOutputLabel.setVisible (prettifierTab);

    dnaHeader.setVisible (prettifierTab);
    colorHeader.setVisible (prettifierTab);
    prettifierHeader.setVisible (false);   // the chain tile is the drawer title now
    machinesHeader.setVisible (false);
    mixHeader.setVisible (false);
    for (auto& s : dnaKnobs)  s.setVisible (prettifierTab);
    for (auto& l : dnaLabels) l.setVisible (prettifierTab);
    for (auto* b : { &echoOnButton, &reverbOnButton, &chorusOnButton, &crushOnButton, &phaserOnButton,
                     &flangerOnButton, &dreamOnButton, &angelOnButton, &harmonyOnButton, &convolutionOnButton })
        b->setVisible (prettifierTab);
    convolutionLoadButton.setVisible (prettifierTab);
    convolutionIRBox.setVisible (prettifierTab);
    convolutionIRLabel.setVisible (prettifierTab);
    echoSyncBox.setVisible (prettifierTab);

    // MACHINES tab. Damage and Time Breaker collapse to essentials; their deep
    // params appear only when the per-machine "Advanced" toggle is on.
    const bool damageMore = adv;   // global Advanced replaces the per-machine expanders
    const bool timeMore   = adv;
    for (auto* l : { &machSpectralTitle, &machPitchTitle, &machDamageTitle, &machTimeTitle })
        l->setVisible (machinesTab);
    for (auto* b : { &machSpectralOn, &machPitchOn, &machPitchFormant, &machDamageOn, &machTimeOn, &machTimeSync })
        b->setVisible (machinesTab);
    // Ducker + mod matrix + poly/MPE: deep layer.
    machDuckerTitle.setVisible (machinesTab && adv);
    machDuckerOn.setVisible (machinesTab && adv);
    machDuckerSource.setVisible (machinesTab && adv);
    for (auto* s : { &machDuckerAmount, &machDuckerThreshold, &machDuckerAttack, &machDuckerRelease })
        s->setVisible (machinesTab && adv);
    for (auto* l : { &machDuckerAmountL, &machDuckerThresholdL, &machDuckerAttackL, &machDuckerReleaseL })
        l->setVisible (machinesTab && adv);
    modMatrixTitle.setVisible (machinesTab && adv);
    modSourcesTitle.setVisible (machinesTab && adv);
    stepSeqTitle.setVisible (machinesTab && adv);
    for (auto* c : { &lfo2Shape, &lfo2Sync, &stepSeqDivision })  c->setVisible (machinesTab && adv);
    for (auto* k : { &lfo2Rate, &modRandomRate, &modCcNumber, &stepSeqLength, &stepSeqSmooth })
        k->setVisible (machinesTab && adv);
    for (auto* l : { &lfo2RateL, &modRandomRateL, &modCcNumberL, &stepSeqLengthL, &stepSeqSmoothL })
        l->setVisible (machinesTab && adv);
    for (auto& st : stepSeqSteps)     st.setVisible (machinesTab && adv);
    for (auto& c : modMatrixSource)   c.setVisible (machinesTab && adv);
    for (auto& c : modMatrixTarget)   c.setVisible (machinesTab && adv);
    for (auto& s : modMatrixDepth)    s.setVisible (machinesTab && adv);
    for (auto& l : modMatrixArrow)    l.setVisible (machinesTab && adv);
    for (auto& a : modMatrixActivity) a.setVisible (machinesTab && adv);
    for (auto* b : { &machDamageMore, &machTimeMore })
        b->setVisible (false);
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
    // Multiband controls (also in the Advanced section).
    machDamageSplit.setVisible (machinesTab && damageMore);
    machDamageSplitHz.setVisible (machinesTab && damageMore);
    machDamageHighAmount.setVisible (machinesTab && damageMore);
    machDamageSplitHzL.setVisible (machinesTab && damageMore);
    machDamageHighAmountL.setVisible (machinesTab && damageMore);
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

bool GrainFreezeEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;ogg;mp3"))
            return true;
    return false;
}

void GrainFreezeEditor::fileDragEnter (const juce::StringArray& files, int, int)
{
    if (isInterestedInFileDrag (files))
    {
        fileDragActive = true;
        repaint();
    }
}

void GrainFreezeEditor::fileDragExit (const juce::StringArray&)
{
    fileDragActive = false;
    repaint();
}

void GrainFreezeEditor::filesDropped (const juce::StringArray& files, int, int)
{
    fileDragActive = false;
    repaint();
    for (const auto& f : files)
    {
        const juce::File file (f);
        if (file.hasFileExtension ("wav;aif;aiff;flac;ogg;mp3"))
        {
            loadSampleFile (file);
            return;   // one sample at a time
        }
    }
}

void GrainFreezeEditor::loadSampleFile (const juce::File& file)
{
    if (! proc.loadGranularSample (file))
    {
        grainSampleName.setText ("Could not read " + file.getFileName(), juce::dontSendNotification);
        return;
    }
    updateGrainSampleLabel();
}

void GrainFreezeEditor::updateGrainSampleLabel()
{
    const juce::File f (proc.getGranularSamplePath());
    if (f.getFullPathName().isEmpty())
    {
        grainSampleName.setText ("Drop an audio file anywhere to granulate it",
                                 juce::dontSendNotification);
        return;
    }
    if (! proc.hasGranularSample())
    {
        grainSampleName.setText ("Sample not found: " + f.getFileName(), juce::dontSendNotification);
        return;
    }
    grainSampleName.setText (f.getFileName() + "  ("
                                 + juce::String (proc.getGranularSampleSeconds(), 1) + "s)",
                             juce::dontSendNotification);
}

void GrainFreezeEditor::paint (juce::Graphics& g)
{
    using LF = gf::BiohazardLookAndFeel;

    // Fully static, lightweight background: no boot fade, no glow breathe, no
    // flicker, no drifting bloom, no spores. paint() only runs on real repaints.
    const float pulse = 0.12f;
    const float boot = 1.0f;   // no boot fade now; logo halo still scales by this
    // Paint in the same logical space the children are laid out in.
    g.addTransform (juce::AffineTransform::scale ((float) getWidth() / (float) kDesignW));
    const auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) kDesignW, (float) kDesignH);
    const int tab = 0;   // one visual theme for the whole page (was per-tab tints)

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
        // Premium-minimal: a consistent deep vignette with only the faintest tab
        // tint, rather than strong warm/cool per-tab washes.
        const juce::Colour topCol = pretty   ? juce::Colour (0xff14161b)
                                  : tab == 1 ? juce::Colour (0xff16151a)
                                             : juce::Colour (0xff121512);
        const juce::Colour midCol = LF::bg;
        const juce::Colour botCol = LF::bg.darker (0.4f);
        juce::ColourGradient base (topCol, bounds.getCentreX(), 0.0f,
                                   botCol, bounds.getCentreX(), bounds.getHeight(), false);
        base.addColour (0.45, midCol);
        g.setGradientFill (base);
        g.fillRect (bounds);
    }

    // Background artwork (static, subtle).
    if (tab == 0 || tab == 3 || tab == 4)
        drawBgImage (bgImage, 0.07f);
    else if (tab == 2)
        drawBgImage (prettifierBgImage, 0.11f);
    else
        drawBgImage (mixBgImage, 0.11f);

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
            // Emblem sits centred on the header row and may overhang it a little.
            const auto band = juce::Rectangle<int> (0, 0, kDesignW, kDesignH).reduced (20).removeFromTop (kHeaderH);
            auto logoBox = juce::Rectangle<float> (56.0f, 56.0f).withCentre ({ (float) band.getX() + kLogoSlot * 0.5f,
                                                                               (float) band.getCentreY() });

            // Soft halo behind the emblem: Entropy = large breathing green, Mix =
            // large breathing orange (both clearly lit), other tabs keep accent.
            const bool tinted = entropyTab || mixTab;
            const juce::Colour glowCol = entropyTab ? LF::toxic : mixTab ? orange : acc;
            const float breathe   = 1.0f + 0.05f * std::sin (animPhase * 1.7f);
            const float haloScale = tinted ? 0.42f * breathe : 0.35f;
            const float haloAlpha = (entropyTab ? 0.34f : mixTab ? 0.18f : 0.18f) + glowLevel * 0.05f;
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

    // Signal-chain strip: a connecting line behind the tiles, and a hollow ring
    // in the on/off slot of the two stages that have no switch (Machines,
    // Master) so the row keeps its rhythm.
    {
        const auto first = tabEntropy.getBounds(), last = tabMix.getBounds();
        if (! first.isEmpty() && ! last.isEmpty())
        {
            const float y = (float) first.getCentreY();
            g.setColour (LF::textCol.withAlpha (0.14f));
            g.drawLine ((float) homeTextureOn.getX(), y, (float) last.getRight(), y, 2.0f);
            auto ring = [&] (const juce::Rectangle<int>& tile)
            {
                const float cx = (float) tile.getX() - 17.0f;
                g.setColour (LF::textCol.withAlpha (0.35f));
                g.drawEllipse (cx - 5.0f, y - 5.0f, 10.0f, 10.0f, 1.5f);
            };
            ring (tabMachines.getBounds());
            ring (tabMix.getBounds());
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

    // Dragging an audio file over the plugin: say what dropping it will do.
    if (fileDragActive)
    {
        g.setColour (acc.withAlpha (0.10f));
        g.fillRect (bounds);
        g.setColour (acc);
        g.drawRoundedRectangle (bounds.reduced (6.0f), 10.0f, 2.5f);
        auto strip = juce::Rectangle<float> (bounds.getCentreX() - 220.0f, bounds.getCentreY() - 30.0f,
                                             440.0f, 60.0f);
        g.setColour (LF::bg.withAlpha (0.88f));
        g.fillRoundedRectangle (strip, 8.0f);
        g.setColour (acc);
        g.drawRoundedRectangle (strip, 8.0f, 1.5f);
        g.setFont (juce::FontOptions (18.0f));
        g.drawText ("Drop to granulate this file", strip, juce::Justification::centred);
    }
}

void GrainFreezeEditor::resized()
{
    constexpr int pad = 20;
    constexpr int gap = 10;
    constexpr bool inputToolsVisible = kMkUltraExperimentalInputTools;

    // Persist the editor size so it survives session reload.
    proc.apvts.state.setProperty ("editorWidth",  getWidth(),  nullptr);
    proc.apvts.state.setProperty ("editorHeight", getHeight(), nullptr);

    // Lay out at the design size; scale the root to fit.
    const float uiScale = (float) getWidth() / (float) kDesignW;
    root.setTransform (juce::AffineTransform::scale (uiScale));
    root.setBounds (0, 0, kDesignW, kDesignH);
    const auto design = juce::Rectangle<int> (0, 0, kDesignW, kDesignH);
    fadeOverlay.setBounds (design);
    tourOverlay.setBounds (design);
    tourOverlay.toFront (false);
    auto area = design.reduced (pad);

    // ---- Zone 1: header. Logo slot (painted) + ONE control row. ----
    auto header = area.removeFromTop (kHeaderH);
    header.removeFromLeft (kLogoSlot + 16);
    auto hrow = header.withSizeKeepingCentre (header.getWidth(), 30);
    prevButton.setBounds (hrow.removeFromLeft (30));   hrow.removeFromLeft (2);
    nextButton.setBounds (hrow.removeFromLeft (30));   hrow.removeFromLeft (gap);
    presetBox.setBounds  (hrow.removeFromLeft (220));  hrow.removeFromLeft (gap);
    // Right cluster, outermost first.
    panicButton.setBounds    (hrow.removeFromRight (64));  hrow.removeFromRight (6);
    tourButton.setBounds     (hrow.removeFromRight (28));  hrow.removeFromRight (6);
    moreButton.setBounds     (hrow.removeFromRight (34));  hrow.removeFromRight (gap);
    advancedButton.setBounds (hrow.removeFromRight (88));  hrow.removeFromRight (gap);
    randomizeButton.setBounds (hrow.removeFromRight (96)); hrow.removeFromRight (gap);
    if (updateButton.isVisible())
    {
        updateButton.setBounds (hrow.removeFromRight (110));
        hrow.removeFromRight (gap);
    }
    saveButton.setBounds (hrow.removeFromRight (64));      hrow.removeFromRight (6);
    presetName.setBounds (hrow);                           // whatever width is left
    area.removeFromTop (gap);

    // ---- Zone 2: macros. Always visible. ----
    {
        auto macroRow = area.removeFromTop (kMacroCell);
        macroRow = macroRow.withSizeKeepingCentre (juce::jmin (macroRow.getWidth(), kMacroCell * kNumMacros), macroRow.getHeight());
        for (int i = 0; i < kNumMacros; ++i)
            layoutDialCell (macroRow, macroLabels[(size_t) i], macroKnobs[(size_t) i], kMacroCell, kMacroKnob);
    }
    area.removeFromTop (gap);

    // ---- Zone 3: signal-chain strip. Always visible. Click a tile to open its drawer. ----
    {
        struct Tile { juce::Button* on; juce::TextButton* name; int w; };
        const Tile tiles[] = {
            { &homeTextureOn, &tabEntropy,    124 },
            { nullptr,        &tabMachines,   124 },
            { &homeSpaceOn,   &tabPrettifier, 108 },
            { &airOn,         &tabAir,         88 },
            { nullptr,        &tabMix,        108 },
        };
        int total = 0;
        for (auto& t : tiles) total += t.w + (t.on != nullptr ? 30 : 0);
        total += 4 * 16;
        auto chain = area.removeFromTop (40);
        auto row = chain.withSizeKeepingCentre (juce::jmin (chain.getWidth(), total), 32);
        for (auto& t : tiles)
        {
            if (t.on != nullptr)
                t.on->setBounds (row.removeFromLeft (30).withSizeKeepingCentre (26, 26));
            t.name->setBounds (row.removeFromLeft (t.w));
            row.removeFromLeft (16);
        }
    }
    area.removeFromTop (gap + 2);

    // ---- Zone 4: content. Play surface when no drawer is open, else the drawer. ----

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
    // Texture-only support rows (Warmth strip; Global Mod when Advanced) flow
    // directly under the knob grid, so they're carved inside the branch.
    const bool showGlobalMod = (currentTab == 0 && advancedMode);
    juce::Rectangle<int> specRow, bottom;
    watermark.clear();

    // Drawers lay out into drawerContent (origin 0,0, effectively unbounded
    // height); the play surface lays out straight into the editor.
    const auto contentZone = area;
    constexpr int kBigH = 4000;
    if (currentTab == -1)
        drawerView.setVisible (false);
    else
    {
        drawerView.setVisible (true);
        drawerView.setBounds (contentZone);
        area = juce::Rectangle<int> (0, 0, contentZone.getWidth() - drawerView.getScrollBarThickness() - 2, kBigH);
    }

    // One row grammar for every stage: [title][on][extra slot][knob][knob]...
    // The extra slot is always reserved (combo / second toggle / nothing) so the
    // knob column starts at the same x on every row of every drawer.
    auto stageRow = [&] (int height, juce::Label* title, juce::Button* on,
                         std::initializer_list<std::pair<juce::Component*, int>> extras,
                         std::initializer_list<std::pair<juce::Slider*, juce::Label*>> knobs)
    {
        auto row = area.removeFromTop (height);
        auto t = row.removeFromLeft (kStageTitleW);
        if (title != nullptr) title->setBounds (t.withSizeKeepingCentre (kStageTitleW, 22));
        auto o = row.removeFromLeft (kStageOnW);
        if (on != nullptr) on->setBounds (o.withSizeKeepingCentre (kStageOnW - 4, 24));
        auto x = row.removeFromLeft (kStageExtraW);
        for (auto& ex : extras)
        {
            auto c = x.removeFromLeft (ex.second);
            ex.first->setBounds (c.withSizeKeepingCentre (ex.second - 6, 26));
        }
        row.removeFromLeft (8);
        for (auto& kv : knobs)
            layoutDialCell (row, *kv.second, *kv.first, kCell, kKnob);
        area.removeFromTop (6);
    };

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
        freezeButton.setBounds (area.removeFromTop (30).removeFromRight (104).reduced (2, 2));
        area.removeFromTop (4);
        const float wmSize = (float) juce::jmin (contentZone.getWidth(), contentZone.getHeight()) * 1.05f;
        auto wmBounds = juce::Rectangle<float> (wmSize, wmSize).withCentre (contentZone.toFloat().getCentre());
        watermark = gf::makeBiohazardPath (wmBounds);

        auto grid = area.removeFromTop (juce::jmin (area.getHeight(), 2 * (kRowH + 24)));
        layoutKnobGrid (grid, 5, 2, kNumKnobs, [this] (int idx, juce::Rectangle<int> cell)
        {
            auto top = cell.removeFromTop (20);
            if (advancedMode)
            {
                knobs[(size_t) idx].modButton.setBounds (top.removeFromRight (26).reduced (1, 0));
                knobs[(size_t) idx].lock.setBounds (top.removeFromRight (28));
            }
            knobs[(size_t) idx].label.setBounds (top);
            const int d = juce::jmin (kKnob + 10, cell.getWidth(), cell.getHeight());
            auto k = cell.withSizeKeepingCentre (d, d);
            knobs[(size_t) idx].slider.setBounds (k);
            if (knobs[(size_t) idx].ring != nullptr)
                knobs[(size_t) idx].ring->setBounds (k);
        });
        area.removeFromTop (gap);
        bottom = area.removeFromTop (122);
        // SOURCE row: [SOURCE][Live/Sample][Load...][Clear][file name]
        {
            area.removeFromTop (4);
            auto srow = area.removeFromTop (34);
            grainSourceTitle.setBounds (srow.removeFromLeft (84).withSizeKeepingCentre (84, 20));
            grainSourceBox.setBounds (srow.removeFromLeft (104).withSizeKeepingCentre (100, 26));
            srow.removeFromLeft (6);
            grainSampleLoad.setBounds (srow.removeFromLeft (84).withSizeKeepingCentre (80, 26));
            grainSampleClear.setBounds (srow.removeFromLeft (70).withSizeKeepingCentre (66, 26));
            srow.removeFromLeft (8);
            grainSampleName.setBounds (srow.withTrimmedRight (8));
        }
        // MIDI row: [MIDI on][Root][Glide][Vel->Amp]   [Poly Grains][MPE]
        {
            area.removeFromTop (4);
            auto mrow = area.removeFromTop (56);
            midiEnableButton.setBounds (mrow.removeFromLeft (84).withSizeKeepingCentre (80, 26));
            auto cell = [&] (juce::Slider& sl, juce::Label& lb)
            {
                auto c = mrow.removeFromLeft (84);
                lb.setBounds (c.removeFromTop (14));
                const int d = juce::jmin (36, c.getHeight());
                sl.setBounds (c.withSizeKeepingCentre (d, d));
            };
            cell (midiRootSlider, midiRootLabel);
            cell (midiGlideSlider, midiGlideLabel);
            cell (midiVelAmpSlider, midiVelAmpLabel);
            mrow.removeFromLeft (gap * 2);
            polyGrainButton.setBounds (mrow.removeFromLeft (124).withSizeKeepingCentre (120, 26));
            mpeOnButton.setBounds (mrow.removeFromLeft (80).withSizeKeepingCentre (76, 26));
        }
        if (showGlobalMod)
        {
            area.removeFromTop (gap);
            specRow = area.removeFromTop (74);
        }
    }
    else if (currentTab == 2)
    {
        auto grid = area.removeFromTop (juce::jmin (area.getHeight() - 180, 2 * (kRowH + 24)));
        layoutKnobGrid (grid, 5, 2, kNumPrettyKnobs, [this] (int idx, juce::Rectangle<int> cell)
        {
            auto top = cell.removeFromTop (20);
            if (advancedMode)
            {
                prettyKnobs[(size_t) idx].modButton.setBounds (top.removeFromRight (26).reduced (1, 0));
                prettyKnobs[(size_t) idx].lock.setBounds (top.removeFromRight (28));
            }
            prettyKnobs[(size_t) idx].label.setBounds (top);
            const int d = juce::jmin (kKnob + 10, cell.getWidth(), cell.getHeight());
            auto k = cell.withSizeKeepingCentre (d, d);
            prettyKnobs[(size_t) idx].slider.setBounds (k);
            if (prettyKnobs[(size_t) idx].ring != nullptr)
                prettyKnobs[(size_t) idx].ring->setBounds (k);
        });

        // Output knob fills the empty bottom-right cell of the 5x2 grid, same scale.
        {
            const int pCellW = grid.getWidth() / 5;
            const int pCellH = grid.getHeight() / 2;
            auto outCell = juce::Rectangle<int> (grid.getX() + 4 * pCellW, grid.getY() + pCellH,
                                                 pCellW, pCellH).reduced (10, 8);
            prettyOutputLabel.setBounds (outCell.removeFromTop (20));
            const int d = juce::jmin (kKnob + 10, outCell.getWidth(), outCell.getHeight());
            prettyOutputKnob.setBounds (outCell.withSizeKeepingCentre (d, d));
        }

        // MODULES: on/off pills. COLOR: the ten character knobs. Flow under the grid.
        area.removeFromTop (gap);
        auto dnaSection = area.removeFromTop (168);
        dnaHeader.setBounds (dnaSection.removeFromTop (16).reduced (4, 0));
        juce::ToggleButton* mods[10] = { &echoOnButton, &reverbOnButton, &chorusOnButton, &crushOnButton,
                                         &phaserOnButton, &flangerOnButton, &dreamOnButton, &angelOnButton,
                                         &harmonyOnButton, &convolutionOnButton };
        // Row 1: echo-sync + the four classic modules.
        auto modRow1 = dnaSection.removeFromTop (24);
        echoSyncBox.setBounds (modRow1.removeFromLeft (80).withSizeKeepingCentre (76, 22));
        const int w1 = juce::jmin (118, modRow1.getWidth() / 4);
        for (int i = 0; i < 4; ++i)
            mods[i]->setBounds (modRow1.removeFromLeft (w1).reduced (3, 1));
        // Row 2: the five movement + shimmer modules + convolve, centred.
        auto modRow2 = dnaSection.removeFromTop (24);
        const int w2 = juce::jmin (118, modRow2.getWidth() / 6);
        modRow2 = modRow2.withSizeKeepingCentre (w2 * 6, modRow2.getHeight());
        for (int i = 4; i < 10; ++i)
            mods[i]->setBounds (modRow2.removeFromLeft (w2).reduced (3, 1));
        // Row 3: Load IR button + the loaded IR name (only meaningful when Convolve is on).
        auto modRow3 = dnaSection.removeFromTop (22);
        convolutionIRBox.setBounds (modRow3.removeFromLeft (110).reduced (3, 0));
        modRow3.removeFromLeft (6);
        convolutionLoadButton.setBounds (modRow3.removeFromLeft (90).reduced (3, 1));
        modRow3.removeFromLeft (8);
        convolutionIRLabel.setBounds (modRow3.removeFromLeft (juce::jmin (320, modRow3.getWidth())));

        dnaSection.removeFromTop (6);
        colorHeader.setBounds (dnaSection.removeFromTop (16).reduced (4, 0));
        dnaSection.removeFromTop (2);
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
        tempoLockOnButton.setBounds (topToggles.removeFromLeft (134).reduced (2, 4));

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

        // Body: [left loops (Advanced) | right master knobs], bounded height, then
        // the Routing + Pitch Lock band under it.
        auto body = area.removeFromTop (advancedMode ? 250 : 230);
        auto routeRow = area.removeFromTop (46);
        area.removeFromTop (gap);
        if (advancedMode)
        {
            routingLabel.setBounds (routeRow.removeFromLeft (66).withSizeKeepingCentre (66, 26));
            routingMode.setBounds (routeRow.removeFromLeft (168).withSizeKeepingCentre (164, 28));
            routeRow.removeFromLeft (gap * 2);
        }
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

        // Split the body: LEFT loops (Advanced only) | RIGHT master.
        auto leftCol = advancedMode ? body.removeFromLeft (juce::jmax (300, body.getWidth() * 42 / 100))
                                    : juce::Rectangle<int>();
        if (advancedMode) body.removeFromLeft (gap * 2);
        auto rightCol = body;

        // ---- LEFT: one send/return loop per engine, headed by its on/off ----
        auto loopPanel = [&] (juce::Rectangle<int> r, juce::ToggleButton& onBtn,
                              juce::Slider& send, juce::Label& sendL,
                              juce::Slider& ret,  juce::Label& retL)
        {
            onBtn.setBounds (r.removeFromTop (30).withTrimmedLeft (6).withWidth (180));
            r.removeFromTop (2);
            const int half = r.getWidth() / 2;
            auto place = [] (juce::Rectangle<int> c, juce::Label& l, juce::Slider& s)
            {
                l.setBounds (c.removeFromTop (16));
                const int d = juce::jmin (kKnob + 8, c.getWidth(), c.getHeight());
                s.setBounds (c.withSizeKeepingCentre (d, d));
            };
            place (r.removeFromLeft (half).reduced (10, 6), sendL, send);
            place (r.reduced (10, 6), retL, ret);
        };
        if (advancedMode)
        {
            const int loopH = leftCol.getHeight() / 2;
            loopPanel (leftCol.removeFromTop (loopH).reduced (2), entropyOnButton,
                       entropySend, mixLabels[1], entropyReturn, mixLabels[2]);
            loopPanel (leftCol.reduced (2), prettifierOnButton,
                       prettifierSend, mixLabels[3], prettifierReturn, mixLabels[4]);
        }

        // ---- RIGHT: the six master knobs on one row, then the four EQ knobs ----
        std::pair<juce::Slider*, juce::Label*> mk[] = {
            { &dryLevel, &mixLabels[0] }, { &mixWidth, &mixLabels[7] },  { &mixGlue, &mixLabels[8] },
            { &mixCeiling, &mixLabels[9] }, { &mixOutput, &mixLabels[5] }, { &chaosBeauty, &mixLabels[6] } };
        {
            auto r = rightCol.removeFromTop (kRowH + 8);
            const int cw = juce::jmin (kCell + 24, r.getWidth() / 6);
            r = r.withSizeKeepingCentre (cw * 6, r.getHeight());
            for (auto& kv : mk) layoutDialCell (r, *kv.second, *kv.first, cw, kKnob + 8);
        }
        rightCol.removeFromTop (gap);
        eqHeader.setBounds (rightCol.removeFromTop (18).reduced (4, 0));
        {
            auto r = rightCol.removeFromTop (kRowH);
            const int cw = juce::jmin (kCell + 24, r.getWidth() / 4);
            r = r.withSizeKeepingCentre (cw * 4, r.getHeight());
            juce::Slider* eqSliders[] = { &eqLowKnob, &eqMidKnob, &eqHighKnob, &eqLoFiKnob };
            for (size_t i = 0; i < eqLabels.size(); ++i)
                layoutDialCell (r, eqLabels[i], *eqSliders[i], cw, kKnob);
        }
    }
    else if (currentTab == 3)   // MACHINES
    {
        const bool adv = advancedMode;
        stageRow (kRowH, &machSpectralTitle, &machSpectralOn, {},
                  { { &machSpectralMix, &machSpectralMixL }, { &machSpectralAmount, &machSpectralAmountL } });
        stageRow (kRowH, &machPitchTitle, &machPitchOn, { { &machPitchFormant, 100 } },
                  { { &machPitchMix, &machPitchMixL }, { &machPitchShift, &machPitchShiftL } });
        if (! adv)
            stageRow (kRowH, &machDamageTitle, &machDamageOn, { { &machDamageClip, 116 } },
                      { { &machDamageAmount, &machDamageAmountL }, { &machDamageMix, &machDamageMixL } });
        else
        {
            stageRow (kRowH, &machDamageTitle, &machDamageOn, { { &machDamageClip, 116 } },
                      { { &machDamageAmount, &machDamageAmountL }, { &machDamageMix, &machDamageMixL },
                        { &machDamageBits, &machDamageBitsL },     { &machDamageRate, &machDamageRateL },
                        { &machDamageJitter, &machDamageJitterL }, { &machDamageNoise, &machDamageNoiseL } });
            stageRow (kRowH, nullptr, nullptr, { { &machDamageSplit, 120 } },
                      { { &machDamageDropout, &machDamageDropoutL }, { &machDamageTone, &machDamageToneL },
                        { &machDamageSplitHz, &machDamageSplitHzL }, { &machDamageHighAmount, &machDamageHighAmountL } });
        }
        if (! adv)
            stageRow (kRowH, &machTimeTitle, &machTimeOn, { { &machTimeSync, 66 }, { &machTimeDivision, 84 } },
                      { { &machTimeChance, &machTimeChanceL }, { &machTimeMix, &machTimeMixL } });
        else
        {
            stageRow (kRowH, &machTimeTitle, &machTimeOn, { { &machTimeSync, 66 }, { &machTimeDivision, 84 } },
                      { { &machTimeChance, &machTimeChanceL }, { &machTimeMix, &machTimeMixL },
                        { &machTimeRate, &machTimeRateL },     { &machTimeSize, &machTimeSizeL },
                        { &machTimeReverse, &machTimeReverseL } });
            // Routing slots share the row grammar: title column, then the two
            // [target][depth] pairs start where the knobs do.
            auto rr = area.removeFromTop (38);
            machTimeRouteL.setBounds (rr.removeFromLeft (kStageTitleW).withSizeKeepingCentre (kStageTitleW, 22));
            rr.removeFromLeft (kStageOnW + kStageExtraW + 8);
            auto slot = [&] (juce::ComboBox& cb, juce::Slider& dk, juce::Label& dl)
            {
                cb.setBounds (rr.removeFromLeft (150).withSizeKeepingCentre (144, 28));
                auto c = rr.removeFromLeft (kCell);
                dl.setBounds (c.removeFromTop (14));
                const int d = juce::jmin (kKnob - 16, c.getHeight());
                dk.setBounds (c.withSizeKeepingCentre (d, d));
                rr.removeFromLeft (gap);
            };
            slot (machTimeRoute1Target, machTimeRoute1Depth, machTimeRoute1DepthL);
            slot (machTimeRoute2Target, machTimeRoute2Depth, machTimeRoute2DepthL);
            area.removeFromTop (6);
        }
        if (adv)
        {
            stageRow (kRowH, &machDuckerTitle, &machDuckerOn, { { &machDuckerSource, 128 } },
                      { { &machDuckerAmount, &machDuckerAmountL }, { &machDuckerThreshold, &machDuckerThresholdL },
                        { &machDuckerAttack, &machDuckerAttackL }, { &machDuckerRelease, &machDuckerReleaseL } });

            // Mod matrix: title column holds the title + poly/MPE; rows start at the knob x.
            auto head = area.removeFromTop (22);
            modMatrixTitle.setBounds (head.removeFromLeft (kStageTitleW).withSizeKeepingCentre (kStageTitleW, 20));
            area.removeFromTop (4);
            // Two slots per row -> the four slots take two rows instead of four.
            for (int r = 0; r < 2; ++r)
            {
                auto row = area.removeFromTop (24);
                row.removeFromLeft (kStageTitleW + kStageOnW - 40);
                for (int k = 0; k < 2; ++k)
                {
                    const size_t i = (size_t) (r * 2 + k);
                    modMatrixSource[i].setBounds (row.removeFromLeft (128).reduced (2, 1));
                    modMatrixArrow [i].setBounds (row.removeFromLeft (18));
                    modMatrixTarget[i].setBounds (row.removeFromLeft (128).reduced (2, 1));
                    row.removeFromLeft (6);
                    modMatrixActivity[i].setBounds (row.removeFromLeft (12).withSizeKeepingCentre (10, 10));
                    row.removeFromLeft (2);
                    modMatrixDepth [i].setBounds (row.removeFromLeft (96).reduced (2, 4));
                    row.removeFromLeft (18);
                }
                area.removeFromTop (2);
            }
            area.removeFromTop (6);

            // MOD SOURCES: the generators the matrix can route. LFO 2 and the
            // random S&H share one row; the step sequencer gets its own strip.
            stageRow (kRowH, &modSourcesTitle, nullptr, { { &lfo2Shape, 92 }, { &lfo2Sync, 84 } },
                      { { &lfo2Rate, &lfo2RateL }, { &modRandomRate, &modRandomRateL },
                        { &modCcNumber, &modCcNumberL } });
            stageRow (kRowH, &stepSeqTitle, nullptr, { { &stepSeqDivision, 92 } },
                      { { &stepSeqLength, &stepSeqLengthL }, { &stepSeqSmooth, &stepSeqSmoothL } });
            {
                // 16 vertical steps across the knob columns, starting where the
                // knobs do so the strip lines up with the rest of the grammar.
                auto row = area.removeFromTop (76);
                row.removeFromLeft (kStageTitleW + kStageOnW + kStageExtraW + 8);
                const int cell = juce::jmax (12, row.getWidth() / 16);
                for (auto& st : stepSeqSteps)
                    st.setBounds (row.removeFromLeft (cell).reduced (2, 2));
                area.removeFromTop (6);
            }
        }
    }
    else if (currentTab == 5)   // AIR
    {
        stageRow (kRowH, &airHeader, nullptr, {},
                  { { &airCrossover, &airCrossoverL }, { &airMix, &airMixL } });
        stageRow (kRowH, &airSquelchTitle, &airSquelchOn, { { &airSquelchMode, 100 } },
                  { { &airSquelchHz, &airSquelchHzL }, { &airSquelchRes, &airSquelchResL }, { &airSquelchEnv, &airSquelchEnvL } });
        stageRow (kRowH, &airExciterTitle, &airExciterOn, {},
                  { { &airExciterDrive, &airExciterDriveL }, { &airExciterMix, &airExciterMixL } });
        stageRow (kRowH, &airShelfTitle, &airShelfOn, {},
                  { { &airShelfHz, &airShelfHzL }, { &airShelfAmount, &airShelfAmountL }, { &airShelfThreshold, &airShelfThresholdL } });
        stageRow (kRowH, &airPhaserTitle, &airPhaserOn, {},
                  { { &airPhaserRate, &airPhaserRateL }, { &airPhaserDepth, &airPhaserDepthL }, { &airPhaserMix, &airPhaserMixL } });
        stageRow (kRowH, &airDelayTitle, &airDelayOn, {},
                  { { &airDelayMs, &airDelayMsL }, { &airDelayFeedback, &airDelayFeedbackL }, { &airDelayMix, &airDelayMixL } });
    }
    else if (currentTab == -1)   // play surface: morph pad + grain cloud, larger
    {
        morphPadLabel.setBounds (area.removeFromTop (18).withSizeKeepingCentre (200, 18));
        area.removeFromTop (6);
        const int padSize = juce::jlimit (160, 380, area.getHeight() - 60);
        auto block = area.removeFromTop (padSize + 34);
        block = block.withSizeKeepingCentre (juce::jmin (block.getWidth(), padSize * 2 + 24), block.getHeight());
        auto pad = block.removeFromLeft (padSize).withHeight (padSize);
        morphPad.setBounds (pad);
        block.removeFromLeft (24);
        grainViz.setBounds (block.removeFromLeft (padSize).withHeight (padSize));
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


    if (currentTab != -1)
    {
        const int used = kBigH - area.getHeight() + 10;
        drawerContent.setSize (contentZone.getWidth() - drawerView.getScrollBarThickness() - 2,
                               juce::jmax (used, contentZone.getHeight()));
    }
}
