#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "GrainFreeze/BiohazardLookAndFeel.h"
#include <functional>
#include <vector>

namespace gf
{

// A lightweight first-run / help overlay. It dims the editor, paints a
// numbered spotlight + caption pointing at one feature at a time, and advances
// on click or Enter / Esc to dismiss. The host editor passes a list of
// (target rectangle in editor coords + caption) pairs and an onDismiss
// callback used to persist "user has seen this".
class TourOverlay : public juce::Component
{
public:
    struct Step { juce::Rectangle<int> target; juce::String caption; };

    void setSteps (std::vector<Step> s) { steps = std::move (s); current = 0; repaint(); }
    void onDismiss (std::function<void()> cb) { onDismissCb = std::move (cb); }

    void mouseDown (const juce::MouseEvent&) override { advance(); }

    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress::escapeKey || k == juce::KeyPress::returnKey) { dismiss(); return true; }
        if (k == juce::KeyPress::spaceKey  || k == juce::KeyPress::rightKey)  { advance();  return true; }
        return false;
    }

    void paint (juce::Graphics& g) override
    {
        if (steps.empty()) return;
        using LF = BiohazardLookAndFeel;
        const auto& s = steps[(size_t) juce::jlimit (0, (int) steps.size() - 1, current)];

        // Dim the rest of the editor.
        g.fillAll (juce::Colours::black.withAlpha (0.62f));

        // Punch a soft "spotlight" around the target -- a slightly larger
        // rounded rect drawn back transparent.
        auto spot = s.target.expanded (8).toFloat();
        g.setColour (juce::Colours::transparentBlack);
        g.fillRoundedRectangle (spot, 8.0f);
        g.setColour (LF::toxic.withAlpha (0.85f));
        g.drawRoundedRectangle (spot, 8.0f, 2.0f);

        // Caption bubble below (or above if no room).
        const int capW = 360, capH = 110;
        int cx = juce::jlimit (10, getWidth() - capW - 10, s.target.getCentreX() - capW / 2);
        int cy = s.target.getBottom() + 18;
        if (cy + capH > getHeight() - 10)
            cy = juce::jmax (10, s.target.getY() - capH - 18);
        juce::Rectangle<int> cap (cx, cy, capW, capH);

        g.setColour (LF::panel.withAlpha (0.96f));
        g.fillRoundedRectangle (cap.toFloat(), 8.0f);
        g.setColour (LF::toxic.withAlpha (0.55f));
        g.drawRoundedRectangle (cap.toFloat(), 8.0f, 1.0f);

        auto text = cap.reduced (14, 12);
        g.setColour (LF::toxic);
        g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)).withExtraKerningFactor (0.18f));
        g.drawText (juce::String (current + 1) + " / " + juce::String ((int) steps.size())
                    + "    MK-ULTRA QUICK TOUR",
                    text.removeFromTop (16), juce::Justification::topLeft);

        g.setColour (LF::textCol);
        g.setFont (juce::Font (juce::FontOptions (13.5f)));
        g.drawFittedText (s.caption, text.removeFromTop (text.getHeight() - 18), juce::Justification::topLeft, 4);

        g.setColour (LF::textCol.withAlpha (0.55f));
        g.setFont (juce::Font (juce::FontOptions (10.0f)));
        g.drawText (current + 1 == (int) steps.size()
                        ? "Click anywhere or press Enter to finish"
                        : "Click anywhere or press → for the next stop  ·  Esc to skip",
                    cap.removeFromBottom (14).reduced (14, 0),
                    juce::Justification::centredRight);
    }

private:
    void advance()
    {
        if (current + 1 >= (int) steps.size()) { dismiss(); return; }
        ++current; repaint();
    }
    void dismiss()
    {
        setVisible (false);
        if (onDismissCb) onDismissCb();
    }

    std::vector<Step> steps;
    int current = 0;
    std::function<void()> onDismissCb;
};

} // namespace gf
