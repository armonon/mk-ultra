#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace gf
{

// Toxic-green biohazard theme. Centralizes the palette and overrides JUCE's
// drawing for rotary sliders, linear sliders, buttons, and combo boxes so the
// whole plugin shares one industrial, radioactive look.
class BiohazardLookAndFeel : public juce::LookAndFeel_V4
{
public:
    // Palette, pulled from the reference image.
    static const juce::Colour bg;        // near-black background
    static const juce::Colour panel;     // slightly lifted panel surface
    static const juce::Colour metal;     // brushed chrome
    static const juce::Colour metalHi;   // highlight edge
    static const juce::Colour metalLo;   // shadow edge
    static const juce::Colour toxic;      // glowing toxic green
    static const juce::Colour toxicDim;  // dimmer green for tracks
    static const juce::Colour coral;      // negative-mod accent
    static const juce::Colour textCol;   // pale green-white text
    static const juce::Colour gold;     // (legacy) warm tone, used by the mix blend
    static const juce::Colour goldDim;
    static const juce::Colour blendAccent; // mix-tab bridge tone
    static const juce::Colour iceBlue;    // prettifier accent (diamond blue)
    static const juce::Colour iceBlueDim;

    enum class AccentTheme { entropy, mix, prettifier };

    BiohazardLookAndFeel();

    void setAccentTheme (AccentTheme theme) { accentTheme = theme; }
    AccentTheme getAccentTheme() const { return accentTheme; }

    juce::Colour accent() const;
    juce::Colour accentDim() const;

    void drawPanelInset (juce::Graphics& g, juce::Rectangle<float> bounds, float cornerRadius = 5.0f) const;
    void drawPanelRaised (juce::Graphics& g, juce::Rectangle<float> bounds, float cornerRadius = 5.0f) const;

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;

    void drawButtonText (juce::Graphics&, juce::TextButton&,
                         bool shouldDrawButtonAsHighlighted,
                         bool shouldDrawButtonAsDown) override;

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox&) override;

    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;

    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area,
                            bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool hasSubMenu, const juce::String& text,
                            const juce::String& shortcutKeyText, const juce::Drawable* icon,
                            const juce::Colour* textColourToUse) override;

    void fillTextEditorBackground (juce::Graphics&, int width, int height, juce::TextEditor&) override;

    juce::Font getLabelFont (juce::Label&) override;

    void drawLabel (juce::Graphics&, juce::Label&) override; // TEMP DIAG

private:
    void drawBevelEmbossRect (juce::Graphics& g, juce::Rectangle<float> bounds, float cornerRadius,
                              juce::Colour face, bool raised, float depth) const;
    void drawBevelEmbossEllipse (juce::Graphics& g, juce::Rectangle<float> bounds,
                                 juce::Colour face, bool raised, float depth) const;
    void drawBrushedMetalEllipse (juce::Graphics& g, juce::Rectangle<float> bounds, bool raised) const;
    void drawSpecularHighlight (juce::Graphics& g, juce::Rectangle<float> bounds, float amount) const;
    void drawDiamondFace (juce::Graphics& g, juce::Rectangle<float> bounds, bool hover,
                          juce::Colour accentTint) const;

    AccentTheme accentTheme = AccentTheme::entropy;
    // Procedural weathered-metal texture, generated once and composited onto
    // knob bodies (clipped to the circle) for an industrial, corroded look.
    void ensureGrungeTexture (int diameter);
    juce::Image grunge;
    int grungeDiameter = 0;
};

} // namespace gf
