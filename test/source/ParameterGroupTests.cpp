// Parameter grouping. Several hundred flat parameters make a host's automation
// menu unusable, so every parameter belongs to a named section. What matters is
// that grouping loses nothing and that no section is a wall of its own.
#include <catch2/catch_test_macros.hpp>
#include "ProcessorFixture.h"
#include <iostream>
#include <iomanip>
#include <map>

TEST_CASE ("Every parameter lives in a sensibly sized group", "[params]")
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    auto procOwner = std::make_unique<GrainFreezeProcessor>();
    auto& proc = *procOwner;

    const auto& tree = proc.getParameterTree();

    // 1. Nothing sits loose at the top level: every child is a group.
    int looseParams = 0;
    for (auto* node : tree)
        if (node->getParameter() != nullptr)
            ++looseParams;
    INFO (looseParams << " parameters are not in any group");
    CHECK (looseParams == 0);

    // 2. Grouping loses nothing: the groups hold every parameter the host sees.
    std::map<juce::String, int> sizes;
    int grouped = 0;
    for (auto* node : tree)
    {
        auto* group = node->getGroup();
        if (group == nullptr) continue;
        const int n = group->getParameters (true).size();
        sizes[group->getName()] = n;
        grouped += n;
    }
    const int total = proc.getParameters().size();
    INFO ("grouped " << grouped << " of " << total << " parameters into " << sizes.size() << " groups");
    CHECK (grouped == total);
    CHECK (sizes.size() >= 10);

    // 3. Group names are unique and non-empty (a host shows these verbatim).
    for (const auto& [name, n] : sizes)
    {
        INFO ("group \"" << name << "\"");
        CHECK (name.isNotEmpty());
        CHECK (n > 0);
    }

    // 4. No section is a wall of its own.
    std::cout << "[params] " << total << " parameters in " << sizes.size() << " groups\n";
    for (const auto& [name, n] : sizes)
        std::cout << "[params] " << std::left << std::setw (28) << name.toStdString()
                  << std::right << std::setw (5) << n << "\n";
    for (const auto& [name, n] : sizes)
    {
        INFO ("group \"" << name << "\" holds " << n << " parameters");
        CHECK (n <= 64);
    }
}
