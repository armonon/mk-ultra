#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>

namespace gf
{

// Wraps juce::MPEInstrument and tracks up to 16 simultaneous voices, each with
// its own pitch (note + bend), pressure (channel Y), timbre (CC74 Z) and
// velocity. The audio thread reads the active voices via `copyActiveVoices()`
// which is allocation-free.
//
// Why this exists: the existing MidiNoteController is monophonic and only
// tracks integer note numbers. Full per-note MPE expression (pitch bend,
// pressure, timbre) needs persistent per-voice state, which JUCE's
// MPEInstrument provides via callbacks. This class just keeps a snapshot of
// "active voices right now" that the audio thread can read in O(N) per grain.
class MPEVoiceTracker : private juce::MPEInstrument::Listener
{
public:
    struct VoiceData
    {
        float semitones;   // note + pitch bend, relative to the root
        float pressure;    // 0..1
        float timbre;      // -1..+1 (CC74 normalised)
        float velocity;    // 0..1 from the initial noteOn
    };

    MPEVoiceTracker()
    {
        // Default to the lower MPE zone: master ch1, member channels 2-16.
        juce::MPEZoneLayout layout;
        layout.setLowerZone (15);   // 15 member channels (2..16)
        instrument.setZoneLayout (layout);
        instrument.addListener (this);
    }

    ~MPEVoiceTracker() override { instrument.removeListener (this); }

    void setRoot (int n) { root.store (juce::jlimit (0, 127, n), std::memory_order_relaxed); }

    void handleMessage (const juce::MidiMessage& m) { instrument.processNextMidiEvent (m); }

    void releaseAllNotes() { instrument.releaseAllNotes(); }

    // Copies up to `maxVoices` active voices into `out` and returns the count.
    // Audio-thread safe: a lock-free read of the atomics. Snapshot semantics --
    // the data may race a Listener callback by one block, which is fine: voices
    // appear/disappear smoothly between blocks.
    int copyActiveVoices (VoiceData* out, int maxVoices) const
    {
        int n = 0;
        for (int i = 0; i < (int) voices.size() && n < maxVoices; ++i)
        {
            if (! voices[(size_t) i].active.load (std::memory_order_acquire))
                continue;
            out[n].semitones = voices[(size_t) i].semitones.load (std::memory_order_relaxed);
            out[n].pressure  = voices[(size_t) i].pressure .load (std::memory_order_relaxed);
            out[n].timbre    = voices[(size_t) i].timbre   .load (std::memory_order_relaxed);
            out[n].velocity  = voices[(size_t) i].velocity .load (std::memory_order_relaxed);
            ++n;
        }
        return n;
    }

    int getActiveCount() const
    {
        int n = 0;
        for (auto& v : voices) if (v.active.load (std::memory_order_relaxed)) ++n;
        return n;
    }

private:
    void noteAdded (juce::MPENote newNote) override
    {
        // Find a free slot.
        for (int i = 0; i < (int) voices.size(); ++i)
        {
            if (! voices[(size_t) i].active.load (std::memory_order_relaxed))
            {
                writeVoice ((size_t) i, newNote, true);
                return;
            }
        }
        // All full -- steal the OLDEST active voice (lowest startTime).
        int oldest = 0;
        std::uint64_t oldestT = voices[0].startTime.load (std::memory_order_relaxed);
        for (int i = 1; i < (int) voices.size(); ++i)
        {
            const auto t = voices[(size_t) i].startTime.load (std::memory_order_relaxed);
            if (t < oldestT) { oldest = i; oldestT = t; }
        }
        writeVoice ((size_t) oldest, newNote, true);
    }

    void notePressureChanged (juce::MPENote n) override   { updateVoice (n); }
    void notePitchbendChanged (juce::MPENote n) override  { updateVoice (n); }
    void noteTimbreChanged   (juce::MPENote n) override   { updateVoice (n); }
    void noteReleased (juce::MPENote finishedNote) override
    {
        for (int i = 0; i < (int) voices.size(); ++i)
            if (voices[(size_t) i].noteId.load() == (int) finishedNote.noteID)
                voices[(size_t) i].active.store (false, std::memory_order_release);
    }

    void writeVoice (size_t slot, juce::MPENote n, bool isNew)
    {
        const float semi = (float) (n.initialNote - root.load (std::memory_order_relaxed))
                         + n.totalPitchbendInSemitones;
        voices[slot].noteId.store ((int) n.noteID, std::memory_order_relaxed);
        voices[slot].semitones.store (semi, std::memory_order_relaxed);
        voices[slot].pressure .store (n.pressure.asUnsignedFloat(), std::memory_order_relaxed);
        voices[slot].timbre   .store (2.0f * n.timbre.asUnsignedFloat() - 1.0f, std::memory_order_relaxed);
        if (isNew)
        {
            voices[slot].velocity.store (n.noteOnVelocity.asUnsignedFloat(), std::memory_order_relaxed);
            voices[slot].startTime.store (++noteCounter, std::memory_order_relaxed);
        }
        voices[slot].active.store (true, std::memory_order_release);
    }

    void updateVoice (juce::MPENote n)
    {
        for (int i = 0; i < (int) voices.size(); ++i)
            if (voices[(size_t) i].noteId.load() == (int) n.noteID
                && voices[(size_t) i].active.load (std::memory_order_relaxed))
            { writeVoice ((size_t) i, n, false); return; }
    }

    struct Voice
    {
        std::atomic<bool>  active   { false };
        std::atomic<int>   noteId   { -1 };
        std::atomic<float> semitones { 0.0f };
        std::atomic<float> pressure  { 0.0f };
        std::atomic<float> timbre    { 0.0f };
        std::atomic<float> velocity  { 0.0f };
        std::atomic<std::uint64_t> startTime { 0 };  // monotonic counter -> oldest-first stealing
    };

    juce::MPEInstrument instrument;
    std::array<Voice, 16> voices {};
    std::atomic<int> root { 60 };
    std::atomic<std::uint64_t> noteCounter { 0 };
};

} // namespace gf
