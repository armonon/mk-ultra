#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace gf
{

// The plugin's visual language, taken from the Sattari Cymatic Generator: a
// near-black ground with a warm bloom off to one side, translucent panels with
// hairline borders and generous radii, cream text on taupe labels, amber as the
// accent with coral for its gradient partner, and cyan reserved for readouts.
// Everything is drawn -- no bitmaps, no bevels, no metal -- so it stays crisp at
// every UI scale.
//
// (The class keeps its original name so every existing reference still resolves;
// the "biohazard" era of brushed chrome and grunge textures is gone.)
class BiohazardLookAndFeel : public juce::LookAndFeel_V4
{
public:
    // ---- Design tokens ----------------------------------------------------
    static const juce::Colour ink;        // page ground            #07070a
    static const juce::Colour stage;      // canvas / visualiser bed #020203
    static const juce::Colour surface;    // panel                   rgba(16,15,20,.82)
    static const juce::Colour surfaceHi;  // control face            white @ 7%
    static const juce::Colour line;       // hairline border         white @ 9%
    static const juce::Colour lineHi;     // control border          white @ 12%
    static const juce::Colour accentA;    // the accent: amber       #f4b35e
    static const juce::Colour accentB;    // gradient partner: coral #ff6b6b
    static const juce::Colour accentC;    // readouts: cyan          #67e8f9
    static const juce::Colour text;       // cream                   #f7f0e8
    static const juce::Colour textDim;    // taupe                   #a99e92
    static const juce::Colour negative;   // negative modulation / danger

    // Radii, which are a big part of the look.
    static constexpr float kPanelRadius   = 22.0f;
    static constexpr float kStageRadius   = 26.0f;
    static constexpr float kControlRadius = 12.0f;

    // ---- Legacy names, re-pointed at the tokens above so existing call sites
    // keep working. Prefer the tokens in new code.
    static const juce::Colour bg;
    static const juce::Colour panel;
    static const juce::Colour metal;
    static const juce::Colour metalHi;
    static const juce::Colour metalLo;
    static const juce::Colour toxic;
    static const juce::Colour toxicDim;
    static const juce::Colour coral;
    static const juce::Colour textCol;
    static const juce::Colour gold;
    static const juce::Colour goldDim;
    static const juce::Colour blendAccent;
    static const juce::Colour iceBlue;
    static const juce::Colour iceBlueDim;

    // One accent for the whole plugin. The enum survives for call compatibility.
    enum class AccentTheme { entropy, mix, prettifier };

    BiohazardLookAndFeel();

    void setAccentTheme (AccentTheme theme) { accentTheme = theme; }
    AccentTheme getAccentTheme() const { return accentTheme; }

    juce::Colour accent() const;
    juce::Colour accentDim() const;

    // ---- Shared drawing helpers -------------------------------------------
    // Uppercase, letter-spaced text -- the single strongest signal of the style.
    // JUCE has no tracking, so glyphs are advanced by hand.
    static void drawTracked (juce::Graphics& g, const juce::String& textToDraw,
                             juce::Rectangle<float> area, juce::Justification just,
                             float tracking = 1.6f);

    // A panel: fill plus a hairline, with the drop shadow the reference uses.
    static void drawPanel (juce::Graphics& g, juce::Rectangle<float> bounds,
                           float radius = kPanelRadius, bool elevated = false);

    // A "stage": the near-black bed a visualiser sits on, with an amber bloom
    // around it. This is the reference's signature move.
    static void drawStage (juce::Graphics& g, juce::Rectangle<float> bounds,
                           float radius = kStageRadius, float glow = 1.0f);

    // Eyebrow: amber, uppercase, widely tracked. The reference's section label.
    static void drawEyebrow (juce::Graphics& g, const juce::String& textToDraw,
                             juce::Rectangle<float> area, juce::Justification just);

    // Soft bloom around a shape, for accent-lit elements.
    static void drawGlow (juce::Graphics& g, const juce::Path& shape,
                          juce::Colour colour, float strength = 1.0f);

    // Kept for source compatibility with older call sites.
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

    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;

    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area,
                            bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool hasSubMenu, const juce::String& text,
                            const juce::String& shortcutKeyText, const juce::Drawable* icon,
                            const juce::Colour* textColourToUse) override;

    void fillTextEditorBackground (juce::Graphics&, int width, int height, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int width, int height, juce::TextEditor&) override;

    juce::Font getLabelFont (juce::Label&) override;
    void drawLabel (juce::Graphics&, juce::Label&) override;

    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int width, int height,
                        bool isScrollbarVertical, int thumbStartPosition, int thumbSize,
                        bool isMouseOver, bool isMouseDown) override;

private:
    AccentTheme accentTheme = AccentTheme::entropy;
};

} // namespace gf
