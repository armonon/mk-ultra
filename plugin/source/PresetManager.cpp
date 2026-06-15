#include "GrainFreeze/PresetManager.h"

namespace gf
{

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& state)
    : apvts (state)
{
    getPresetDirectory(); // ensure it exists
}

juce::File PresetManager::getPresetDirectory()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                   .getChildFile ("GrainFreeze")
                   .getChildFile ("Presets");
    if (! dir.exists())
        dir.createDirectory();
    return dir;
}

bool PresetManager::savePreset (const juce::String& name)
{
    if (name.isEmpty()) return false;

    auto state = apvts.copyState();
    auto xml   = state.createXml();
    if (xml == nullptr) return false;

    auto file = getPresetDirectory().getChildFile (name + kExtension);
    const bool ok = xml->writeTo (file, {});
    if (ok) currentPreset = name;
    return ok;
}

bool PresetManager::loadPreset (const juce::String& name)
{
    auto file = getPresetDirectory().getChildFile (name + kExtension);
    if (! file.existsAsFile()) return false;

    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr) return false;

    auto tree = juce::ValueTree::fromXml (*xml);
    if (! tree.isValid()) return false;

    apvts.replaceState (tree);
    currentPreset = name;
    return true;
}

bool PresetManager::deletePreset (const juce::String& name)
{
    auto file = getPresetDirectory().getChildFile (name + kExtension);
    if (! file.existsAsFile()) return false;
    const bool ok = file.deleteFile();
    if (ok && currentPreset == name) currentPreset = {};
    return ok;
}

juce::StringArray PresetManager::getPresetNames() const
{
    juce::StringArray names;
    auto files = getPresetDirectory().findChildFiles (
        juce::File::findFiles, false, juce::String ("*") + kExtension);
    for (auto& f : files)
        names.add (f.getFileNameWithoutExtension());
    names.sort (true);
    return names;
}

int PresetManager::indexOfCurrent (const juce::StringArray& names) const
{
    return names.indexOf (currentPreset);
}

void PresetManager::loadNext()
{
    auto names = getPresetNames();
    if (names.isEmpty()) return;
    int idx = indexOfCurrent (names);
    idx = (idx + 1) % names.size();
    loadPreset (names[idx]);
}

void PresetManager::loadPrevious()
{
    auto names = getPresetNames();
    if (names.isEmpty()) return;
    int idx = indexOfCurrent (names);
    if (idx < 0) idx = 0;
    idx = (idx - 1 + names.size()) % names.size();
    loadPreset (names[idx]);
}

} // namespace gf
