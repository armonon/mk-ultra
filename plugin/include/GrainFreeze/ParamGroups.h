#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <utility>
#include <vector>

namespace gf
{

// Collects parameters into juce::AudioProcessorParameterGroups as they are
// declared, so a host's automation list reads as a couple of dozen named
// sections instead of one flat wall of several hundred entries.
//
// It deliberately exposes the same add() shape as
// juce::AudioProcessorValueTreeState::ParameterLayout, so declaration code reads
// identically whether or not it is grouped: open a section with group(), then go
// on calling layout.add (std::make_unique<...>) exactly as before.
//
// group() is resumable -- naming a section that already exists appends to it --
// because the declaration order interleaves real controls with the per-knob
// "ModOn"/"Lock" plumbing, and those belong in different places in the list.
//
// Parameter IDs are untouched, and both the VST3 and AU wrappers derive their
// own parameter IDs from those strings, so grouping does not disturb automation
// in existing projects.
class ParamLayoutBuilder
{
public:
    void group (juce::String groupId, juce::String groupName)
    {
        flush();
        currentId   = std::move (groupId);
        currentName = std::move (groupName);
    }

    void add (std::unique_ptr<juce::RangedAudioParameter> p)
    {
        pending.push_back (std::move (p));
    }

    // Anything declared before the first group() lands in a catch-all section, so
    // a parameter can never go missing just because a group() call was forgotten.
    juce::AudioProcessorValueTreeState::ParameterLayout build()
    {
        flush();
        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        for (auto& g : groups)
            layout.add (std::move (g.group));
        groups.clear();
        return layout;
    }

private:
    void flush()
    {
        if (pending.empty())
            return;

        const auto id   = currentId.isNotEmpty()   ? currentId   : juce::String ("general");
        const auto name = currentName.isNotEmpty() ? currentName : juce::String ("General");

        auto* target = find (id);
        if (target == nullptr)
        {
            groups.push_back ({ id, std::make_unique<juce::AudioProcessorParameterGroup> (id, name, "|") });
            target = &groups.back();
        }
        for (auto& p : pending)
            target->group->addChild (std::move (p));
        pending.clear();
    }

    struct Entry
    {
        juce::String id;
        std::unique_ptr<juce::AudioProcessorParameterGroup> group;
    };
    Entry* find (const juce::String& id)
    {
        for (auto& e : groups)
            if (e.id == id)
                return &e;
        return nullptr;
    }

    juce::String currentId, currentName;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> pending;
    std::vector<Entry> groups;
};

} // namespace gf
