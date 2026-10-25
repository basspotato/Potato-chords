#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>

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
}
