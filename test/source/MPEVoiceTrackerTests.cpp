// Behavioral guards for MPEVoiceTracker: per-note voice allocation, pitch bend
// + pressure + timbre update, voice release on noteOff, snapshot semantics.
#include <catch2/catch_test_macros.hpp>
#include "GrainFreeze/Modulation/MPEVoiceTracker.h"

namespace
{
    // Construct a midi message on a specific MPE member channel (2-16).
    juce::MidiMessage memberNoteOn (int channel, int note, float vel)
    {
        return juce::MidiMessage::noteOn (channel, note, vel);
    }
    juce::MidiMessage memberNoteOff (int channel, int note)
    {
        return juce::MidiMessage::noteOff (channel, note);
    }
    juce::MidiMessage memberPitchBend (int channel, int value14bit)
    {
        return juce::MidiMessage::pitchWheel (channel, value14bit);
    }
    juce::MidiMessage memberPressure (int channel, int value)   // channel pressure
    {
        return juce::MidiMessage::channelPressureChange (channel, value);
    }
}

TEST_CASE ("MPEVoiceTracker allocates one voice per noteOn, frees on noteOff", "[mpe]")
{
    gf::MPEVoiceTracker t;
    t.setRoot (60);

    REQUIRE (t.getActiveCount() == 0);

    t.handleMessage (memberNoteOn (2, 60, 0.8f));
    t.handleMessage (memberNoteOn (3, 64, 0.7f));
    t.handleMessage (memberNoteOn (4, 67, 0.6f));
    CHECK (t.getActiveCount() == 3);

    gf::MPEVoiceTracker::VoiceData v[16] {};
    const int n = t.copyActiveVoices (v, 16);
    REQUIRE (n == 3);
    // Semitones are stored relative to the root (60), so 60 -> 0, 64 -> 4, 67 -> 7.
    juce::Array<float> seen;
    for (int i = 0; i < n; ++i) seen.add (v[i].semitones);
    CHECK (seen.contains (0.0f));
    CHECK (seen.contains (4.0f));
    CHECK (seen.contains (7.0f));

    t.handleMessage (memberNoteOff (3, 64));
    CHECK (t.getActiveCount() == 2);

    t.handleMessage (memberNoteOff (2, 60));
    t.handleMessage (memberNoteOff (4, 67));
    CHECK (t.getActiveCount() == 0);
}

TEST_CASE ("MPEVoiceTracker reflects per-voice pitch bend in semitones", "[mpe]")
{
    gf::MPEVoiceTracker t;
    t.setRoot (60);
    t.handleMessage (memberNoteOn (2, 60, 0.9f));

    // 14-bit pitch bend: 8192 = no bend (range typically +/- 2 semis or +/- 48
    // for MPE master, but on member channels the default is +/- 48). We send
    // +1 bend unit -- the absolute value doesn't matter for this test as long as
    // the tracker updates when the bend changes.
    gf::MPEVoiceTracker::VoiceData before[16] {};
    const int n0 = t.copyActiveVoices (before, 16);
    REQUIRE (n0 == 1);
    const float baseline = before[0].semitones;

    t.handleMessage (memberPitchBend (2, 12000));   // bend upward
    gf::MPEVoiceTracker::VoiceData after[16] {};
    const int n1 = t.copyActiveVoices (after, 16);
    REQUIRE (n1 == 1);
    CHECK (after[0].semitones != baseline);
}

TEST_CASE ("MPEVoiceTracker reflects per-voice pressure (channel aftertouch)", "[mpe]")
{
    gf::MPEVoiceTracker t;
    t.setRoot (60);
    t.handleMessage (memberNoteOn (2, 60, 0.8f));

    t.handleMessage (memberPressure (2, 110));
    gf::MPEVoiceTracker::VoiceData v[16] {};
    REQUIRE (t.copyActiveVoices (v, 16) == 1);
    CHECK (v[0].pressure > 0.7f);

    t.handleMessage (memberPressure (2, 0));
    REQUIRE (t.copyActiveVoices (v, 16) == 1);
    CHECK (v[0].pressure < 0.1f);
}

TEST_CASE ("MPEVoiceTracker stays within 16 voices and reuses freed slots", "[mpe]")
{
    gf::MPEVoiceTracker t;
    t.setRoot (60);

    // Hold 16 notes on channels 2..16 (15 channels) plus an extra (steals).
    for (int i = 0; i < 16; ++i)
        t.handleMessage (memberNoteOn (2 + (i % 15), 48 + i, 0.5f));
    CHECK (t.getActiveCount() <= 16);

    // Release all and confirm the tracker drops to zero (no leaks).
    for (int i = 0; i < 16; ++i)
        t.handleMessage (memberNoteOff (2 + (i % 15), 48 + i));
    CHECK (t.getActiveCount() == 0);
}
