#include "GrainFreeze/BiohazardLookAndFeel.h"
#include <cmath>

namespace gf
{

// ---- Design tokens --------------------------------------------------------
const juce::Colour BiohazardLookAndFeel::ink       { 0xff07070a };
const juce::Colour BiohazardLookAndFeel::stage     { 0xff020203 };
const juce::Colour BiohazardLookAndFeel::surface   { 0xff100f14 };
const juce::Colour BiohazardLookAndFeel::surfaceHi { 0xff1b1a21 };
const juce::Colour BiohazardLookAndFeel::line      { 0x17ffffff };
const juce::Colour BiohazardLookAndFeel::lineHi    { 0x1fffffff };
const juce::Colour BiohazardLookAndFeel::accentA   { 0xfff4b35e };
const juce::Colour BiohazardLookAndFeel::accentB   { 0xffff6b6b };
const juce::Colour BiohazardLookAndFeel::accentC   { 0xff67e8f9 };
const juce::Colour BiohazardLookAndFeel::text      { 0xfff7f0e8 };
const juce::Colour BiohazardLookAndFeel::textDim   { 0xffa99e92 };
const juce::Colour BiohazardLookAndFeel::negative  { 0xffff6b6b };

// ---- Legacy aliases -------------------------------------------------------
const juce::Colour BiohazardLookAndFeel::bg          = BiohazardLookAndFeel::ink;
const juce::Colour BiohazardLookAndFeel::panel       = BiohazardLookAndFeel::surface;
const juce::Colour BiohazardLookAndFeel::metal       = BiohazardLookAndFeel::surfaceHi;
const juce::Colour BiohazardLookAndFeel::metalHi     { 0xff262430 };
const juce::Colour BiohazardLookAndFeel::metalLo     { 0xff020203 };
const juce::Colour BiohazardLookAndFeel::toxic       = BiohazardLookAndFeel::accentA;
const juce::Colour BiohazardLookAndFeel::toxicDim    { 0xff6b4e28 };
const juce::Colour BiohazardLookAndFeel::coral       = BiohazardLookAndFeel::negative;
const juce::Colour BiohazardLookAndFeel::textCol     = BiohazardLookAndFeel::text;
const juce::Colour BiohazardLookAndFeel::gold        = BiohazardLookAndFeel::accentB;
const juce::Colour BiohazardLookAndFeel::goldDim     { 0xff6b4e28 };
const juce::Colour BiohazardLookAndFeel::blendAccent = BiohazardLookAndFeel::accentC;
const juce::Colour BiohazardLookAndFeel::iceBlue     = BiohazardLookAndFeel::accentC;
const juce::Colour BiohazardLookAndFeel::iceBlueDim  { 0xff2a6270 };

namespace
{
juce::Font uiFont (float height, bool bold = false)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
}
}

BiohazardLookAndFeel::BiohazardLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, ink);
    setColour (juce::Slider::textBoxTextColourId,         text);
    setColour (juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId,   juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId,                 text);
    setColour (juce::ComboBox::backgroundColourId,        surfaceHi);
    setColour (juce::ComboBox::textColourId,              text);
    setColour (juce::ComboBox::outlineColourId,           juce::Colours::transparentBlack);
    setColour (juce::ComboBox::arrowColourId,             textDim);
    setColour (juce::PopupMenu::backgroundColourId,       surface);
    setColour (juce::PopupMenu::textColourId,             text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accentA.withAlpha (0.18f));
    setColour (juce::PopupMenu::highlightedTextColourId,  text);
    setColour (juce::TextButton::textColourOffId,         text);
    setColour (juce::TextButton::textColourOnId,          ink);
    setColour (juce::TextEditor::backgroundColourId,      surfaceHi);
    setColour (juce::TextEditor::textColourId,            text);
    setColour (juce::TextEditor::outlineColourId,         juce::Colours::transparentBlack);
    setColour (juce::TextEditor::focusedOutlineColourId,  accentA.withAlpha (0.6f));
    setColour (juce::TextEditor::highlightColourId,       accentA.withAlpha (0.28f));
    setColour (juce::CaretComponent::caretColourId,       accentA);
    setColour (juce::BubbleComponent::backgroundColourId, surface);
    setColour (juce::BubbleComponent::outlineColourId,    line);
    setColour (juce::TooltipWindow::backgroundColourId,   surface);
    setColour (juce::TooltipWindow::textColourId,         text);
    setColour (juce::TooltipWindow::outlineColourId,      juce::Colours::transparentBlack);
    setColour (juce::ScrollBar::thumbColourId,            accentA.withAlpha (0.5f));
}

juce::Colour BiohazardLookAndFeel::accent() const
{
    // One accent across the whole plugin: the pages are one instrument, not
    // three, and a per-tab colour shift was reading as three different plugins.
    return accentA;
}

juce::Colour BiohazardLookAndFeel::accentDim() const { return toxicDim; }

// ---------------------------------------------------------------------------
void BiohazardLookAndFeel::drawTracked (juce::Graphics& g, const juce::String& textToDraw,
                                        juce::Rectangle<float> area, juce::Justification just,
                                        float tracking)
{
    if (textToDraw.isEmpty())
        return;

    const auto font = g.getCurrentFont();
    const auto up = textToDraw.toUpperCase();

    float total = 0.0f;
    for (int i = 0; i < up.length(); ++i)
        total += font.getStringWidthFloat (up.substring (i, i + 1)) + tracking;
    total -= tracking;

    float x = area.getX();
    if (just.testFlags (juce::Justification::horizontallyCentred))
        x = area.getCentreX() - total * 0.5f;
    else if (just.testFlags (juce::Justification::right))
        x = area.getRight() - total;

    float y = area.getCentreY() + font.getAscent() * 0.5f - font.getDescent() * 0.25f;
    if (just.testFlags (juce::Justification::top))
        y = area.getY() + font.getAscent();
    else if (just.testFlags (juce::Justification::bottom))
        y = area.getBottom() - font.getDescent();

    for (int i = 0; i < up.length(); ++i)
    {
        const auto glyph = up.substring (i, i + 1);
        g.drawSingleLineText (glyph, juce::roundToInt (x), juce::roundToInt (y));
        x += font.getStringWidthFloat (glyph) + tracking;
    }
}

// Tracked when it fits, shrunk to fit when it does not. drawTracked places
// glyphs by hand and knows nothing about the area's width, so long captions like
// "Chaos <-> Beauty" were being clipped to "IOS <-> BEA".
static void drawTrackedFitted (juce::Graphics& g, const juce::String& textToDraw,
                               juce::Rectangle<float> area, juce::Justification just,
                               float tracking)
{
    const auto up = textToDraw.toUpperCase();
    const float width = g.getCurrentFont().getStringWidthFloat (up)
                      + tracking * (float) juce::jmax (0, up.length() - 1);
    if (width <= area.getWidth())
        BiohazardLookAndFeel::drawTracked (g, textToDraw, area, just, tracking);
    else
        g.drawFittedText (up, area.toNearestInt(), just, 1, 0.62f);
}

void BiohazardLookAndFeel::drawPanel (juce::Graphics& g, juce::Rectangle<float> bounds,
                                      float radius, bool elevated)
{
    // Panels sit on a soft drop shadow -- 0 22px 80px rgba(0,0,0,.4) in the
    // reference -- approximated with a few offset, fading rounded rects.
    for (int i = 3; i >= 1; --i)
    {
        g.setColour (juce::Colours::black.withAlpha (0.10f));
        g.fillRoundedRectangle (bounds.translated (0.0f, (float) i * 2.5f)
                                      .expanded ((float) i * 1.5f), radius + (float) i);
    }
    g.setColour (elevated ? surfaceHi : surface);
    g.fillRoundedRectangle (bounds, radius);
    g.setColour (line);
    g.drawRoundedRectangle (bounds.reduced (0.5f), radius, 1.0f);
}

void BiohazardLookAndFeel::drawStage (juce::Graphics& g, juce::Rectangle<float> bounds,
                                      float radius, float glow)
{
    // The bed a visualiser sits on: darker than anything else on the page, with
    // an amber bloom around it (0 0 90px rgba(244,179,94,.13)).
    if (glow > 0.0f)
    {
        for (int i = 4; i >= 1; --i)
        {
            const float spread = (float) i * 5.0f;
            g.setColour (accentA.withAlpha (0.030f * glow));
            g.fillRoundedRectangle (bounds.expanded (spread), radius + spread * 0.6f);
        }
    }
    g.setColour (stage);
    g.fillRoundedRectangle (bounds, radius);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), radius, 1.0f);
}

void BiohazardLookAndFeel::drawEyebrow (juce::Graphics& g, const juce::String& textToDraw,
                                        juce::Rectangle<float> area, juce::Justification just)
{
    g.setColour (accentA);
    // 0.74rem at 0.16em tracking, in the reference's terms.
    g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    drawTrackedFitted (g, textToDraw, area, just, 1.8f);
}

void BiohazardLookAndFeel::drawGlow (juce::Graphics& g, const juce::Path& shape,
                                     juce::Colour colour, float strength)
{
    // Three passes of widening, fading stroke read as bloom without a blur.
    for (int i = 3; i >= 1; --i)
    {
        const float w = 2.0f + (float) i * 2.2f;
        g.setColour (colour.withAlpha (0.055f * strength * (float) (4 - i)));
        g.strokePath (shape, juce::PathStrokeType (w, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
    }
}

void BiohazardLookAndFeel::drawPanelInset (juce::Graphics& g, juce::Rectangle<float> bounds,
                                           float cornerRadius) const
{
    drawPanel (g, bounds, cornerRadius, false);
}

void BiohazardLookAndFeel::drawPanelRaised (juce::Graphics& g, juce::Rectangle<float> bounds,
                                            float cornerRadius) const
{
    drawPanel (g, bounds, cornerRadius, true);
}

// ---------------------------------------------------------------------------
void BiohazardLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                             float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                             juce::Slider& slider)
{
    const auto area = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height)
                          .reduced (2.0f);
    const auto centre = area.getCentre();
    const float radius = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f;
    const float ringR = radius - 3.0f;
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const bool enabled = slider.isEnabled();
    const bool hover = slider.isMouseOverOrDragging();
    const auto acc = accent();

    // Track.
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, ringR, ringR, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (juce::Colours::white.withAlpha (enabled ? 0.10f : 0.05f));
    g.strokePath (track, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    // Value arc. Bipolar controls fill out from twelve o'clock instead of from
    // the left, so "no modulation" reads as an empty ring rather than a half-full one.
    const bool bipolar = slider.getMinimum() < -0.0001;
    const float originAngle = bipolar ? (rotaryStartAngle + rotaryEndAngle) * 0.5f : rotaryStartAngle;
    if (std::abs (angle - originAngle) > 0.001f)
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, ringR, ringR, 0.0f,
                             juce::jmin (originAngle, angle), juce::jmax (originAngle, angle), true);
        if (enabled)
            drawGlow (g, value, acc, hover ? 1.35f : 1.0f);
        g.setColour (enabled ? acc : textDim.withAlpha (0.4f));
        g.strokePath (value, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
    }

    // Face.
    const float faceR = ringR - 6.0f;
    if (faceR > 3.0f)
    {
        auto face = juce::Rectangle<float> (faceR * 2.0f, faceR * 2.0f).withCentre (centre);
        g.setColour (hover ? surfaceHi.brighter (0.10f) : surfaceHi);
        g.fillEllipse (face);
        g.setColour (line);
        g.drawEllipse (face.reduced (0.5f), 1.0f);

        // Pointer.
        const float p0 = faceR * 0.34f, p1 = faceR * 0.86f;
        const float ca = std::cos (angle - juce::MathConstants<float>::halfPi);
        const float sa = std::sin (angle - juce::MathConstants<float>::halfPi);
        g.setColour (enabled ? acc : textDim);
        g.drawLine (centre.x + ca * p0, centre.y + sa * p0,
                    centre.x + ca * p1, centre.y + sa * p1, 2.0f);
    }
}

void BiohazardLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                             float sliderPos, float, float,
                                             juce::Slider::SliderStyle, juce::Slider& slider)
{
    const auto acc = accent();

    // Step-sequencer steps: a vertical bipolar bar rather than a horizontal
    // track, brightened while that step is the one playing.
    if (slider.getComponentID() == "step")
    {
        auto b = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (1.0f);
        const bool lit = (bool) slider.getProperties().getWithDefault ("lit", false);
        g.setColour (surfaceHi);
        g.fillRoundedRectangle (b, 2.0f);
        const float mid = b.getCentreY();
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawHorizontalLine ((int) mid, b.getX(), b.getRight());
        const float top = juce::jmin (mid, sliderPos);
        const float bot = juce::jmax (mid, sliderPos);
        auto bar = juce::Rectangle<float> (b.getX() + 1.0f, top, b.getWidth() - 2.0f,
                                           juce::jmax (1.5f, bot - top));
        g.setColour (lit ? acc : acc.withAlpha (0.55f));
        g.fillRoundedRectangle (bar, 1.5f);
        g.setColour (lit ? acc.withAlpha (0.85f) : line);
        g.drawRoundedRectangle (b, 2.0f, lit ? 1.4f : 1.0f);
        return;
    }

    const float cy = (float) y + (float) height * 0.5f;
    auto track = juce::Rectangle<float> ((float) x + 2.0f, cy - 3.5f, (float) width - 4.0f, 7.0f);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.fillRoundedRectangle (track, track.getHeight() * 0.5f);

    // Bipolar sliders (mod depth) fill from the centre.
    const bool bipolar = slider.getMinimum() < -0.0001;
    const float origin = bipolar ? track.getCentreX() : track.getX();
    auto filled = juce::Rectangle<float> (juce::jmin (origin, sliderPos), track.getY(),
                                          juce::jmax (2.0f, std::abs (sliderPos - origin)),
                                          track.getHeight());
    g.setColour (acc);
    g.fillRoundedRectangle (filled, filled.getHeight() * 0.5f);

    const float r = juce::jmin (7.0f, (float) height * 0.45f);
    auto thumb = juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre ({ sliderPos, cy });
    g.setColour (slider.isMouseOverOrDragging() ? text : juce::Colour (0xffe8dccf));
    g.fillEllipse (thumb);
}

// ---------------------------------------------------------------------------
void BiohazardLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b,
                                                 const juce::Colour&,
                                                 bool highlighted, bool down)
{
    const auto acc = accent();
    auto bounds = b.getLocalBounds().toFloat();

    if (b.getComponentID() == "settings")
        return;  // bare icon

    // Signal-chain tiles: no box when idle, an accent underline when open.
    if (b.getComponentID() == "chain")
    {
        const bool on = b.getToggleState();
        if (on || highlighted)
        {
            g.setColour (juce::Colours::white.withAlpha (on ? 0.05f : 0.03f));
            g.fillRoundedRectangle (bounds, 8.0f);
        }
        if (on)
        {
            auto bar = juce::Rectangle<float> (bounds.getCentreX() - 16.0f, bounds.getBottom() - 3.0f,
                                               32.0f, 2.0f);
            g.setColour (acc);
            g.fillRoundedRectangle (bar, 1.0f);
        }
        return;
    }

    const bool on = b.getToggleState();
    const float radius = juce::jmin (bounds.getHeight() * 0.5f, kControlRadius);
    bounds = bounds.reduced (0.5f);
    // Hover lifts by a pixel, as in the reference.
    if (highlighted && ! down)
        bounds = bounds.translated (0.0f, -1.0f);

    if (on)
    {
        juce::ColourGradient grad (accentA, bounds.getX(), bounds.getY(),
                                   accentB, bounds.getRight(), bounds.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (bounds, radius);
        if (down)
        {
            g.setColour (juce::Colours::black.withAlpha (0.15f));
            g.fillRoundedRectangle (bounds, radius);
        }
    }
    else
    {
        g.setColour (juce::Colours::white.withAlpha (down ? 0.14f : (highlighted ? 0.12f : 0.07f)));
        g.fillRoundedRectangle (bounds, radius);
        g.setColour (lineHi);
        g.drawRoundedRectangle (bounds, radius, 1.0f);
    }
}

void BiohazardLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b,
                                           bool highlighted, bool)
{
    const auto acc = accent();
    const bool on = b.getToggleState();
    const bool chain = b.getComponentID() == "chain";

    juce::Colour colour = on ? juce::Colour (0xff160d07) : text;   // the reference's ink-on-amber
    if (chain)
        colour = on ? accentA : (highlighted ? text.withAlpha (0.9f) : textDim);

    g.setColour (b.isEnabled() ? colour : textDim.withAlpha (0.4f));
    g.setFont (getTextButtonFont (b, b.getHeight()));
    juce::ignoreUnused (acc);

    drawTrackedFitted (g, b.getButtonText(), b.getLocalBounds().toFloat(),
                       juce::Justification::centred, chain ? 2.0f : 1.2f);
}

juce::Font BiohazardLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return uiFont (juce::jlimit (10.0f, 14.0f, (float) buttonHeight * 0.44f), true);
}

void BiohazardLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b,
                                             bool highlighted, bool)
{
    const auto acc = accent();
    const bool on = b.getToggleState();
    auto bounds = b.getLocalBounds().toFloat();

    // A switch, not a tick box: capsule track with a travelling knob. It gives
    // ground when the button is narrow, so a label beside it still fits -- these
    // sit in fixed-width cells and a capsule that never shrank was clipping them.
    const bool hasText = b.getButtonText().isNotEmpty();
    float h = juce::jlimit (12.0f, 20.0f, bounds.getHeight() - 6.0f);
    if (hasText)
        h = juce::jmin (h, juce::jmax (12.0f, bounds.getWidth() * 0.42f / 1.85f));
    const float w = h * 1.85f;
    auto track = juce::Rectangle<float> (bounds.getX() + 1.0f, bounds.getCentreY() - h * 0.5f, w, h);

    if (on)
    {
        juce::ColourGradient grad (accentA, track.getX(), track.getY(),
                                   accentB, track.getRight(), track.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (track, h * 0.5f);
    }
    else
    {
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.fillRoundedRectangle (track, h * 0.5f);
        g.setColour (highlighted ? accentA.withAlpha (0.45f) : lineHi);
        g.drawRoundedRectangle (track.reduced (0.5f), h * 0.5f, 1.0f);
    }

    const float kr = h * 0.5f - 3.0f;
    const float kx = on ? track.getRight() - kr - 3.0f : track.getX() + kr + 3.0f;
    auto knob = juce::Rectangle<float> (kr * 2.0f, kr * 2.0f).withCentre ({ kx, track.getCentreY() });
    g.setColour (on ? juce::Colour (0xff160d07) : textDim);
    g.fillEllipse (knob);

    auto textArea = bounds.withTrimmedLeft (w + 8.0f);
    if (textArea.getWidth() > 4.0f && hasText)
    {
        g.setColour (b.isEnabled() ? (on ? text : textDim) : textDim.withAlpha (0.4f));
        g.setFont (uiFont (11.5f, true));
        // Tracked when there is room for it, shrunk to fit when there is not --
        // drawTracked has no idea about clipping, so measure first.
        const auto up = b.getButtonText().toUpperCase();
        const float tracked = g.getCurrentFont().getStringWidthFloat (up) + 1.2f * (float) up.length();
        if (tracked <= textArea.getWidth())
            drawTracked (g, b.getButtonText(), textArea, juce::Justification::centredLeft, 1.2f);
        else
            g.drawFittedText (up, textArea.toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);
    }
}

// ---------------------------------------------------------------------------
void BiohazardLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                         int, int, int, int, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
    const bool hover = box.isMouseOver();
    g.setColour (juce::Colours::white.withAlpha (hover ? 0.12f : 0.07f));
    g.fillRoundedRectangle (bounds, kControlRadius);
    g.setColour (hover ? accentA.withAlpha (0.5f) : lineHi);
    g.drawRoundedRectangle (bounds, kControlRadius, 1.0f);

    // Chevron.
    const float cx = bounds.getRight() - 13.0f, cy = bounds.getCentreY();
    juce::Path chevron;
    chevron.startNewSubPath (cx - 4.0f, cy - 2.0f);
    chevron.lineTo (cx, cy + 2.5f);
    chevron.lineTo (cx + 4.0f, cy - 2.0f);
    g.setColour (hover ? text : textDim);
    g.strokePath (chevron, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
}

void BiohazardLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (10, 1, box.getWidth() - 26, box.getHeight() - 2);
    label.setFont (uiFont (12.0f));
    label.setJustificationType (juce::Justification::centredLeft);
}

void BiohazardLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);
    g.setColour (surface);
    g.fillRoundedRectangle (bounds, 16.0f);
    g.setColour (line);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 16.0f, 1.0f);
}

void BiohazardLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                                              bool isSeparator, bool isActive, bool isHighlighted,
                                              bool isTicked, bool hasSubMenu, const juce::String& itemText,
                                              const juce::String& shortcutKeyText, const juce::Drawable*,
                                              const juce::Colour*)
{
    const auto acc = accent();
    if (isSeparator)
    {
        g.setColour (line);
        g.fillRect (area.reduced (10, 0).withHeight (1).withY (area.getCentreY()));
        return;
    }

    auto r = area.toFloat().reduced (4.0f, 1.0f);
    if (isHighlighted && isActive)
    {
        g.setColour (acc.withAlpha (0.16f));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (acc);
        g.fillRoundedRectangle (r.withWidth (2.5f), 1.2f);
    }

    g.setColour (! isActive ? textDim.withAlpha (0.45f) : (isHighlighted ? text : text.withAlpha (0.85f)));
    g.setFont (uiFont (13.0f));

    auto textArea = r.withTrimmedLeft (isTicked ? 22.0f : 12.0f).withTrimmedRight (hasSubMenu ? 22.0f : 8.0f);
    g.drawText (itemText, textArea, juce::Justification::centredLeft, true);

    if (isTicked)
    {
        juce::Path tick;
        const float ty = r.getCentreY();
        tick.startNewSubPath (r.getX() + 8.0f,  ty);
        tick.lineTo          (r.getX() + 11.5f, ty + 3.5f);
        tick.lineTo          (r.getX() + 17.0f, ty - 4.0f);
        g.setColour (acc);
        g.strokePath (tick, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
    }
    if (hasSubMenu)
    {
        juce::Path arrow;
        const float ax = r.getRight() - 14.0f, ay = r.getCentreY();
        arrow.startNewSubPath (ax - 2.0f, ay - 4.0f);
        arrow.lineTo (ax + 2.5f, ay);
        arrow.lineTo (ax - 2.0f, ay + 4.0f);
        g.setColour (textDim);
        g.strokePath (arrow, juce::PathStrokeType (1.5f));
    }
    if (shortcutKeyText.isNotEmpty())
    {
        g.setColour (textDim);
        g.setFont (uiFont (11.0f));
        g.drawText (shortcutKeyText, r.withTrimmedRight (10.0f), juce::Justification::centredRight, true);
    }
}

void BiohazardLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int width, int height,
                                                     juce::TextEditor&)
{
    auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.fillRoundedRectangle (bounds, kControlRadius);
}

void BiohazardLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int width, int height,
                                                  juce::TextEditor& editor)
{
    auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
    g.setColour (editor.hasKeyboardFocus (true) ? accentA.withAlpha (0.6f) : lineHi);
    g.drawRoundedRectangle (bounds, kControlRadius, 1.0f);
}

juce::Font BiohazardLookAndFeel::getLabelFont (juce::Label& l)
{
    return l.getFont().withHeight (juce::jmax (11.0f, l.getFont().getHeight()));
}

void BiohazardLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    if (! label.isBeingEdited())
    {
        g.setFont (getLabelFont (label));
        const auto area = label.getLocalBounds().toFloat();

        // Section headings and knob captions are set as tracked uppercase; the
        // componentID says which role a label is playing.
        const auto id = label.getComponentID();
        if (id == "section")
        {
            drawEyebrow (g, label.getText(), area, label.getJustificationType());
            return;
        }
        if (id == "caption")
        {
            g.setColour (textDim);
            drawTrackedFitted (g, label.getText(), area, label.getJustificationType(), 1.2f);
            return;
        }
        g.setColour (label.findColour (juce::Label::textColourId));
        g.drawFittedText (label.getText(), label.getLocalBounds(), label.getJustificationType(),
                          juce::jmax (1, (int) ((float) label.getHeight() / g.getCurrentFont().getHeight())),
                          label.getMinimumHorizontalScale());
        return;
    }
    juce::LookAndFeel_V4::drawLabel (g, label);
}

void BiohazardLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y,
                                          int width, int height, bool isScrollbarVertical,
                                          int thumbStartPosition, int thumbSize,
                                          bool isMouseOver, bool isMouseDown)
{
    juce::Rectangle<float> thumb;
    if (isScrollbarVertical)
        thumb = { (float) x + (float) width * 0.35f, (float) thumbStartPosition,
                  (float) width * 0.3f, (float) thumbSize };
    else
        thumb = { (float) thumbStartPosition, (float) y + (float) height * 0.35f,
                  (float) thumbSize, (float) height * 0.3f };

    g.setColour (juce::Colours::white.withAlpha (isMouseDown ? 0.35f : (isMouseOver ? 0.24f : 0.13f)));
    g.fillRoundedRectangle (thumb, thumb.getWidth() * 0.5f);
}

} // namespace gf
