#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

namespace gf
{

class SnapshotManager
{
public:
    explicit SnapshotManager (juce::AudioProcessorValueTreeState& state) : apvts (state) {}

    void store (int idx);
    void load (int idx);
    void morph (int a, int b, float amount);

private:
    juce::AudioProcessorValueTreeState& apvts;
    std::array<juce::ValueTree, 8> snapshots {};
};

} // namespace gf
