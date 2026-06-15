#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace gf
{

// Builds a biohazard symbol as a vector Path, centred in the given bounds.
// Approximates the ISO symbol: a central ring with three "blade" lobes at
// 120-degree spacing, each a thick arc with an inner notch. Drawn as a
// filled path so it can be used as a glowing watermark behind the UI.
inline juce::Path makeBiohazardPath (juce::Rectangle<float> bounds)
{
    const auto  c = bounds.getCentre();
    const float R = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float pi = juce::MathConstants<float>::pi;

    juce::Path p;

    // Central ring.
    const float coreOuter = R * 0.18f;
    const float coreInner = R * 0.09f;
    juce::Path core;
    core.addEllipse (c.x - coreOuter, c.y - coreOuter, coreOuter * 2.0f, coreOuter * 2.0f);
    core.addEllipse (c.x - coreInner, c.y - coreInner, coreInner * 2.0f, coreInner * 2.0f);
    core.setUsingNonZeroWinding (false); // subtract inner -> ring
    p.addPath (core);

    // Three lobes. Each lobe is a ring (annulus) centred away from c, with a
    // wedge cut toward the centre to form the classic open-blade shape.
    const float lobeDist  = R * 0.52f;
    const float lobeOuter = R * 0.40f;
    const float lobeInner = R * 0.22f;

    for (int i = 0; i < 3; ++i)
    {
        const float ang = -pi * 0.5f + (float) i * (2.0f * pi / 3.0f);
        const float lx = c.x + std::cos (ang) * lobeDist;
        const float ly = c.y + std::sin (ang) * lobeDist;

        juce::Path lobe;
        lobe.addEllipse (lx - lobeOuter, ly - lobeOuter, lobeOuter * 2.0f, lobeOuter * 2.0f);
        lobe.addEllipse (lx - lobeInner, ly - lobeInner, lobeInner * 2.0f, lobeInner * 2.0f);
        lobe.setUsingNonZeroWinding (false); // annulus

        // Wedge pointing back toward the centre, to open the blade.
        juce::Path wedge;
        const float wHalf = pi * 0.16f;
        const float back  = ang + pi; // direction from lobe toward centre
        const float reach = R * 0.95f;
        wedge.startNewSubPath (lx, ly);
        wedge.lineTo (lx + std::cos (back - wHalf) * reach, ly + std::sin (back - wHalf) * reach);
        wedge.lineTo (lx + std::cos (back + wHalf) * reach, ly + std::sin (back + wHalf) * reach);
        wedge.closeSubPath();

        // Subtract the wedge from the annulus.
        juce::Path lobeMinusWedge = lobe;
        lobeMinusWedge.addPath (wedge);
        lobeMinusWedge.setUsingNonZeroWinding (false);

        p.addPath (lobeMinusWedge);
    }

    return p;
}

} // namespace gf
