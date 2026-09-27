#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "GrainFreeze/BiohazardLookAndFeel.h"
#include "GrainFreeze/Modulation/CurveSource.h"
#include <array>
#include <functional>

namespace gf
{

// The drawable modulation curve: a beat grid, the incoming audio behind it, the
// shape you draw on top, and a playhead running the loop. Draw the movement you
// want, then point a Mod Matrix slot at "Curve" and pick what it moves.
//
//   double-click empty space   add a node
//   double-click a node        remove it
//   drag a node                move it (the two ends stay linked, so it loops)
//   drag a segment             bend it
//   right-click                shape presets
class CurveEditor : public juce::Component
{
public:
    using LF = BiohazardLookAndFeel;

    // Called whenever the shape changes, so the host can persist it.
    std::function<void (const CurveSource::Shape&)> onEdit;

    explicit CurveEditor (CurveSource& sourceToEdit) : source (sourceToEdit)
    {
        setWantsKeyboardFocus (false);
        shape = source.getShape();
    }

    void setDivisions (int beatsPerLoop) { divisions = juce::jmax (1, beatsPerLoop); repaint(); }
    void setPlayhead (float phase01)
    {
        if (std::abs (phase01 - playhead) > 0.002f)
        {
            playhead = phase01;
            repaint();
        }
    }
    void setEnvelope (const std::array<float, 256>& env) { envelope = env; }
    void refreshFromSource() { shape = source.getShape(); repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (6.0f);
        LF::drawStage (g, b, LF::kStageRadius);
        auto plot = b.reduced (14.0f);

        // ---- Beat grid.
        for (int i = 0; i <= divisions; ++i)
        {
            const float x = plot.getX() + plot.getWidth() * (float) i / (float) divisions;
            const bool bar = (i % 4) == 0;
            g.setColour (juce::Colours::white.withAlpha (bar ? 0.13f : 0.055f));
            g.fillRect (juce::Rectangle<float> (x - 0.5f, plot.getY(), 1.0f, plot.getHeight()));
        }
        for (int i = 1; i < 4; ++i)
        {
            const float y = plot.getY() + plot.getHeight() * (float) i / 4.0f;
            g.setColour (juce::Colours::white.withAlpha (i == 2 ? 0.13f : 0.05f));
            g.fillRect (juce::Rectangle<float> (plot.getX(), y - 0.5f, plot.getWidth(), 1.0f));
        }

        // ---- Incoming audio behind the curve, mirrored around the centre.
        {
            juce::Path wave;
            const float mid = plot.getCentreY();
            wave.startNewSubPath (plot.getX(), mid);
            for (int i = 0; i < 256; ++i)
            {
                const float x = plot.getX() + plot.getWidth() * (float) i / 255.0f;
                wave.lineTo (x, mid - envelope[(size_t) i] * plot.getHeight() * 0.46f);
            }
            for (int i = 255; i >= 0; --i)
            {
                const float x = plot.getX() + plot.getWidth() * (float) i / 255.0f;
                wave.lineTo (x, mid + envelope[(size_t) i] * plot.getHeight() * 0.46f);
            }
            wave.closeSubPath();
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            g.fillPath (wave);
        }

        // ---- The curve: a filled body under the line, then the line, then bloom.
        juce::Path curvePath;
        const int steps = juce::jmax (64, (int) plot.getWidth());
        for (int i = 0; i <= steps; ++i)
        {
            const float t = (float) i / (float) steps;
            const auto p = toScreen (plot, t, shape.valueAt (t));
            if (i == 0) curvePath.startNewSubPath (p);
            else        curvePath.lineTo (p);
        }

        {
            juce::Path body (curvePath);
            body.lineTo (plot.getRight(), plot.getCentreY());
            body.lineTo (plot.getX(), plot.getCentreY());
            body.closeSubPath();
            juce::ColourGradient fill (LF::accentA.withAlpha (0.24f), plot.getCentreX(), plot.getY(),
                                       LF::accentB.withAlpha (0.03f), plot.getCentreX(), plot.getBottom(), false);
            g.setGradientFill (fill);
            g.fillPath (body);
        }
        LF::drawGlow (g, curvePath, LF::accentA, 1.2f);
        g.setColour (LF::accentA);
        g.strokePath (curvePath, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));

        // ---- Nodes.
        for (int i = 0; i < shape.count; ++i)
        {
            const auto p = toScreen (plot, shape.nodes[(size_t) i].x, shape.nodes[(size_t) i].y);
            const bool hot = (i == hoverNode || i == dragNode);
            const float r = hot ? 6.5f : 5.0f;
            g.setColour (LF::ink);
            g.fillEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (p));
            g.setColour (hot ? LF::text : LF::accentA);
            g.drawEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (p).reduced (1.0f), 2.0f);
        }

        // ---- Playhead + the value it is producing right now.
        {
            const float x = plot.getX() + plot.getWidth() * playhead;
            g.setColour (LF::text.withAlpha (0.35f));
            g.fillRect (juce::Rectangle<float> (x - 0.5f, plot.getY(), 1.0f, plot.getHeight()));
            const auto p = toScreen (plot, playhead, shape.valueAt (playhead));
            juce::Path dot;
            dot.addEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (p));
            LF::drawGlow (g, dot, LF::accentA, 1.6f);
            g.setColour (LF::text);
            g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (p));
        }

        // ---- Hint, only while nothing has been drawn yet.
        if (shape.count <= 3 && ! isMouseOver (true))
        {
            g.setColour (LF::textDim.withAlpha (0.5f));
            g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
            LF::drawTracked (g, "double-click to add a point  -  drag to bend",
                             plot.removeFromBottom (16.0f), juce::Justification::centred, 1.2f);
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const int was = hoverNode;
        hoverNode = nodeAt (e.position);
        if (was != hoverNode)
            repaint();
        setMouseCursor (hoverNode >= 0 ? juce::MouseCursor::DraggingHandCursor
                                       : juce::MouseCursor::NormalCursor);
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        hoverNode = -1;
        repaint();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
        {
            showShapeMenu();
            return;
        }
        dragNode = nodeAt (e.position);
        dragTension = dragNode < 0;
        if (dragTension)
            tensionSegment = segmentAt (e.position);
        dragStartY = e.position.y;
        if (dragTension && tensionSegment >= 0)
            tensionStart = shape.nodes[(size_t) tensionSegment].tension;
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        auto plot = getLocalBounds().toFloat().reduced (10.0f);
        if (dragNode >= 0)
        {
            auto& n = shape.nodes[(size_t) dragNode];
            const bool isFirst = dragNode == 0;
            const bool isLast  = dragNode == shape.count - 1;
            if (! isFirst && ! isLast)
            {
                const float lo = shape.nodes[(size_t) (dragNode - 1)].x + 0.004f;
                const float hi = shape.nodes[(size_t) (dragNode + 1)].x - 0.004f;
                n.x = juce::jlimit (lo, hi, (e.position.x - plot.getX()) / plot.getWidth());
            }
            n.y = juce::jlimit (-1.0f, 1.0f,
                                1.0f - 2.0f * (e.position.y - plot.getY()) / plot.getHeight());
            if (e.mods.isShiftDown())
                n.y = juce::roundToInt (n.y * 4.0f) / 4.0f;   // snap to quarters
            // Keep the loop seamless: the two ends share a value.
            if (isFirst) shape.nodes[(size_t) (shape.count - 1)].y = n.y;
            if (isLast)  shape.nodes[0].y = n.y;
            publish();
        }
        else if (dragTension && tensionSegment >= 0)
        {
            const float delta = (dragStartY - e.position.y) / 60.0f;
            shape.nodes[(size_t) tensionSegment].tension = juce::jlimit (-1.0f, 1.0f, tensionStart + delta);
            publish();
        }
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        dragNode = -1;
        dragTension = false;
        tensionSegment = -1;
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const int hit = nodeAt (e.position);
        if (hit > 0 && hit < shape.count - 1)      // the two ends always stay
        {
            for (int i = hit; i < shape.count - 1; ++i)
                shape.nodes[(size_t) i] = shape.nodes[(size_t) (i + 1)];
            --shape.count;
            publish();
            return;
        }
        if (hit >= 0 || shape.count >= CurveSource::kMaxNodes)
            return;

        auto plot = getLocalBounds().toFloat().reduced (10.0f);
        const float x = juce::jlimit (0.0f, 1.0f, (e.position.x - plot.getX()) / plot.getWidth());
        const float y = juce::jlimit (-1.0f, 1.0f,
                                      1.0f - 2.0f * (e.position.y - plot.getY()) / plot.getHeight());
        int insert = 1;
        while (insert < shape.count && shape.nodes[(size_t) insert].x < x)
            ++insert;
        for (int i = shape.count; i > insert; --i)
            shape.nodes[(size_t) i] = shape.nodes[(size_t) (i - 1)];
        shape.nodes[(size_t) insert] = { x, y, 0.0f };
        ++shape.count;
        publish();
    }

private:
    juce::Point<float> toScreen (juce::Rectangle<float> plot, float x, float y) const
    {
        return { plot.getX() + plot.getWidth() * x,
                 plot.getY() + plot.getHeight() * (1.0f - y) * 0.5f };
    }

    int nodeAt (juce::Point<float> p) const
    {
        auto plot = getLocalBounds().toFloat().reduced (10.0f);
        for (int i = 0; i < shape.count; ++i)
            if (toScreen (plot, shape.nodes[(size_t) i].x, shape.nodes[(size_t) i].y).getDistanceFrom (p) < 11.0f)
                return i;
        return -1;
    }

    int segmentAt (juce::Point<float> p) const
    {
        auto plot = getLocalBounds().toFloat().reduced (10.0f);
        const float x = (p.x - plot.getX()) / juce::jmax (1.0f, plot.getWidth());
        for (int i = 0; i < shape.count - 1; ++i)
            if (x >= shape.nodes[(size_t) i].x && x <= shape.nodes[(size_t) (i + 1)].x)
                return i;
        return -1;
    }

    void publish()
    {
        source.setShape (shape);
        if (onEdit) onEdit (shape);
        repaint();
    }

    void setPreset (int which)
    {
        CurveSource::Shape s;
        switch (which)
        {
            case 0: // Duck: the sidechain pump
                s.count = 3;
                s.nodes[0] = { 0.0f, 1.0f, 0.5f };
                s.nodes[1] = { 0.6f, -1.0f, 0.0f };
                s.nodes[2] = { 1.0f, 1.0f, 0.0f };
                break;
            case 1: // Gate: hard on/off eighths
                s.count = 5;
                s.nodes[0] = { 0.0f, 1.0f, 1.0f };
                s.nodes[1] = { 0.25f, 1.0f, -1.0f };
                s.nodes[2] = { 0.5f, -1.0f, 1.0f };
                s.nodes[3] = { 0.75f, -1.0f, -1.0f };
                s.nodes[4] = { 1.0f, 1.0f, 0.0f };
                break;
            case 2: // Swell
                s.count = 3;
                s.nodes[0] = { 0.0f, -1.0f, 0.6f };
                s.nodes[1] = { 0.75f, 1.0f, -0.6f };
                s.nodes[2] = { 1.0f, -1.0f, 0.0f };
                break;
            case 3: // Stairs
                s.count = 9;
                for (int i = 0; i < 8; ++i)
                    s.nodes[(size_t) i] = { (float) i / 8.0f, 1.0f - 2.0f * (float) (i % 4) / 3.0f, 1.0f };
                s.nodes[8] = { 1.0f, s.nodes[0].y, 0.0f };
                break;
            default: // Flat
                s.count = 2;
                s.nodes[0] = { 0.0f, 0.0f, 0.0f };
                s.nodes[1] = { 1.0f, 0.0f, 0.0f };
                break;
        }
        shape = s;
        publish();
    }

    void showShapeMenu()
    {
        juce::PopupMenu m;
        m.addSectionHeader ("Shapes");
        m.addItem (1, "Duck");
        m.addItem (2, "Gate");
        m.addItem (3, "Swell");
        m.addItem (4, "Stairs");
        m.addItem (5, "Flat");
        m.addSeparator();
        m.addItem (6, "Invert");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                         [this] (int r)
                         {
                             if (r >= 1 && r <= 5) { setPreset (r - 1); return; }
                             if (r == 6)
                             {
                                 for (int i = 0; i < shape.count; ++i)
                                     shape.nodes[(size_t) i].y = -shape.nodes[(size_t) i].y;
                                 publish();
                             }
                         });
    }

    CurveSource& source;
    CurveSource::Shape shape;
    std::array<float, 256> envelope {};
    int   divisions = 16;
    float playhead = 0.0f;
    int   hoverNode = -1, dragNode = -1, tensionSegment = -1;
    bool  dragTension = false;
    float dragStartY = 0.0f, tensionStart = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CurveEditor)
};

} // namespace gf
