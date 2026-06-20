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

bool PresetManager::applyFactory (const juce::String& name)
{
    const FactoryPreset* found = nullptr;
    for (auto& fp : factoryPresets())
        if (name == fp.name) { found = &fp; break; }
    if (found == nullptr) return false;

    // Reset every parameter to its default, then apply the preset's overrides.
    for (auto child : apvts.state)
    {
        const auto id = child.getProperty ("id").toString();
        if (id.isNotEmpty())
            if (auto* p = apvts.getParameter (id))
                p->setValueNotifyingHost (p->getDefaultValue());
    }
    for (auto& fpp : found->params)
        if (auto* p = apvts.getParameter (fpp.id))
            p->setValueNotifyingHost (p->getNormalisableRange().convertTo0to1 (fpp.value));

    currentPreset = name;
    return true;
}

void PresetManager::loadDefaultPatch()
{
    applyFactory (kDefaultPresetName);
}

bool PresetManager::loadPreset (const juce::String& name)
{
    if (applyFactory (name))            // built-in factory preset
        return true;

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

bool PresetManager::exportPresetToFile (const juce::File& file) const
{
    auto state = apvts.copyState();
    auto xml   = state.createXml();
    return xml != nullptr && xml->writeTo (file, {});
}

bool PresetManager::importPresetFromFile (const juce::File& file)
{
    if (! file.existsAsFile()) return false;
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr) return false;
    auto tree = juce::ValueTree::fromXml (*xml);
    if (! tree.isValid()) return false;
    apvts.replaceState (tree);
    currentPreset = file.getFileNameWithoutExtension();
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
    for (auto& fp : factoryPresets())   // built-ins first (default, then categories)
        names.add (fp.name);

    juce::StringArray userNames;
    auto files = getPresetDirectory().findChildFiles (
        juce::File::findFiles, false, juce::String ("*") + kExtension);
    for (auto& f : files)
        userNames.add (f.getFileNameWithoutExtension());
    userNames.sort (true);
    names.addArray (userNames);
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
