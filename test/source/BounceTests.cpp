// Bounce: render the current patch to a file you can drag into a DAW. The point
// is that it sounds like what you were just hearing, so it renders through a
// second processor carrying this one's state.
#include <catch2/catch_test_macros.hpp>
#include "ProcessorFixture.h"
#include <juce_audio_formats/juce_audio_formats.h>

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int    kBlock = 512;

    // Push audio through so the rolling capture has something in it.
    void feed (GrainFreezeProcessor& proc, int blocks)
    {
        juce::AudioBuffer<float> work (2, kBlock);
        juce::MidiBuffer midi;
        for (int b = 0; b < blocks; ++b)
        {
            fx::fillMusical (work, kSr, b * kBlock);
            proc.processBlock (work, midi);
        }
    }
}

TEST_CASE ("Bounce needs material, then renders a real file", "[bounce]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    auto procOwner = std::make_unique<GrainFreezeProcessor>();
    auto& proc = *procOwner;
    proc.setDeterministicSeed (12u);
    proc.prepareToPlay (kSr, kBlock);

    // Nothing has come through yet.
    CHECK_FALSE (proc.canBounce());

    feed (proc, 150);
    CHECK (proc.canBounce());

    auto out = juce::File::getSpecialLocation (juce::File::tempDirectory)
                   .getChildFile ("mkultra-bounce-test.wav");
    out.deleteFile();
    REQUIRE (proc.renderBounce (out, 2.0));
    REQUIRE (out.existsAsFile());

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (out));
    REQUIRE (reader != nullptr);
    CHECK (reader->sampleRate == kSr);
    CHECK (reader->numChannels == 2);

    // It carries the tail past the end of the source material.
    const double seconds = (double) reader->lengthInSamples / reader->sampleRate;
    INFO ("rendered " << seconds << "s");
    CHECK (seconds > 2.0);

    juce::AudioBuffer<float> audio (2, (int) reader->lengthInSamples);
    reader->read (&audio, 0, (int) reader->lengthInSamples, 0, true, true);
    CHECK (fx::allFinite (audio));
    CHECK (fx::rms (audio) > 1.0e-3f);   // not silence
    CHECK (fx::peak (audio) <= 1.0f);    // and not clipped past full scale
    out.deleteFile();
}

TEST_CASE ("Bounce renders the patch that is loaded", "[bounce]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    auto renderWith = [] (const juce::String& preset, const juce::String& name)
    {
        auto procOwner = std::make_unique<GrainFreezeProcessor>();
        auto& proc = *procOwner;
        proc.setDeterministicSeed (12u);
        REQUIRE (proc.presets.loadPreset (preset));
        proc.prepareToPlay (kSr, kBlock);
        feed (proc, 150);

        auto out = juce::File::getSpecialLocation (juce::File::tempDirectory)
                       .getChildFile ("mkultra-bounce-" + name + ".wav");
        out.deleteFile();
        REQUIRE (proc.renderBounce (out, 2.0));

        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (out));
        REQUIRE (reader != nullptr);
        juce::AudioBuffer<float> audio (2, (int) reader->lengthInSamples);
        reader->read (&audio, 0, (int) reader->lengthInSamples, 0, true, true);
        out.deleteFile();
        return audio;
    };

    auto clean = renderWith ("Default - MK Signature", "clean");
    auto wrecked = renderWith ("Destroyed - Meltdown", "wrecked");
    REQUIRE (clean.getNumSamples() > 0);
    REQUIRE (wrecked.getNumSamples() > 0);

    // Two different patches must not render the same audio.
    const int n = juce::jmin (clean.getNumSamples(), wrecked.getNumSamples());
    juce::AudioBuffer<float> diff (2, n);
    for (int c = 0; c < 2; ++c)
    {
        diff.copyFrom (c, 0, wrecked, c, 0, n);
        diff.addFrom (c, 0, clean, c, 0, n, -1.0f);
    }
    const float change = fx::rms (diff) / juce::jmax (1.0e-6f, fx::rms (wrecked));
    INFO ("two patches differ by " << (100.0f * change) << "%");
    CHECK (change > 0.15f);
}
