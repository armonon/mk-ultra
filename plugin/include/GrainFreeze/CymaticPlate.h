#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "GrainFreeze/BiohazardLookAndFeel.h"
#include <array>
#include <cmath>
#include <random>
#include <vector>

namespace gf
{

// A real cymatic dish, driven by what is going through the plugin.
//
// This is the Sattari Cymatic Generator's model, ported: a circular dish with
// rigid walls, whose eigenmodes are eta(r, theta) = J_n(alpha_{n,s} * r/R) * cos(n * theta)
// with alpha the zeros of J_n' (Neumann boundary -- the pattern runs right up to
// the rim, as it does in a real dish). Each mode is driven through a resonance
// integral against the incoming spectrum, the driven modes superpose coherently
// into one standing wave, and sand grains do a random walk that settles them onto
// its nodal lines. Change the sound and the figure changes, because the physics
// says it does -- nothing here is an animation on a timer.
class CymaticPlate : public juce::Component,
                     public juce::SettableTooltipClient
{
public:
    using LF = BiohazardLookAndFeel;

    CymaticPlate() { bake(); }

    // Drive the dish from the plugin's own analyser: per-bin energies and the
    // frequency each bin sits at.
    void setSpectrum (const float* energy, const float* hz, int numBins, float peakAmp)
    {
        bandEnergy.assign (energy, energy + numBins);
        bandHz.assign (hz, hz + numBins);
        peak = peakAmp;
    }

    // One step of the model. Called from the editor's timer.
    void advance()
    {
        updateTuning();
        computeExcitations();
        buildField();
        buildGradient();
        moveGrains();
        paintField();
        repaint();
    }

    bool isDriven() const { return response > 0.02f; }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (4.0f);
        LF::drawStage (g, b, 20.0f, 0.55f + response * 0.8f);

        auto plate = b.reduced (10.0f);
        const float side = juce::jmin (plate.getWidth(), plate.getHeight());
        auto disc = juce::Rectangle<float> (side, side).withCentre (plate.getCentre());

        // The dish itself.
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillEllipse (disc);

        // The standing wave behind the sand: bright where the plate is still.
        // Without it a figure this small reads as scattered dots rather than a
        // pattern, and it is the thing the sand is actually settling onto.
        if (fieldImage.isValid() && response > 0.02f)
        {
            juce::Graphics::ScopedSaveState ss (g);
            juce::Path clip;
            clip.addEllipse (disc);
            g.reduceClipRegion (clip);
            g.setOpacity (0.55f);
            g.drawImage (fieldImage, disc, juce::RectanglePlacement::stretchToFit);
            g.setOpacity (1.0f);
        }
        g.setColour (LF::accentA.withAlpha (0.10f + response * 0.16f));
        g.drawEllipse (disc.reduced (1.0f), 1.2f);

        // Grains. Drawn as small additive dots; the ones that have settled onto a
        // nodal line are the bright ones, which is what draws the figure.
        for (const auto& p : grains)
        {
            const float x = disc.getX() + p.x * disc.getWidth();
            const float y = disc.getY() + p.y * disc.getHeight();
            const float a = juce::jlimit (0.0f, 1.0f, p.brightness);
            if (a <= 0.02f)
                continue;
            const float r = 0.7f + a * 1.5f;
            g.setColour ((p.warm ? LF::accentA : LF::text).withAlpha (a * 0.85f));
            g.fillEllipse (x - r, y - r, r * 2.0f, r * 2.0f);
        }

        // What it is showing, in the reference's HUD voice.
        auto hud = b.reduced (12.0f).removeFromBottom (14.0f);
        g.setColour (LF::textDim.withAlpha (0.8f));
        g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
        LF::drawTracked (g, dominantLabel, hud, juce::Justification::centredLeft, 1.4f);
        LF::drawTracked (g, juce::String (juce::roundToInt (dominantHz)) + " HZ",
                         hud, juce::Justification::centredRight, 1.4f);
    }

private:
    // ---- Bessel functions (Numerical Recipes `bessj`) ---------------------
    static double bessj0 (double x)
    {
        const double ax = std::abs (x);
        if (ax < 8.0)
        {
            const double y = x * x;
            const double p1 = 57568490574.0 + y * (-13362590354.0 + y * (651619640.7
                            + y * (-11214424.18 + y * (77392.33017 + y * (-184.9052456)))));
            const double p2 = 57568490411.0 + y * (1029532985.0 + y * (9494680.718
                            + y * (59272.64853 + y * (267.8532712 + y))));
            return p1 / p2;
        }
        const double z = 8.0 / ax, y = z * z, xx = ax - 0.785398164;
        const double p1 = 1.0 + y * (-0.1098628627e-2 + y * (0.2734510407e-4
                        + y * (-0.2073370639e-5 + y * 0.2093887211e-6)));
        const double p2 = -0.1562499995e-1 + y * (0.1430488765e-3 + y * (-0.6911147651e-5
                        + y * (0.7621095161e-6 + y * (-0.934935152e-7))));
        return std::sqrt (0.636619772 / ax) * (std::cos (xx) * p1 - z * std::sin (xx) * p2);
    }

    static double bessj1 (double x)
    {
        const double ax = std::abs (x);
        double ans;
        if (ax < 8.0)
        {
            const double y = x * x;
            const double p1 = x * (72362614232.0 + y * (-7895059235.0 + y * (242396853.1
                            + y * (-2972611.439 + y * (15704.48260 + y * (-30.16036606))))));
            const double p2 = 144725228442.0 + y * (2300535178.0 + y * (18583304.74
                            + y * (99447.43394 + y * (376.9991397 + y))));
            ans = p1 / p2;
        }
        else
        {
            const double z = 8.0 / ax, y = z * z, xx = ax - 2.356194491;
            const double p1 = 1.0 + y * (0.183105e-2 + y * (-0.3516396496e-4
                            + y * (0.2457520174e-5 + y * (-0.240337019e-6))));
            const double p2 = 0.04687499995 + y * (-0.2002690873e-3 + y * (0.8449199096e-5
                            + y * (-0.88228987e-6 + y * 0.105787412e-6)));
            ans = std::sqrt (0.636619772 / ax) * (std::cos (xx) * p1 - z * std::sin (xx) * p2);
            if (x < 0.0) ans = -ans;
        }
        return ans;
    }

    static double besselJ (int n, double x)
    {
        if (n == 0) return bessj0 (x);
        if (n == 1) return bessj1 (x);
        const double ax = std::abs (x);
        if (ax == 0.0) return 0.0;
        double ans;
        const double tox = 2.0 / ax;
        if (ax > (double) n)
        {
            double bjm = bessj0 (ax), bj = bessj1 (ax);
            for (int j = 1; j < n; ++j) { const double bjp = j * tox * bj - bjm; bjm = bj; bj = bjp; }
            ans = bj;
        }
        else
        {
            constexpr double ACC = 40.0, BIGNO = 1.0e10, BIGNI = 1.0e-10;
            const int m = 2 * (int) ((n + (int) std::sqrt (ACC * n)) / 2);
            bool jsum = false;
            double bjp = 0.0, bj = 1.0, sum = 0.0, accum = 0.0;
            for (int j = m; j > 0; --j)
            {
                const double bjm = j * tox * bj - bjp;
                bjp = bj; bj = bjm;
                if (std::abs (bj) > BIGNO) { bj *= BIGNI; bjp *= BIGNI; accum *= BIGNI; sum *= BIGNI; }
                if (jsum) sum += bj;
                jsum = ! jsum;
                if (j == n) accum = bjp;
            }
            sum = 2.0 * sum - bj;
            ans = accum / sum;
        }
        return (x < 0.0 && (n & 1)) ? -ans : ans;
    }

    // J_n'(x): the Neumann boundary condition, which is what makes the pattern
    // reach the rim instead of dying at it.
    static double besselJp (int n, double x)
    {
        if (n == 0) return -besselJ (1, x);
        return 0.5 * (besselJ (n - 1, x) - besselJ (n + 1, x));
    }

    static std::vector<double> besselPrimeZeros (int n, double alphaMax)
    {
        std::vector<double> zeros;
        const double step = 0.05;
        double prevX = 0.3, prev = besselJp (n, prevX);
        for (double x = prevX + step; x <= alphaMax; x += step)
        {
            const double cur = besselJp (n, x);
            // Only a genuine sign change: for large n and small x, J_n underflows
            // to exactly zero over a stretch, and those are not roots.
            if (prev * cur < 0.0)
            {
                double a = prevX, bb = x, fa = prev;
                for (int i = 0; i < 60; ++i)
                {
                    const double mid = 0.5 * (a + bb), fm = besselJp (n, mid);
                    if (fa * fm <= 0.0) bb = mid; else { a = mid; fa = fm; }
                }
                zeros.push_back (0.5 * (a + bb));
            }
            prevX = x; prev = cur;
        }
        return zeros;
    }

    // ---- The dish ---------------------------------------------------------
    static constexpr int kGrid = 72;
    static constexpr int kMaxModes = 22;
    static constexpr int kNumGrains = 1100;

    struct Mode
    {
        std::array<float, kGrid * kGrid> shape {};
        float norm = 1.0f;     // f_k = baseHz * norm
        int   n = 0, s = 1;
    };

    void bake()
    {
        // Every (n, s) up to alphaMax, ordered by alpha, keeping the lowest few.
        struct Spec { int n, s; double alpha; };
        std::vector<Spec> specs;
        for (int n = 0; n <= 10; ++n)
        {
            const auto roots = besselPrimeZeros (n, 16.0);
            for (size_t i = 0; i < roots.size(); ++i)
                specs.push_back ({ n, (int) i + 1, roots[i] });
        }
        std::sort (specs.begin(), specs.end(),
                   [] (const Spec& a, const Spec& b) { return a.alpha < b.alpha; });
        if ((int) specs.size() > kMaxModes)
            specs.resize (kMaxModes);
        if (specs.empty())
            return;

        // Polar coordinates of every cell, and which cells are inside the dish.
        for (int j = 0; j < kGrid; ++j)
        {
            const double cy = ((j + 0.5) / kGrid) * 2.0 - 1.0;
            for (int i = 0; i < kGrid; ++i)
            {
                const double cx = ((i + 0.5) / kGrid) * 2.0 - 1.0;
                const int idx = j * kGrid + i;
                const double r = std::hypot (cx, cy);
                inside[(size_t) idx] = r <= 1.0;
                radius[(size_t) idx] = (float) r;
                theta[(size_t) idx] = (float) std::atan2 (cy, cx);
            }
        }

        const double alphaMin = specs.front().alpha;
        modes.clear();
        for (const auto& sp : specs)
        {
            Mode m;
            m.n = sp.n; m.s = sp.s;
            m.norm = (float) (sp.alpha / alphaMin);
            float peakAbs = 0.0f;
            for (int idx = 0; idx < kGrid * kGrid; ++idx)
            {
                if (! inside[(size_t) idx]) continue;
                const double rad = besselJ (sp.n, sp.alpha * radius[(size_t) idx]);
                const double v = sp.n == 0 ? rad : rad * std::cos (sp.n * theta[(size_t) idx]);
                m.shape[(size_t) idx] = (float) v;
                peakAbs = juce::jmax (peakAbs, std::abs ((float) v));
            }
            if (peakAbs > 0.0f)
                for (auto& v : m.shape) v /= peakAbs;
            modes.push_back (std::move (m));
        }
        excitation.assign (modes.size(), 0.0f);

        grains.resize (kNumGrains);
        std::mt19937 rng (0xCEEDu);
        std::uniform_real_distribution<float> d (0.0f, 1.0f);
        for (auto& gr : grains)
        {
            // Start them scattered inside the dish.
            for (int tries = 0; tries < 8; ++tries)
            {
                gr.x = d (rng); gr.y = d (rng);
                const float dx = gr.x * 2.0f - 1.0f, dy = gr.y * 2.0f - 1.0f;
                if (dx * dx + dy * dy <= 0.98f) break;
            }
            gr.warm = d (rng) > 0.35f;
        }
    }

    // Retune the dish so one of its modes sits exactly on the note being played.
    // Without this the figure is the sum of whichever near-degenerate modes happen
    // to share a frequency -- physically true, but it reads as mud at this size.
    // A real cymatics rig is tuned to the tone for the same reason.
    void updateTuning()
    {
        if (modes.empty() || bandEnergy.empty())
            return;

        // The loudest partial, with parabolic interpolation for sub-bin accuracy.
        int peakBin = -1;
        float peakVal = 0.0f;
        for (size_t b = 1; b + 1 < bandEnergy.size(); ++b)
            if (bandEnergy[b] > peakVal) { peakVal = bandEnergy[b]; peakBin = (int) b; }
        if (peakBin < 0 || peakVal <= 1.0e-7f)
            return;

        const float a = bandEnergy[(size_t) peakBin - 1];
        const float bb = bandEnergy[(size_t) peakBin];
        const float c = bandEnergy[(size_t) peakBin + 1];
        const float denom = a - 2.0f * bb + c;
        const float delta = std::abs (denom) > 1.0e-12f ? 0.5f * (a - c) / denom : 0.0f;
        const float ratio = bandHz[(size_t) juce::jmin ((int) bandHz.size() - 1, peakBin + 1)]
                          / juce::jmax (1.0f, bandHz[(size_t) peakBin]);
        const float pitch = bandHz[(size_t) peakBin] * std::pow (ratio, juce::jlimit (-1.0f, 1.0f, delta));
        if (! (pitch > 20.0f))
            return;

        // Which mode is nearest the note right now; keep that one on it.
        int bestK = 0;
        float bestErr = 1.0e9f;
        for (size_t k = 0; k < modes.size(); ++k)
        {
            const float err = std::abs (std::log ((baseHz * modes[k].norm) / pitch));
            if (err < bestErr) { bestErr = err; bestK = (int) k; }
        }
        const float target = juce::jlimit (8.0f, 4000.0f, pitch / modes[(size_t) bestK].norm);
        baseHz += (target - baseHz) * 0.16f;   // smooth, so figures morph rather than snap
    }

    // Resonance integral: every partial in the spectrum drives every mode through
    // a damped-oscillator response, so a chord is handled by the model itself.
    void computeExcitations()
    {
        response = 0.0f;
        if (modes.empty() || bandEnergy.empty() || peak <= 1.0e-6f)
        {
            std::fill (excitation.begin(), excitation.end(), 0.0f);
            return;
        }

        constexpr float Q = 48.0f;   // sharp enough that the tuned mode dominates
        const float inv = 1.0f / (Q * peak);
        float best = 0.0f; int bestK = 0;
        for (size_t k = 0; k < modes.size(); ++k)
        {
            const float fk = baseHz * modes[k].norm;
            float acc = 0.0f;
            for (size_t b = 0; b < bandEnergy.size(); ++b)
            {
                const float e = bandEnergy[b];
                if (e <= 0.0f) continue;
                const float r = bandHz[b] / fk;
                const float d1 = 1.0f - r * r, d2 = r / Q;
                acc += e / (d1 * d1 + d2 * d2);
            }
            const float a = juce::jlimit (0.0f, 1.0f, std::sqrt (acc) * inv);
            // Smooth so figures dissolve into each other instead of flickering.
            excitation[k] += (a - excitation[k]) * 0.22f;
            if (excitation[k] > best) { best = excitation[k]; bestK = (int) k; }
        }
        response = best;
        dominantHz = baseHz * modes[(size_t) bestK].norm;
        dominantLabel = "(" + juce::String (modes[(size_t) bestK].n) + ","
                            + juce::String (modes[(size_t) bestK].s) + ")";
    }

    // Coherent sum, not RMS: modes driven together superpose into ONE standing
    // wave whose nodes are continuous lines. Summing their squares would zero
    // only where lines cross, drawing a dot lattice instead of a figure.
    void buildField()
    {
        field.fill (0.0f);
        for (size_t k = 0; k < modes.size(); ++k)
        {
            const float a = excitation[k];
            if (a < 0.02f) continue;
            const auto& sh = modes[k].shape;
            for (int i = 0; i < kGrid * kGrid; ++i)
                field[(size_t) i] += a * sh[(size_t) i];
        }
        fieldMax = 1.0e-6f;
        for (int i = 0; i < kGrid * kGrid; ++i)
        {
            const float v = std::abs (field[(size_t) i]);
            field[(size_t) i] = v;
            if (inside[(size_t) i])
                fieldMax = juce::jmax (fieldMax, v);
        }
        // Outside the rim there is no plate. Leaving it at zero makes it look
        // like a perfect node, which drags every grain off the dish and smears
        // them round the edge; mark it as an antinode so they are pushed back in.
        for (int i = 0; i < kGrid * kGrid; ++i)
            if (! inside[(size_t) i])
                field[(size_t) i] = fieldMax * 1.6f;
    }

    // A picture of |u(x)|: dark at antinodes, lit along the nodal lines.
    void paintField()
    {
        if (! fieldImage.isValid())
            fieldImage = juce::Image (juce::Image::ARGB, kGrid, kGrid, true);
        const float inv = 1.0f / juce::jmax (1.0e-6f, fieldMax);
        juce::Image::BitmapData bmp (fieldImage, juce::Image::BitmapData::writeOnly);
        const auto warm = LF::accentA;
        for (int j = 0; j < kGrid; ++j)
            for (int i = 0; i < kGrid; ++i)
            {
                const int idx = j * kGrid + i;
                if (! inside[(size_t) idx]) { bmp.setPixelColour (i, j, juce::Colours::transparentBlack); continue; }
                const float A = juce::jlimit (0.0f, 1.0f, field[(size_t) idx] * inv);
                const float node = 1.0f - A;
                // Sharpen so the nodal line reads as a line, not a gradient.
                const float lit = std::pow (node, 5.0f) * response;
                bmp.setPixelColour (i, j, warm.withAlpha (juce::jlimit (0.0f, 1.0f, lit)));
            }
    }

    void buildGradient()
    {
        for (int j = 0; j < kGrid; ++j)
            for (int i = 0; i < kGrid; ++i)
            {
                const int i0 = juce::jmax (0, i - 1), i1 = juce::jmin (kGrid - 1, i + 1);
                const int j0 = juce::jmax (0, j - 1), j1 = juce::jmin (kGrid - 1, j + 1);
                // Scaled by h*GS, as the reference does, so the settling behaves
                // the same as it does there rather than overshooting every frame.
                constexpr float scale = 0.006f * (float) kGrid;
                gradX[(size_t) (j * kGrid + i)] = (field[(size_t) (j * kGrid + i1)] - field[(size_t) (j * kGrid + i0)]) * scale;
                gradY[(size_t) (j * kGrid + i)] = (field[(size_t) (j1 * kGrid + i)] - field[(size_t) (j0 * kGrid + i)]) * scale;
            }
    }

    float sampleGrid (const std::array<float, kGrid * kGrid>& grid, float x, float y) const
    {
        const float fx = juce::jlimit (0.0f, (float) kGrid - 1.001f, x * kGrid);
        const float fy = juce::jlimit (0.0f, (float) kGrid - 1.001f, y * kGrid);
        const int x0 = (int) fx, y0 = (int) fy;
        const float tx = fx - (float) x0, ty = fy - (float) y0;
        const float a = grid[(size_t) (y0 * kGrid + x0)],     b = grid[(size_t) (y0 * kGrid + x0 + 1)];
        const float c = grid[(size_t) ((y0 + 1) * kGrid + x0)], d = grid[(size_t) ((y0 + 1) * kGrid + x0 + 1)];
        return (a + (b - a) * tx) + ((c + (d - c) * tx) - (a + (b - a) * tx)) * ty;
    }

    // Grains jump where the dish is moving and settle where it is still. This is
    // a particle-transport approximation, not a granular-material solver.
    void moveGrains()
    {
        const float inv = 1.0f / juce::jmax (1.0e-6f, fieldMax);
        const float maxStep = 0.012f * (0.25f + response);
        const float settle = 0.22f;
        std::uniform_real_distribution<float> d (-1.0f, 1.0f);

        for (auto& gr : grains)
        {
            const float A = juce::jlimit (0.0f, 1.0f, sampleGrid (field, gr.x, gr.y) * inv);
            const float step = maxStep * A * response + 0.0009f;
            const float gx = sampleGrid (gradX, gr.x, gr.y) * inv;
            const float gy = sampleGrid (gradY, gr.x, gr.y) * inv;
            const float pull = settle * (0.15f + 0.85f * A) * response;

            gr.x += d (rng) * step - gx * pull;
            gr.y += d (rng) * step - gy * pull;

            // Reflect at the rim so the population stays on the dish.
            float dx = gr.x * 2.0f - 1.0f, dy = gr.y * 2.0f - 1.0f;
            const float r = std::sqrt (dx * dx + dy * dy);
            if (r > 0.97f)
            {
                dx *= 0.94f / r; dy *= 0.94f / r;
                gr.x = dx * 0.5f + 0.5f; gr.y = dy * 0.5f + 0.5f;
            }
            gr.x = juce::jlimit (0.005f, 0.995f, gr.x);
            gr.y = juce::jlimit (0.005f, 0.995f, gr.y);

            // Settled on a node = bright. Contrast scales with how hard the dish
            // is actually being driven, so an unexcited plate reads as flat
            // scatter rather than a ghost of a figure it is not forming.
            const float nodeness = 1.0f - A;
            gr.brightness = juce::jlimit (0.0f, 1.0f,
                                          0.05f + response * nodeness * nodeness * 1.15f);
        }
    }

    struct Grain { float x = 0.5f, y = 0.5f, brightness = 0.0f; bool warm = false; };

    std::vector<Mode>  modes;
    std::vector<float> excitation;
    std::vector<Grain> grains;
    std::vector<float> bandEnergy, bandHz;

    std::array<float, kGrid * kGrid> field {}, gradX {}, gradY {}, radius {}, theta {};
    std::array<bool,  kGrid * kGrid> inside {};

    // A fixed dish: the drive changes, the plate does not. That is why the figure
    // changes with the music instead of just pulsing.
    float baseHz = 58.0f;
    float peak = 0.0f, fieldMax = 1.0f, response = 0.0f, dominantHz = 0.0f;
    juce::String dominantLabel { "(1,1)" };
    juce::Image  fieldImage;
    std::mt19937 rng { 0xC1A0u };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CymaticPlate)
};

} // namespace gf
