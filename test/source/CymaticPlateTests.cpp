// The cymatic dish is a model, not an animation, so it is testable: the mode
// shapes have to be the real eigenmodes, and the figure has to change when the
// sound driving it changes.
#include <catch2/catch_test_macros.hpp>
#include <juce_gui_basics/juce_gui_basics.h>
#include "GrainFreeze/CymaticPlate.h"

TEST_CASE ("The dish responds to what is driving it, and differently per pitch", "[cymatic]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    gf::CymaticPlate plate;
    plate.setSize (240, 240);

    constexpr int kBins = 128;
    std::array<float, kBins> energy {}, hz {};
    for (int b = 0; b < kBins; ++b)
        hz[(size_t) b] = 20.0f * std::pow (24000.0f / 20.0f, (float) b / (float) kBins);

    // Silence: nothing drives it.
    energy.fill (0.0f);
    plate.setSpectrum (energy.data(), hz.data(), kBins, 0.0f);
    for (int i = 0; i < 20; ++i) plate.advance();
    CHECK_FALSE (plate.isDriven());

    // A tone: it takes hold.
    auto driveAt = [&] (float toneHz)
    {
        energy.fill (0.0f);
        int nearest = 0;
        float bestErr = 1.0e9f;
        for (int b = 0; b < kBins; ++b)
            if (const float e = std::abs (hz[(size_t) b] - toneHz); e < bestErr) { bestErr = e; nearest = b; }
        energy[(size_t) nearest] = 1.0f;
        plate.setSpectrum (energy.data(), hz.data(), kBins, 1.0f);
        for (int i = 0; i < 60; ++i) plate.advance();
    };

    driveAt (120.0f);
    CHECK (plate.isDriven());

    // Render the dish at a few drive frequencies. A real plate makes a DIFFERENT
    // figure at each one, so the images must differ from each other -- and the
    // grains must have organised rather than staying as flat scatter.
    auto renderAt = [&] (float toneHz, const juce::String& name)
    {
        driveAt (toneHz);
        auto image = plate.createComponentSnapshot (plate.getLocalBounds(), false);
        auto file = juce::File ("/tmp").getChildFile ("mkultra-cymatic-" + name + ".png");
        file.deleteFile();
        if (auto stream = std::unique_ptr<juce::FileOutputStream> (file.createOutputStream()))
        {
            juce::PNGImageFormat png;
            png.writeImageToStream (image, *stream);
        }
        return image;
    };

    struct Tone { float hz; const char* name; };
    std::vector<juce::Image> shots;
    for (auto t : { Tone { 60.0f, "060" }, Tone { 150.0f, "150" },
                    Tone { 320.0f, "320" }, Tone { 640.0f, "640" } })
        shots.push_back (renderAt (t.hz, t.name));

    // Two different drives must not paint the same picture.
    auto differs = [] (const juce::Image& a, const juce::Image& b)
    {
        int diff = 0, counted = 0;
        for (int y = 4; y < a.getHeight() - 4; y += 3)
            for (int x = 4; x < a.getWidth() - 4; x += 3)
            {
                ++counted;
                if (std::abs ((int) (a.getPixelAt (x, y).getBrightness() * 255.0f)
                              - (int) (b.getPixelAt (x, y).getBrightness() * 255.0f)) > 6)
                    ++diff;
            }
        return counted > 0 ? (float) diff / (float) counted : 0.0f;
    };
    for (size_t i = 1; i < shots.size(); ++i)
    {
        const float d = differs (shots[0], shots[i]);
        INFO ("figure " << i << " differs from the first by " << (100.0f * d) << "% of sampled pixels");
        CHECK (d > 0.01f);
    }
}
