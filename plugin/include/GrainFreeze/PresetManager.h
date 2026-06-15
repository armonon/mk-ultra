#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace gf
{

// Saves and loads shareable .preset files (XML) in a shared user folder.
// The preset captures the full APVTS state plus the modulation settings,
// which the processor stores in the same ValueTree.
class PresetManager
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState& state);

    // Returns the folder where .preset files live (created if missing).
    static juce::File getPresetDirectory();

    // Persist current state to <name>.preset. Returns false on failure.
    bool savePreset (const juce::String& name);

    // Load a preset by display name (no extension). Returns false if missing.
    bool loadPreset (const juce::String& name);

    // Delete a preset file. Returns false if it didn't exist.
    bool deletePreset (const juce::String& name);

    // All preset names (no extension), sorted.
    juce::StringArray getPresetNames() const;

    // Cycle through presets; wraps around. No-op if none exist.
    void loadNext();
    void loadPrevious();

    juce::String getCurrentPresetName() const { return currentPreset; }

    static constexpr const char* kExtension = ".preset";

private:
    int indexOfCurrent (const juce::StringArray& names) const;

    juce::AudioProcessorValueTreeState& apvts;
    juce::String currentPreset;
};

} // namespace gf
