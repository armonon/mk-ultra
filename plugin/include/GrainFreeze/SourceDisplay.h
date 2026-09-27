#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "GrainFreeze/BiohazardLookAndFeel.h"
#include "GrainFreeze/GranularEngine.h"
#include <array>

namespace gf
{

// The material the grain cloud is reading, with the cloud drawn on it: the
// Position anchor, the Spray window around it, and a dot per live grain at the
// point in the sound it is reading. Position and Spray stop being numbers and
// become somewhere you are pointing.
class SourceDisplay : public juce::Component,
                      public juce::SettableTooltipClient
{
public:
    using LF = BiohazardLookAndFeel;
    static constexpr int kBuckets = 320;
    static constexpr int kMaxGrainDots = 96;

    // The non-copyable macro below declares a constructor, which suppresses the
    // implicit default one.
    SourceDisplay() = default;

    void setWaveform (const std::array<float, kBuckets>& peaks) { wave = peaks; }
    void setGrains (const GranularEngine::GrainSnapshot* g, int count)
    {
        grainCount = juce::jlimit (0, kMaxGrainDots, count);
        for (int i = 0; i < grainCount; ++i)
            grains[(size_t) i] = g[i];
    }
    // `position` and `spray` are both 0..1 of the source length.
    void setWindow (float position, float sprayWidth)
    {
        pos = juce::jlimit (0.0f, 1.0f, position);
        spray = juce::jlimit (0.0f, 1.0f, sprayWidth);
    }
    void setSourceName (juce::String name) { sourceName = std::move (name); }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (4.0f);
        LF::drawStage (g, b, 20.0f, 0.65f);
        auto plot = b.reduced (12.0f);
        const float mid = plot.getCentreY();

        // ---- The sound itself.
        juce::Path body;
        body.startNewSubPath (plot.getX(), mid);
        for (int i = 0; i < kBuckets; ++i)
        {
            const float x = plot.getX() + plot.getWidth() * (float) i / (float) (kBuckets - 1);
            body.lineTo (x, mid - wave[(size_t) i] * plot.getHeight() * 0.46f);
        }
        for (int i = kBuckets - 1; i >= 0; --i)
        {
            const float x = plot.getX() + plot.getWidth() * (float) i / (float) (kBuckets - 1);
            body.lineTo (x, mid + wave[(size_t) i] * plot.getHeight() * 0.46f);
        }
        body.closeSubPath();
        g.setColour (LF::text.withAlpha (0.16f));
        g.fillPath (body);

        // ---- The window the grains are drawn from.
        {
            const float half = juce::jmax (0.004f, spray * 0.5f);
            auto band = juce::Rectangle<float> (plot.getX() + plot.getWidth() * (pos - half),
                                                plot.getY(),
                                                plot.getWidth() * half * 2.0f,
                                                plot.getHeight());
            band = band.getIntersection (plot);
            g.setColour (LF::accentA.withAlpha (0.10f));
            g.fillRect (band);
            g.setColour (LF::accentA.withAlpha (0.30f));
            g.drawRect (band, 1.0f);

            const float px = plot.getX() + plot.getWidth() * pos;
            g.setColour (LF::accentA.withAlpha (0.75f));
            g.fillRect (juce::Rectangle<float> (px - 0.75f, plot.getY(), 1.5f, plot.getHeight()));
        }

        // ---- A dot per live grain, where it is reading, fading as it ages.
        for (int i = 0; i < grainCount; ++i)
        {
            const auto& gr = grains[(size_t) i];
            const float x = plot.getX() + plot.getWidth() * juce::jlimit (0.0f, 1.0f, gr.pos01);
            // Age gives a rise and fall, so a grain blooms and goes.
            const float life = std::sin (juce::jlimit (0.0f, 1.0f, gr.age01) * juce::MathConstants<float>::pi);
            const float y = mid + (gr.pan - 0.5f) * plot.getHeight() * 0.7f;
            const float r = 1.6f + 2.6f * life * juce::jlimit (0.0f, 1.0f, gr.amp);
            g.setColour (LF::accentA.withAlpha (0.20f + 0.55f * life));
            g.fillEllipse (x - r, y - r, r * 2.0f, r * 2.0f);
        }

        if (sourceName.isNotEmpty())
        {
            g.setColour (LF::textDim);
            g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
            LF::drawTracked (g, sourceName, plot.removeFromTop (14.0f).withTrimmedLeft (4.0f),
                             juce::Justification::centredLeft, 1.4f);
        }
    }

private:
    std::array<float, kBuckets> wave {};
    std::array<GranularEngine::GrainSnapshot, kMaxGrainDots> grains {};
    int   grainCount = 0;
    float pos = 0.5f, spray = 0.1f;
    juce::String sourceName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SourceDisplay)
};

} // namespace gf
