#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cstdint>

// Note scheduling with hanging-note protection. Every note-on that goes out is counted, so the plugin can
// always send the matching note-offs (on stop, state load, bypass, chord change).
namespace pc::midiout
{
    class NoteTracker
    {
    public:
        // A note already sounding is retriggered (off, then on) so the host never sees a doubled note-on
        void noteOn (juce::MidiBuffer& out, int channel, int note, juce::uint8 velocity, int samplePos)
        {
            auto& count = counts[(size_t) channel - 1][(size_t) note];
            if (count > 0)
                out.addEvent (juce::MidiMessage::noteOff (channel, note), samplePos);
            out.addEvent (juce::MidiMessage::noteOn (channel, note, velocity), samplePos);
            if (count < 255) ++count;
        }

        // The note-off goes out once the last source holding the note lets go
        void noteOff (juce::MidiBuffer& out, int channel, int note, int samplePos)
        {
            auto& count = counts[(size_t) channel - 1][(size_t) note];
            if (count == 0) return;
            if (--count == 0)
                out.addEvent (juce::MidiMessage::noteOff (channel, note), samplePos);
        }

        void allOff (juce::MidiBuffer& out, int samplePos)
        {
            for (int ch = 1; ch <= 16; ++ch)
                for (int note = 0; note < 128; ++note)
                    if (auto& count = counts[(size_t) ch - 1][(size_t) note]; count > 0)
                    {
                        out.addEvent (juce::MidiMessage::noteOff (ch, note), samplePos);
                        count = 0;
                    }
        }

        bool isOn (int channel, int note) const { return counts[(size_t) channel - 1][(size_t) note] > 0; }

        // One bit per MIDI note that is on in any channel (bits 0-63 in [0], 64-127 in [1])
        std::array<uint64_t, 2> noteBits() const
        {
            std::array<uint64_t, 2> bits {};
            for (auto& ch : counts)
                for (int note = 0; note < 128; ++note)
                    if (ch[(size_t) note] > 0) bits[(size_t) note / 64] |= uint64_t (1) << (note % 64);
            return bits;
        }

        int numOn() const
        {
            int n = 0;
            for (auto& ch : counts)
                for (auto c : ch) n += c > 0 ? 1 : 0;
            return n;
        }

    private:
        std::array<std::array<juce::uint8, 128>, 16> counts {};
    };

    // Note-ons held back for a strum, each sent on its own sample, possibly in a later block.
    // Times are absolute sample counts. An owner's notes are fired before its next change, so its
    // note-ons and note-offs always stay in time order.
    class Strummer
    {
    public:
        static constexpr int capacity = 32;

        void clear() { for (auto& p : pending) p.active = false; }

        void add (int owner, int channel, int note, juce::uint8 velocity, int64_t time)
        {
            for (auto& p : pending)
                if (! p.active) { p = { time, owner, channel, note, velocity, true }; return; }
        }

        // Sends every note-on of `owner` (or of all owners, -1) due before `until`
        void fireUntil (NoteTracker& tracker, juce::MidiBuffer& out, int64_t blockStart, int64_t until, int owner = -1)
        {
            for (auto& p : pending)
                if (p.active && p.time < until && (owner < 0 || p.owner == owner))
                {
                    tracker.noteOn (out, p.channel, p.note, p.velocity, (int) (p.time - blockStart));
                    p.active = false;
                }
        }

        // Drops a note that has not gone out yet; true if it was still waiting
        bool cancel (int owner, int channel, int note)
        {
            for (auto& p : pending)
                if (p.active && p.owner == owner && p.channel == channel && p.note == note) { p.active = false; return true; }
            return false;
        }

        int numPending() const
        {
            int n = 0;
            for (auto& p : pending) n += p.active ? 1 : 0;
            return n;
        }

    private:
        struct Pending
        {
            int64_t time = 0;
            int owner = 0, channel = 1, note = 0;
            juce::uint8 velocity = 0;
            bool active = false;
        };
        std::array<Pending, capacity> pending {};
    };
}
