#include "GrainFreeze/ModMatrix.h"
#include <cmath>

namespace gf
{

ModMatrix::ModMatrix() = default;

void ModMatrix::prepare (double controlRateHz)
{
    controlRate = juce::jmax (1.0, controlRateHz);
    reset();
}

void ModMatrix::reset()
{
    for (auto& s : slots)
    {
        s.lfoPhase = 0.0;
        s.shPhase  = 0.0;
        s.shValue  = 0.0f;
    }
    globalPhase = 0.0;
    globalValue = 0.0f;
    for (auto& o : offsets) o.store (0.0f, std::memory_order_relaxed);
}

float ModMatrix::shapeValue (int shape, double phase)
{
    // phase in [0,1)
    switch (shape)
    {
        case 1: // triangle
            return (float) (phase < 0.5 ? (4.0 * phase - 1.0) : (3.0 - 4.0 * phase));
        case 2: // saw (rising)
            return (float) (2.0 * phase - 1.0);
        case 3: // square
            return phase < 0.5 ? 1.0f : -1.0f;
        case 0: // sine
        default:
            return (float) std::sin (2.0 * juce::MathConstants<double>::pi * phase);
    }
}

void ModMatrix::tick()
{
    const double dt = 1.0 / controlRate;

    // Global source.
    {
        const double rate = (double) globalRate.load();
        globalPhase += rate * dt;
        while (globalPhase >= 1.0) globalPhase -= 1.0;
        globalValue = globalEnabled.load() ? shapeValue (globalShape.load(), globalPhase) : 0.0f;
    }

    for (int i = 0; i < kNumModParams; ++i)
    {
        auto& s = slots[(size_t) i];
        float total = 0.0f;

        // LFO
        const double lfoRate = (double) s.lfoRate.load();
        if (lfoRate > 0.0)
        {
            s.lfoPhase += lfoRate * dt;
            while (s.lfoPhase >= 1.0) s.lfoPhase -= 1.0;
            total += shapeValue (s.lfoShape.load(), s.lfoPhase) * s.lfoDepth.load();
        }

        // Sample & hold wobble: redraw a random value each S&H period.
        const double shRate = (double) s.shRate.load();
        if (shRate > 0.0)
        {
            s.shPhase += shRate * dt;
            if (s.shPhase >= 1.0)
            {
                s.shPhase -= 1.0;
                s.shValue = dist (rng); // new random target in -1..1
            }
            total += s.shValue * s.shDepth.load();
        }

        // Global routing.
        total += globalValue * s.globalAmt.load();

        total = juce::jlimit (-1.0f, 1.0f, total);

        // Unipolar mode: fold the bipolar swing into a positive-only offset.
        if (! s.bipolar.load())
            total = std::abs (total);

        // Skew: bend the response curve. skew>0 emphasises small offsets,
        // skew<0 emphasises large ones. Sign-preserving power curve.
        const float sk = s.skew.load();
        if (std::abs (sk) > 0.001f)
        {
            const float exponent = std::pow (4.0f, -sk); // sk=+1 -> 0.25, sk=-1 -> 4
            const float sign = (total < 0.0f) ? -1.0f : 1.0f;
            total = sign * std::pow (std::abs (total), exponent);
        }

        offsets[(size_t) i].store (juce::jlimit (-1.0f, 1.0f, total), std::memory_order_relaxed);
    }
}

} // namespace gf
