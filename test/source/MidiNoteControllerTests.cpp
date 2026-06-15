#include <catch2/catch_test_macros.hpp>
#include "GrainFreeze/Modulation/MidiNoteController.h"

TEST_CASE ("MidiNoteController uses last-note priority", "[midi]")
{
    gf::MidiNoteController midi;
    midi.prepare (48000.0);
    midi.setRoot (60);

    midi.handleMessage (juce::MidiMessage::noteOn (1, 60, 0.8f));
    REQUIRE (midi.isGateOpen());
    midi.handleMessage (juce::MidiMessage::noteOn (1, 67, 0.5f));

    const float off = midi.nextOffsetSemis();
    REQUIRE (off >= 6.9f);
    REQUIRE (off <= 7.1f);

    midi.handleMessage (juce::MidiMessage::noteOff (1, 67));
    const float back = midi.nextOffsetSemis();
    REQUIRE (back >= -0.1f);
    REQUIRE (back <= 0.1f);
}

TEST_CASE ("MidiNoteController tracks velocity and closes gate", "[midi]")
{
    gf::MidiNoteController midi;
    midi.prepare (44100.0);
    midi.handleMessage (juce::MidiMessage::noteOn (1, 72, 0.3f));
    REQUIRE (midi.getVelocity() > 0.299f);
    REQUIRE (midi.getVelocity() < 0.301f);
    REQUIRE (midi.isGateOpen());
    midi.handleMessage (juce::MidiMessage::noteOff (1, 72));
    REQUIRE_FALSE (midi.isGateOpen());
}
