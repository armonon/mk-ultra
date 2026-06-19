#pragma once

#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace gf
{

// Checks the project's GitHub Releases for a newer version on a background
// thread, then (only if a newer release exists) calls back on the message
// thread with the new version tag + the release page URL. Fails silently if
// there is no network. The plugin installer overrides the previous version on
// install, so "update" just means: here's a newer build, go download it.
class UpdateChecker : private juce::Thread
{
public:
    UpdateChecker() : juce::Thread ("MKUltraUpdateCheck") {}
    ~UpdateChecker() override { stopThread (4000); }

    // onUpdate(versionTag, releaseUrl) is invoked on the message thread.
    void start (juce::String currentVersion,
                std::function<void (juce::String, juce::String)> onUpdate)
    {
        current  = std::move (currentVersion);
        callback = std::move (onUpdate);
        startThread();
    }

    // Compare dotted versions ("v2.4", "2.3.0", ...). Returns true if latest > currentV.
    static bool isNewer (juce::String latest, juce::String currentV)
    {
        auto rank = [] (juce::String s)
        {
            s = s.trimCharactersAtStart ("vV ").trim();
            juce::StringArray parts;
            parts.addTokens (s, ".", "");
            int n[3] = { 0, 0, 0 };
            for (int i = 0; i < 3 && i < parts.size(); ++i)
                n[i] = parts[i].getIntValue();
            return n[0] * 1000000 + n[1] * 1000 + n[2];
        };
        return rank (latest) > rank (currentV);
    }

    void run() override
    {
        juce::URL url ("https://api.github.com/repos/armonon/mk-ultra/releases/latest");
        auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                        .withConnectionTimeoutMs (8000)
                        .withExtraHeaders ("User-Agent: MK-ULTRA-Plugin\r\nAccept: application/vnd.github+json");

        std::unique_ptr<juce::InputStream> stream (url.createInputStream (opts));
        if (stream == nullptr || threadShouldExit())
            return;

        const auto text = stream->readEntireStreamAsString();
        if (threadShouldExit())
            return;

        const auto json = juce::JSON::parse (text);
        const auto tag  = json.getProperty ("tag_name", "").toString();
        const auto html = json.getProperty ("html_url", "").toString();
        if (tag.isEmpty() || ! isNewer (tag, current))
            return;

        auto cb = callback;
        juce::MessageManager::callAsync ([cb, tag, html] { if (cb) cb (tag, html); });
    }

private:
    juce::String current;
    std::function<void (juce::String, juce::String)> callback;
};

} // namespace gf
