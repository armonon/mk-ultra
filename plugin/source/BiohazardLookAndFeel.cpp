#include "GrainFreeze/BiohazardLookAndFeel.h"
#include <cmath>
#include <cstdio> // TEMP DIAG

// TEMP DIAG: log labels painted in the top-left ghost region, with their
// position relative to the top-level window so we can see exactly what draws there.
#define LLOG(x) do { juce::String _l; _l << x << "\n"; \
    if (FILE* _f = std::fopen ("/tmp/entropy_ghost.log", "a")) { \
        std::fputs (_l.toRawUTF8(), _f); std::fclose (_f); } } while (0)

namespace gf
{

// Modern dark studio palette: deep neutral charcoal surfaces with a faint cool
// tint, elevated panels, and clean vivid accents per tab.
const juce::Colour BiohazardLookAndFeel::bg         { 0xff0b0e11 };
const juce::Colour BiohazardLookAndFeel::panel     { 0xff151a20 };
const juce::Colour BiohazardLookAndFeel::metal     { 0xff232a31 };
const juce::Colour BiohazardLookAndFeel::metalHi   { 0xff39424c };
const juce::Colour BiohazardLookAndFeel::metalLo   { 0xff0d1014 };
const juce::Colour BiohazardLookAndFeel::toxic     { 0xff5cf03a };
const juce::Colour BiohazardLookAndFeel::toxicDim  { 0xff2c6b22 };
const juce::Colour BiohazardLookAndFeel::coral     { 0xffe08a64 };
const juce::Colour BiohazardLookAndFeel::textCol   { 0xffe2e8ec };
const juce::Colour BiohazardLookAndFeel::gold      { 0xfff7d873 };
const juce::Colour BiohazardLookAndFeel::goldDim   { 0xff8a7330 };
const juce::Colour BiohazardLookAndFeel::blendAccent { 0xff8fe8c0 };
const juce::Colour BiohazardLookAndFeel::iceBlue    { 0xff5ec6ff };
const juce::Colour BiohazardLookAndFeel::iceBlueDim { 0xff2a6e9e };

BiohazardLookAndFeel::BiohazardLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, bg);
    setColour (juce::Slider::textBoxTextColourId,         textCol);
    setColour (juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId,   juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId,                 textCol);
    setColour (juce::ComboBox::backgroundColourId,        metal);
    setColour (juce::ComboBox::textColourId,              textCol);
    setColour (juce::ComboBox::outlineColourId,           juce::Colours::transparentBlack);
    setColour (juce::ComboBox::arrowColourId,             toxic);
    setColour (juce::PopupMenu::backgroundColourId,       panel);
    setColour (juce::PopupMenu::textColourId,             textCol);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, toxicDim);
    setColour (juce::TextButton::textColourOffId,         textCol);
    setColour (juce::TextButton::textColourOnId,          bg);
    setColour (juce::TextEditor::backgroundColourId,      metal);
    setColour (juce::TextEditor::textColourId,            textCol);
    setColour (juce::TextEditor::outlineColourId,         juce::Colours::transparentBlack);
    setColour (juce::TextEditor::highlightColourId,       toxic.withAlpha (0.30f));
    setColour (juce::CaretComponent::caretColourId,       toxic);
    setColour (juce::BubbleComponent::backgroundColourId, panel.brighter (0.06f));
    setColour (juce::BubbleComponent::outlineColourId,    toxic.withAlpha (0.5f));
    setColour (juce::TooltipWindow::backgroundColourId,   panel.brighter (0.04f));
    setColour (juce::TooltipWindow::textColourId,         textCol);
    setColour (juce::TooltipWindow::outlineColourId,      juce::Colours::transparentBlack);
}

juce::Colour BiohazardLookAndFeel::accent() const
{
    switch (accentTheme)
    {
        case AccentTheme::prettifier: return iceBlue;
        case AccentTheme::mix:        return blendAccent;
        case AccentTheme::entropy:
        default:                      return toxic;
    }
}

juce::Colour BiohazardLookAndFeel::accentDim() const
{
    switch (accentTheme)
    {
        case AccentTheme::prettifier: return iceBlueDim;
        case AccentTheme::mix:        return toxicDim.interpolatedWith (goldDim, 0.5f);
        case AccentTheme::entropy:
        default:                      return toxicDim;
    }
}

void BiohazardLookAndFeel::drawPanelInset (juce::Graphics& g, juce::Rectangle<float> bounds,
                                           float cornerRadius) const
{
    drawBevelEmbossRect (g, bounds, cornerRadius, panel.darker (0.08f), false, 2.5f);
    g.setColour (accentDim().withAlpha (0.25f));
    g.drawRoundedRectangle (bounds.reduced (1.0f), cornerRadius, 1.0f);
}

void BiohazardLookAndFeel::drawPanelRaised (juce::Graphics& g, juce::Rectangle<float> bounds,
                                            float cornerRadius) const
{
    drawBevelEmbossRect (g, bounds, cornerRadius, panel.brighter (0.06f), true, 2.5f);
    drawSpecularHighlight (g, bounds, 0.08f);
    g.setColour (accent().withAlpha (0.2f));
    g.drawRoundedRectangle (bounds.reduced (1.0f), cornerRadius, 1.0f);
}

void BiohazardLookAndFeel::drawBevelEmbossRect (juce::Graphics& g, juce::Rectangle<float> bounds,
                                                 float cornerRadius, juce::Colour face,
                                                 bool raised, float depth) const
{
    const auto hi = face.brighter (raised ? 0.35f : 0.12f);
    const auto lo = face.darker (raised ? 0.55f : 0.35f);
    const auto mid = raised ? face : face.darker (0.12f);

    g.setColour (lo.withAlpha (0.85f));
    g.fillRoundedRectangle (bounds.translated (0.0f, depth * 0.35f), cornerRadius);

    juce::ColourGradient bodyGrad (hi, bounds.getX(), bounds.getY(),
                                   mid.darker (0.18f), bounds.getRight(), bounds.getBottom(), false);
    g.setGradientFill (bodyGrad);
    g.fillRoundedRectangle (bounds, cornerRadius);

    g.setColour (hi.withAlpha (0.75f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), cornerRadius, 1.0f);

    g.setColour (lo.withAlpha (0.9f));
    g.drawLine (bounds.getX() + cornerRadius, bounds.getBottom() - 1.0f,
                bounds.getRight() - cornerRadius, bounds.getBottom() - 1.0f, 1.0f);
    g.drawLine (bounds.getRight() - 1.0f, bounds.getY() + cornerRadius,
                bounds.getRight() - 1.0f, bounds.getBottom() - cornerRadius, 1.0f);

    if (raised)
    {
        g.setColour (hi.withAlpha (0.35f));
        g.drawLine (bounds.getX() + cornerRadius, bounds.getY() + 1.0f,
                    bounds.getRight() - cornerRadius, bounds.getY() + 1.0f, 1.0f);
        g.drawLine (bounds.getX() + 1.0f, bounds.getY() + cornerRadius,
                    bounds.getX() + 1.0f, bounds.getBottom() - cornerRadius, 1.0f);
    }
}

void BiohazardLookAndFeel::drawBevelEmbossEllipse (juce::Graphics& g, juce::Rectangle<float> bounds,
                                                   juce::Colour face, bool raised, float depth) const
{
    const auto hi = face.brighter (raised ? 0.42f : 0.15f);
    const auto lo = face.darker (raised ? 0.55f : 0.35f);

    g.setColour (lo.withAlpha (0.7f));
    g.fillEllipse (bounds.translated (0.0f, depth * 0.45f));

    juce::ColourGradient sphere (hi, bounds.getX(), bounds.getY(),
                                 lo, bounds.getRight(), bounds.getBottom(), false);
    g.setGradientFill (sphere);
    g.fillEllipse (bounds);

    g.setColour (hi.withAlpha (0.55f));
    g.drawEllipse (bounds.reduced (1.0f), 1.0f);

    g.setColour (lo.withAlpha (0.85f));
    g.drawEllipse (bounds, 1.2f);
}

void BiohazardLookAndFeel::drawSpecularHighlight (juce::Graphics& g, juce::Rectangle<float> bounds,
                                                float amount) const
{
    auto highlight = bounds.reduced (bounds.getWidth() * 0.16f, bounds.getHeight() * 0.24f);
    highlight.setHeight (highlight.getHeight() * 0.48f);
    highlight.setY (bounds.getY() + bounds.getHeight() * 0.10f);

    juce::ColourGradient spec (juce::Colours::white.withAlpha (amount),
                               highlight.getCentreX(), highlight.getY(),
                               juce::Colours::transparentWhite,
                               highlight.getCentreX(), highlight.getBottom(), false);
    g.setGradientFill (spec);
    g.fillEllipse (highlight);
}

void BiohazardLookAndFeel::drawDiamondFace (juce::Graphics& g, juce::Rectangle<float> bounds,
                                            bool hover, juce::Colour accentTint) const
{
    const auto  centre = bounds.getCentre();
    const float R = bounds.getWidth() * 0.5f;
    const double t = juce::Time::getMillisecondCounterHiRes() * 0.001;

    // Crystal colour ramp (cool, colourless gem with a faint blue cast).
    const juce::Colour deep  { 0xff8ea6c6 };  // shaded facet
    const juce::Colour mid   { 0xffd4e4f7 };  // lit facet
    const juce::Colour bright { 0xffffffff };  // specular white

    juce::Graphics::ScopedSaveState save (g);
    juce::Path clip;
    clip.addEllipse (bounds);
    g.reduceClipRegion (clip);

    // Base brilliance: radial gradient, bright centre table fading to a deeper rim.
    juce::ColourGradient base (bright, centre.x, centre.y,
                               deep.darker (0.10f), centre.x, bounds.getBottom(), true);
    base.addColour (0.55, mid);
    g.setGradientFill (base);
    g.fillEllipse (bounds);

    // Brilliant-cut facets: triangular kites from the centre to the girdle, each
    // shaded by a fixed light direction so the gem reads as faceted crystal.
    const int   N  = 16;
    const float Lx = -0.55f, Ly = -0.82f;          // light from upper-left
    const float rot = 0.0f;
    for (int i = 0; i < N; ++i)
    {
        const float a0 = rot + juce::MathConstants<float>::twoPi * (float) i / (float) N;
        const float a1 = rot + juce::MathConstants<float>::twoPi * (float) (i + 1) / (float) N;
        const juce::Point<float> p0 (centre.x + std::cos (a0) * R, centre.y + std::sin (a0) * R);
        const juce::Point<float> p1 (centre.x + std::cos (a1) * R, centre.y + std::sin (a1) * R);

        const float am = (a0 + a1) * 0.5f;
        const float d  = std::cos (am) * Lx + std::sin (am) * Ly; // -1..1
        const float b  = 0.5f + 0.5f * d;                          // 0..1 brightness
        auto col = deep.interpolatedWith (bright, juce::jlimit (0.0f, 1.0f, b * b));

        juce::Path tri;
        tri.startNewSubPath (centre);
        tri.lineTo (p0);
        tri.lineTo (p1);
        tri.closeSubPath();
        g.setColour (col.withAlpha (0.55f));
        g.fillPath (tri);
    }

    // Facet edges for crisp crystal definition.
    g.setColour (bright.withAlpha (0.16f));
    for (int i = 0; i < N; ++i)
    {
        const float a = rot + juce::MathConstants<float>::twoPi * (float) i / (float) N;
        g.drawLine (centre.x, centre.y,
                    centre.x + std::cos (a) * R, centre.y + std::sin (a) * R, 0.7f);
    }

    // Central table (flat top) as a bright octagon.
    juce::Path table;
    const float tableR = R * 0.42f;
    for (int k = 0; k < 8; ++k)
    {
        const float a = juce::MathConstants<float>::pi * 0.125f
                      + juce::MathConstants<float>::twoPi * (float) k / 8.0f;
        const juce::Point<float> pt (centre.x + std::cos (a) * tableR, centre.y + std::sin (a) * tableR);
        if (k == 0) table.startNewSubPath (pt); else table.lineTo (pt);
    }
    table.closeSubPath();
    juce::ColourGradient tableGrad (bright, centre.x, centre.y - tableR,
                                    mid.darker (0.05f), centre.x, centre.y + tableR, false);
    g.setGradientFill (tableGrad);
    g.fillPath (table);
    g.setColour (bright.withAlpha (0.5f));
    g.strokePath (table, juce::PathStrokeType (0.8f));

    // Faint accent tint so each tab keeps its identity in the gem.
    g.setColour (accentTint.withAlpha (hover ? 0.16f : 0.10f));
    g.fillEllipse (bounds);

    // Big soft specular highlight (the "shine"), clipped to the gem.
    drawSpecularHighlight (g, bounds, hover ? 0.6f : 0.5f);

    // Travelling glint that sweeps the rim for a living sparkle.
    {
        const float ga = (float) (t * 0.9);
        const juce::Point<float> gp (centre.x + std::cos (ga) * R * 0.82f,
                                     centre.y + std::sin (ga) * R * 0.82f);
        const float gr = R * 0.5f;
        juce::ColourGradient gl (juce::Colours::white.withAlpha (0.55f), gp.x, gp.y,
                                 juce::Colours::transparentWhite, gp.x + gr, gp.y + gr, true);
        g.setGradientFill (gl);
        g.fillEllipse (juce::Rectangle<float> (gr * 2.0f, gr * 2.0f).withCentre (gp));
    }

    // Twinkling star sparkles at a few facet points.
    auto sparkle = [&g] (juce::Point<float> p, float s, float a)
    {
        if (a <= 0.01f) return;
        g.setColour (juce::Colours::white.withAlpha (juce::jlimit (0.0f, 1.0f, a)));
        g.drawLine (p.x - s, p.y, p.x + s, p.y, 1.0f);
        g.drawLine (p.x, p.y - s, p.x, p.y + s, 1.0f);
        const float d = s * 0.55f;
        g.drawLine (p.x - d, p.y - d, p.x + d, p.y + d, 0.6f);
        g.drawLine (p.x - d, p.y + d, p.x + d, p.y - d, 0.6f);
    };
    for (int s = 0; s < 3; ++s)
    {
        const float base2 = (float) s * 2.1f;
        const float a = (float) (0.5 + 0.5 * std::sin (t * 2.4 + base2));
        const float ang = base2 + (float) s * 1.7f - 0.6f;
        const float rr = R * (0.5f + 0.28f * (float) s);
        sparkle ({ centre.x + std::cos (ang) * rr * 0.6f,
                   centre.y + std::sin (ang) * rr * 0.6f },
                 R * 0.22f, a * (hover ? 0.95f : 0.7f));
    }

    g.restoreState();

    // Crystal rim: dark seat + bright inner bevel + faint prismatic accent.
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawEllipse (bounds, 1.4f);
    g.setColour (bright.withAlpha (0.7f));
    g.drawEllipse (bounds.reduced (1.8f), 1.0f);
    g.setColour (accentTint.withAlpha (hover ? 0.5f : 0.28f));
    g.drawEllipse (bounds.reduced (0.6f), 1.0f);
}

void BiohazardLookAndFeel::drawBrushedMetalEllipse (juce::Graphics& g, juce::Rectangle<float> bounds,
                                                   bool raised) const
{
    const auto hi = metalHi;
    const auto lo = metalLo;

    if (raised)
    {
        g.setColour (lo.withAlpha (0.75f));
        g.fillEllipse (bounds.translated (0.0f, 2.0f));
    }

    juce::ColourGradient sphere (hi, bounds.getX(), bounds.getY(),
                                 lo, bounds.getRight(), bounds.getBottom(), false);
    g.setGradientFill (sphere);
    g.fillEllipse (bounds);

    // Brushed radial streaks.
    g.saveState();
    juce::Path clip;
    clip.addEllipse (bounds);
    g.reduceClipRegion (clip);
    const float step = juce::jmax (1.5f, bounds.getHeight() / 14.0f);
    for (float yy = bounds.getY(); yy < bounds.getBottom(); yy += step)
    {
        const float t = (yy - bounds.getY()) / bounds.getHeight();
        g.setColour (juce::Colours::white.withAlpha (0.03f + (1.0f - t) * 0.05f));
        g.drawHorizontalLine ((int) yy, bounds.getX() + 2.0f, bounds.getRight() - 2.0f);
    }
    g.restoreState();

    g.setColour (hi.withAlpha (0.65f));
    g.drawEllipse (bounds.reduced (1.2f), 1.0f);
    g.setColour (lo.withAlpha (0.85f));
    g.drawEllipse (bounds, 1.3f);
}

void BiohazardLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                             float sliderPos, float startAngle, float endAngle,
                                             juce::Slider& slider)
{
    auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (6.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto  centre = bounds.getCentre();
    const float angle  = startAngle + sliderPos * (endAngle - startAngle);
    const bool on = slider.isEnabled();
    const bool hover = slider.isMouseOverOrDragging();
    const auto acc = hover ? accent().brighter (0.15f) : accent();

    const float dialR  = radius * 0.66f;   // solid knob face
    const float trackR = radius * 0.90f;   // value ring radius
    const float trackW = juce::jmax (3.0f, radius * 0.13f);

    // Soft ambient drop shadow under the whole knob.
    for (int i = 3; i >= 1; --i)
    {
        const float spread = (float) i * 1.6f;
        g.setColour (juce::Colours::black.withAlpha (0.16f));
        g.fillEllipse (juce::Rectangle<float> ((dialR + spread) * 2.0f, (dialR + spread) * 2.0f)
                           .withCentre (centre.translated (0.0f, 2.0f)));
    }

    // Recessed track groove (full sweep).
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, trackR, trackR, 0.0f, startAngle, endAngle, true);
    g.setColour (metalLo);
    g.strokePath (track, juce::PathStrokeType (trackW, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    // Value arc with a soft outer glow, then a crisp core. Glow swells on hover.
    if (on)
    {
        juce::Path glow;
        glow.addCentredArc (centre.x, centre.y, trackR, trackR, 0.0f, startAngle, angle, true);
        g.setColour (acc.withAlpha (hover ? 0.40f : 0.25f));
        g.strokePath (glow, juce::PathStrokeType (trackW + (hover ? 8.0f : 5.0f), juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
    }
    juce::Path val;
    val.addCentredArc (centre.x, centre.y, trackR, trackR, 0.0f, startAngle, angle, true);
    g.setColour (on ? acc : metalHi);
    g.strokePath (val, juce::PathStrokeType (trackW, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));

    // Faceted, sparkling diamond knob face.
    auto face = juce::Rectangle<float> (dialR * 2.0f, dialR * 2.0f).withCentre (centre);
    drawDiamondFace (g, face, hover, acc);

    // Indicator dot near the rim (modern, minimal).
    const float dotDist = dialR * 0.66f;
    const float dotR    = juce::jmax (2.4f, radius * 0.075f);
    const juce::Point<float> dot (centre.x + std::cos (angle) * dotDist,
                                  centre.y + std::sin (angle) * dotDist);
    g.setColour (acc.withAlpha (0.35f));
    g.fillEllipse (juce::Rectangle<float> (dotR * 3.0f, dotR * 3.0f).withCentre (dot)); // glow
    g.setColour (on ? acc : metalHi);
    g.fillEllipse (juce::Rectangle<float> (dotR * 2.0f, dotR * 2.0f).withCentre (dot));
}

void BiohazardLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                             float sliderPos, float, float,
                                             juce::Slider::SliderStyle, juce::Slider& slider)
{
    const auto acc = accent();
    const float cy = y + height * 0.5f;

    // Recessed track.
    auto track = juce::Rectangle<float> ((float) x + 2.0f, cy - 3.0f, (float) width - 4.0f, 6.0f);
    g.setColour (metalLo);
    g.fillRoundedRectangle (track, 3.0f);
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawRoundedRectangle (track.reduced (0.5f), 3.0f, 1.0f);

    // Filled portion with subtle glow.
    auto filled = track.withWidth (juce::jmax (4.0f, sliderPos - track.getX()));
    g.setColour (acc.withAlpha (0.30f));
    g.fillRoundedRectangle (filled.expanded (0.0f, 2.0f), 4.0f);
    g.setColour (acc);
    g.fillRoundedRectangle (filled, 3.0f);

    // Clean circular thumb.
    const float r = juce::jmin (9.0f, height * 0.5f);
    auto thumb = juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre ({ sliderPos, cy });
    g.setColour (juce::Colours::black.withAlpha (0.30f));
    g.fillEllipse (thumb.translated (0.0f, 1.5f));
    juce::ColourGradient tg (metal.brighter (0.30f), thumb.getCentreX(), thumb.getY(),
                             metal.darker (0.30f), thumb.getCentreX(), thumb.getBottom(), false);
    g.setGradientFill (tg);
    g.fillEllipse (thumb);
    g.setColour (acc);
    g.drawEllipse (thumb.reduced (1.2f), 1.6f);
    juce::ignoreUnused (slider);
}

void BiohazardLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b,
                                                 const juce::Colour&,
                                                 bool highlighted, bool down)
{
    if (b.getComponentID() == "settings")
        return; // the gear is drawn as a bare icon (no box), matching the padlock

    auto bounds = b.getLocalBounds().toFloat().reduced (1.5f);
    const float radius = 6.0f;  // crisp, consistent corners
    const bool toggled = b.getToggleState();
    const auto acc = accent();

    if (toggled)
    {
        // Active: clean solid accent with a soft glow and crisp top sheen.
        g.setColour (acc.withAlpha (highlighted ? 0.28f : 0.20f));
        g.fillRoundedRectangle (bounds.expanded (1.5f), radius + 1.5f);

        juce::ColourGradient grad (acc.brighter (highlighted ? 0.14f : 0.08f),
                                   bounds.getCentreX(), bounds.getY(),
                                   acc.darker (0.14f), bounds.getCentreX(), bounds.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (bounds, radius);

        g.setColour (juce::Colours::white.withAlpha (0.28f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), radius, 1.0f);
    }
    else
    {
        // Idle: flat panel fill with a crisp hairline border (accent on hover).
        const auto fill = down ? metalLo
                               : (highlighted ? metal.brighter (0.14f) : metal);
        g.setColour (fill);
        g.fillRoundedRectangle (bounds, radius);

        g.setColour (highlighted ? acc.withAlpha (0.60f) : juce::Colours::white.withAlpha (0.10f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), radius, 1.0f);

        // Subtle top edge highlight for a clean, lifted feel.
        if (! down)
        {
            g.setColour (juce::Colours::white.withAlpha (highlighted ? 0.10f : 0.05f));
            g.drawLine (bounds.getX() + radius, bounds.getY() + 1.0f,
                        bounds.getRight() - radius, bounds.getY() + 1.0f, 1.0f);
        }
    }
}

void BiohazardLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b,
                                           bool highlighted, bool down)
{
    juce::ignoreUnused (down);

    // Settings gear: a bare icon (no box) that lights to accent when the knob
    // has modulation assigned (the editor drives textColourOffId).
    if (b.getComponentID() == "settings")
    {
        const auto col = b.findColour (juce::TextButton::textColourOffId);
        g.setColour (highlighted ? col.brighter (0.3f) : col);
        g.setFont (juce::Font (juce::FontOptions (19.0f))); // sized to match the padlock icon
        g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
        return;
    }

    g.setFont (getTextButtonFont (b, b.getHeight()));

    const bool on = b.getToggleState();
    juce::Colour col = on ? bg.darker (0.35f)                       // dark text on accent fill
                          : textCol.withAlpha (b.isEnabled() ? (highlighted ? 1.0f : 0.88f) : 0.45f);
    g.setColour (col);

    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (8, 0),
                      juce::Justification::centred, 1);
}

juce::Font BiohazardLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    const float h = juce::jlimit (12.0f, 14.0f, (float) buttonHeight * 0.44f);
    return juce::Font (juce::FontOptions (h, juce::Font::bold)).withExtraKerningFactor (0.04f);
}

void BiohazardLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b,
                                             bool highlighted, bool down)
{
    auto bounds = b.getLocalBounds().toFloat();
    const bool on = b.getToggleState();
    const auto acc = accent();

    // Per-knob lock: draw a padlock icon (dim = unlocked, accent = locked) so it
    // unmistakably reads as "lock this control" rather than a generic switch.
    if (b.getComponentID() == "lock")
    {
        const auto col = on ? acc : textCol.withAlpha (0.5f);
        const auto c = bounds.getCentre();
        const float bw = 13.0f, bh = 9.0f, sr = 4.2f;
        juce::Rectangle<float> body (c.x - bw * 0.5f, c.y - bh * 0.5f + 2.5f, bw, bh);
        juce::Path shackle;
        shackle.addCentredArc (c.x, body.getY(), sr, sr, 0.0f,
                               -juce::MathConstants<float>::halfPi,
                                juce::MathConstants<float>::halfPi, true);
        g.setColour (col);
        g.strokePath (shackle, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.fillRoundedRectangle (body, 2.0f);
        g.setColour (on ? juce::Colours::black.withAlpha (0.55f) : panel);
        g.fillEllipse (c.x - 1.4f, body.getCentreY() - 1.2f, 2.8f, 2.8f);
        juce::ignoreUnused (highlighted, down);
        return;
    }

    // Modern pill switch.
    const float h = juce::jmin (18.0f, bounds.getHeight() - 2.0f);
    const float w = h * 1.85f;
    auto sw = juce::Rectangle<float> (w, h).withY (bounds.getCentreY() - h * 0.5f).withX (bounds.getX() + 1.0f);

    // Track.
    if (on)
    {
        g.setColour (acc.withAlpha (0.30f));
        g.fillRoundedRectangle (sw.expanded (2.0f), (h + 4.0f) * 0.5f);
        g.setColour (acc);
    }
    else
    {
        g.setColour (metalLo);
    }
    g.fillRoundedRectangle (sw, h * 0.5f);
    g.setColour (juce::Colours::white.withAlpha (on ? 0.18f : 0.06f));
    g.drawRoundedRectangle (sw.reduced (0.5f), h * 0.5f, 1.0f);

    // Knob.
    const float knobR = h - 4.0f;
    const float knobX = on ? sw.getRight() - knobR - 2.0f : sw.getX() + 2.0f;
    auto knob = juce::Rectangle<float> (knobR, knobR).withY (sw.getCentreY() - knobR * 0.5f).withX (knobX);
    g.setColour (juce::Colours::black.withAlpha (0.28f));
    g.fillEllipse (knob.translated (0.0f, 1.0f));
    g.setColour (on ? juce::Colours::white : metalHi.brighter (0.12f));
    g.fillEllipse (knob);
    g.setColour (juce::Colours::black.withAlpha (0.15f));
    g.drawEllipse (knob.reduced (0.5f), 0.8f);

    g.setColour (textCol.withAlpha (on ? 1.0f : 0.80f));
    g.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)).withExtraKerningFactor (0.03f));
    g.drawText (b.getButtonText(),
                bounds.withTrimmedLeft (w + 10.0f).toNearestInt(),
                juce::Justification::centredLeft);
    juce::ignoreUnused (highlighted, down);
}

void BiohazardLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                                         int, int, int, int, juce::ComboBox&)
{
    auto bounds = juce::Rectangle<float> (0, 0, (float) width, (float) height).reduced (1.5f);
    const float radius = 6.0f;

    g.setColour (isButtonDown ? metal.brighter (0.06f) : metal);
    g.fillRoundedRectangle (bounds, radius);
    g.setColour (accent().withAlpha (isButtonDown ? 0.60f : 0.28f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), radius, 1.0f);

    const float cx = (float) width - 15.0f, cy = (float) height * 0.5f;
    juce::Path arrow;
    arrow.addTriangle (cx - 4.5f, cy - 2.5f, cx + 4.5f, cy - 2.5f, cx, cy + 3.5f);
    g.setColour (accent());
    g.fillPath (arrow);
}

void BiohazardLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    auto bounds = juce::Rectangle<float> (0, 0, (float) width, (float) height);
    drawBevelEmbossRect (g, bounds.reduced (1.0f), 6.0f, panel, true, 3.0f);
    g.setColour (accentDim().withAlpha (0.35f));
    g.drawRoundedRectangle (bounds.reduced (1.5f), 6.0f, 1.0f);
}

void BiohazardLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                                              bool isSeparator, bool isActive, bool isHighlighted,
                                              bool isTicked, bool hasSubMenu, const juce::String& text,
                                              const juce::String& shortcutKeyText,
                                              const juce::Drawable* icon, const juce::Colour* textColourToUse)
{
    juce::ignoreUnused (icon, textColourToUse);

    if (isSeparator)
    {
        auto r = area.reduced (8, 0).toFloat();
        g.setColour (textCol.withAlpha (0.12f));
        g.fillRect (r.withSizeKeepingCentre (r.getWidth(), 1.0f));
        return;
    }

    auto r = area.reduced (4, 1).toFloat();
    const auto acc = accent();

    if (isHighlighted && isActive)
    {
        g.setColour (acc.withAlpha (0.20f));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (acc.withAlpha (0.45f));
        g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
    }

    g.setColour (textCol.withAlpha (isActive ? (isHighlighted ? 1.0f : 0.85f) : 0.4f));
    g.setFont (juce::Font (juce::FontOptions (14.0f)));

    auto textArea = area.reduced (14, 0);
    if (isTicked)
    {
        auto tick = textArea.removeFromLeft (18).toFloat();
        g.setColour (acc);
        juce::Path check;
        auto t = tick.withSizeKeepingCentre (10.0f, 10.0f);
        check.startNewSubPath (t.getX(), t.getCentreY());
        check.lineTo (t.getCentreX() - 1.0f, t.getBottom());
        check.lineTo (t.getRight(), t.getY());
        g.strokePath (check, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
        g.setColour (textCol.withAlpha (isHighlighted ? 1.0f : 0.85f));
    }

    g.drawText (text, textArea, juce::Justification::centredLeft, true);

    if (hasSubMenu)
    {
        auto ar = area.toFloat().removeFromRight (22.0f).withSizeKeepingCentre (8.0f, 10.0f);
        juce::Path tri;
        tri.addTriangle (ar.getX(), ar.getY(), ar.getX(), ar.getBottom(), ar.getRight(), ar.getCentreY());
        g.setColour (textCol.withAlpha (0.6f));
        g.fillPath (tri);
    }

    if (shortcutKeyText.isNotEmpty())
    {
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        g.setColour (textCol.withAlpha (0.5f));
        g.drawText (shortcutKeyText, area.reduced (14, 0), juce::Justification::centredRight, true);
    }
}

void BiohazardLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int width, int height,
                                                    juce::TextEditor&)
{
    drawBevelEmbossRect (g, juce::Rectangle<float> (0, 0, (float) width, (float) height).reduced (1.0f),
                         4.0f, metal.darker (0.08f), false, 2.0f);
}

juce::Font BiohazardLookAndFeel::getLabelFont (juce::Label& l)
{
    return l.getFont().withHeight (juce::jmax (12.0f, l.getFont().getHeight()));
}

void BiohazardLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    // TEMP DIAG: figure out where this label sits relative to the window and
    // log it if it lands in the top-left ghost region.
    if (auto* top = label.getTopLevelComponent())
    {
        const auto r = top->getLocalArea (&label, label.getLocalBounds());
        if (r.getX() < 360 && r.getY() < 240)
        {
            const char* parentName = "<none>";
            const char* grandName  = "<none>";
            if (auto* p = label.getParentComponent())
            {
                parentName = typeid (*p).name();
                if (auto* gp = p->getParentComponent())
                    grandName = typeid (*gp).name();
            }
            LLOG ("LABELDIAG text=\"" << label.getText() << "\""
                  << " winBounds=" << r.toString()
                  << " vis=" << (int) label.isVisible()
                  << " showing=" << (int) label.isShowing()
                  << " parent=" << parentName
                  << " grandparent=" << grandName);
        }
    }

    juce::LookAndFeel_V4::drawLabel (g, label);
}

void BiohazardLookAndFeel::ensureGrungeTexture (int diameter)
{
    if (grunge.isValid() && grungeDiameter == diameter)
        return;

    grungeDiameter = diameter;
    grunge = juce::Image (juce::Image::ARGB, diameter, diameter, true);

    juce::Random rng (0x6a3f12);

    for (int yy = 0; yy < diameter; ++yy)
    {
        for (int xx = 0; xx < diameter; ++xx)
        {
            const float n = rng.nextFloat();
            juce::uint8 a = 0;
            juce::Colour col;
            if (n > 0.92f)        { a = (juce::uint8) (40 + rng.nextInt (50)); col = juce::Colours::black.withAlpha (a / 255.0f); }
            else if (n < 0.04f)   { a = (juce::uint8) (20 + rng.nextInt (40)); col = toxic.withAlpha (a / 255.0f); }
            else continue;
            grunge.setPixelAt (xx, yy, col);
        }
    }

    juce::Graphics gg (grunge);
    for (int s = 0; s < diameter / 12; ++s)
    {
        const float x1 = rng.nextFloat() * diameter;
        const float y1 = rng.nextFloat() * diameter;
        const float len = 4.0f + rng.nextFloat() * (diameter * 0.4f);
        const float ang = rng.nextFloat() * juce::MathConstants<float>::twoPi;
        gg.setColour (toxic.withAlpha (0.06f + rng.nextFloat() * 0.06f));
        gg.drawLine (x1, y1,
                     x1 + std::cos (ang) * len, y1 + std::sin (ang) * len,
                     0.6f + rng.nextFloat());
    }
}

} // namespace gf
