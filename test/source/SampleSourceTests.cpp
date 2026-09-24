// The granular Sample source: a dropped audio file becomes what the grain cloud
// chews on, instead of the live input. The load path also has to resample the
// file to the host's rate, survive a session save, and stay out of the way while
// the source is set to Live.
#include <catch2/catch_test_macros.hpp>
#include "ProcessorFixture.h"
#include <juce_audio_formats/juce_audio_formats.h>

namespace
{
    constexpr double kSr = 48000.0;
    constexpr int    kBlock = 512;

    // A sine at `fileRate` written to a real .wav, so the loader is exercised for
    // real (including the resample when fileRate != the processor's rate).
    juce::File writeTestWav (const juce::String& name, double fileRate, double seconds,
                             double freq = 1000.0)
    {
        const int len = (int) (fileRate * seconds);
        juce::AudioBuffer<float> buf (2, len);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < len; ++i)
                buf.setSample (c, i, 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi
                                                              * freq * i / fileRate));

        auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile (name + ".wav");
        file.deleteFile();
        juce::WavAudioFormat wav;
        if (auto stream = std::unique_ptr<juce::FileOutputStream> (file.createOutputStream()))
        {
            if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (
                    wav.createWriterFor (stream.get(), fileRate, 2, 16, {}, 0)))
            {
                stream.release();          // the writer owns it now
                writer->writeFromAudioSampleBuffer (buf, 0, len);
            }
        }
        return file;
    }

    // Run silence through the plugin and report the peak it produces. With a
    // silent input, anything audible can only have come from the sample.
    float peakFromSilence (GrainFreezeProcessor& proc, int blocks = 200)
    {
        juce::AudioBuffer<float> work (2, kBlock);
        juce::MidiBuffer midi;
        float peak = 0.0f;
        for (int b = 0; b < blocks; ++b)
        {
            work.clear();
            proc.processBlock (work, midi);
            if (b >= blocks / 2)
                peak = juce::jmax (peak, work.getMagnitude (0, 0, kBlock),
                                         work.getMagnitude (1, 0, kBlock));
        }
        return peak;
    }
}

TEST_CASE ("A loaded sample becomes the granular source", "[sample]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    auto file = writeTestWav ("mkultra-sample-source", 48000.0, 2.0);
    REQUIRE (file.existsAsFile());

    auto procOwner = std::make_unique<GrainFreezeProcessor>();
    auto& proc = *procOwner;
    proc.setDeterministicSeed (3u);
    proc.prepareToPlay (kSr, kBlock);

    // Nothing loaded: silence in, silence out.
    CHECK_FALSE (proc.hasGranularSample());
    CHECK (peakFromSilence (proc) < 1.0e-4f);

    REQUIRE (proc.loadGranularSample (file));
    CHECK (proc.hasGranularSample());
    CHECK (proc.getGranularSamplePath() == file.getFullPathName());
    // Loading a file switches the source over to it.
    CHECK (proc.apvts.getRawParameterValue ("grainSource")->load() > 0.5f);
    CHECK (proc.getGranularSampleSeconds() > 1.9);
    CHECK (proc.getGranularSampleSeconds() < 2.1);

    // Now silence in produces sound: the grains are reading the file.
    const float fromSample = peakFromSilence (proc);
    INFO ("peak from a silent input with a sample loaded: " << fromSample);
    CHECK (fromSample > 1.0e-3f);

    // Switching back to Live goes quiet again -- the sample is held, not played.
    // (Beauty & Space rings on for a while, so this is a decay, not a hard stop.)
    fx::setParam (proc, "grainSource", 0.0f);
    const float onLive = peakFromSilence (proc, 600);
    INFO ("peak on Live with a silent input: " << onLive);
    CHECK (onLive < fromSample * 0.02f);

    // ...and switching back to Sample brings it back.
    fx::setParam (proc, "grainSource", 1.0f);
    CHECK (peakFromSilence (proc) > fromSample * 0.25f);

    proc.clearGranularSample();
    CHECK_FALSE (proc.hasGranularSample());
    CHECK (proc.apvts.getRawParameterValue ("grainSource")->load() < 0.5f);
    // The grains stop immediately, but Beauty & Space keeps ringing for a while,
    // so what matters is that it decays away rather than hitting zero at once.
    const float afterClear = peakFromSilence (proc, 600);
    INFO ("peak after clearing, once the reverb tail has run down: " << afterClear);
    CHECK (afterClear < fromSample * 0.02f);
    file.deleteFile();
}

TEST_CASE ("A sample recorded at another rate keeps its pitch and length", "[sample]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    // 44.1k file into a 48k host: a wrong resample ratio shows up as both the
    // wrong duration and the wrong pitch.
    auto file = writeTestWav ("mkultra-sample-rate", 44100.0, 2.0, 1000.0);
    REQUIRE (file.existsAsFile());

    auto procOwner = std::make_unique<GrainFreezeProcessor>();
    auto& proc = *procOwner;
    proc.prepareToPlay (kSr, kBlock);
    REQUIRE (proc.loadGranularSample (file));

    INFO ("sample length at 48k: " << proc.getGranularSampleSeconds() << "s");
    CHECK (proc.getGranularSampleSeconds() > 1.95);
    CHECK (proc.getGranularSampleSeconds() < 2.05);

    // Re-preparing at the file's own rate must not change its duration either.
    proc.prepareToPlay (44100.0, kBlock);
    INFO ("sample length at 44.1k: " << proc.getGranularSampleSeconds() << "s");
    CHECK (proc.getGranularSampleSeconds() > 1.95);
    CHECK (proc.getGranularSampleSeconds() < 2.05);
    file.deleteFile();
}

TEST_CASE ("The sample path survives a session save", "[sample]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    auto file = writeTestWav ("mkultra-sample-state", 48000.0, 1.0);
    REQUIRE (file.existsAsFile());

    auto aOwner = std::make_unique<GrainFreezeProcessor>();
    auto& a = *aOwner;
    a.prepareToPlay (kSr, kBlock);
    REQUIRE (a.loadGranularSample (file));

    juce::MemoryBlock blob;
    a.getStateInformation (blob);

    auto bOwner = std::make_unique<GrainFreezeProcessor>();
    auto& b = *bOwner;
    b.setStateInformation (blob.getData(), (int) blob.getSize());
    CHECK (b.getGranularSamplePath() == file.getFullPathName());
    CHECK (b.apvts.getRawParameterValue ("grainSource")->load() > 0.5f);
    file.deleteFile();
}
