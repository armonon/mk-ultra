#include "GrainFreeze/Presets/SnapshotManager.h"

namespace gf
{

void SnapshotManager::store (int idx)
{
    if (idx < 0 || idx >= (int) snapshots.size())
        return;
    snapshots[(size_t) idx] = apvts.copyState();
}

void SnapshotManager::load (int idx)
{
    if (idx < 0 || idx >= (int) snapshots.size())
        return;
    if (snapshots[(size_t) idx].isValid())
        apvts.replaceState (snapshots[(size_t) idx].createCopy());
}

void SnapshotManager::morph (int a, int b, float amount)
{
    if (a < 0 || a >= (int) snapshots.size() || b < 0 || b >= (int) snapshots.size())
        return;
    if (! snapshots[(size_t) a].isValid() || ! snapshots[(size_t) b].isValid())
        return;

    auto from = snapshots[(size_t) a];
    auto to = snapshots[(size_t) b];
    const float t = juce::jlimit (0.0f, 1.0f, amount);
    auto blended = from.createCopy();
    const int props = blended.getNumProperties();
    for (int i = 0; i < props; ++i)
    {
        const auto key = blended.getPropertyName (i);
        const float aNorm = (float) from.getProperty (key, 0.0f);
        const float bNorm = (float) to.getProperty (key, aNorm);
        blended.setProperty (key, juce::jmap (t, aNorm, bNorm), nullptr);
    }

    apvts.replaceState (blended);
}

} // namespace gf
