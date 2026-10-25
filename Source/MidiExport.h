#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include "Progression.h"
#include "Voicing.h"

// Turning what the plugin plays into Standard MIDI Files, for drag-and-drop into the DAW.
// Message thread only (these allocate).
namespace pc::midiexport
{
    constexpr int ticksPerQuarter = 960;

    // One outgoing MIDI event as the audio thread hands it over, timed in quarter notes
    struct CaptureEvent
    {
        double quarters = 0.0;
        juce::uint8 status = 0, data1 = 0, data2 = 0;
    };

    // Collects captured events into a take, closing any note still held when the take ends
    class Recorder
    {
    public:
        void clear() { sequence.clear(); held.fill (0); }

        void add (const CaptureEvent& e)
        {
            const juce::MidiMessage m (e.status, e.data1, e.data2, e.quarters);
            if (m.isNoteOn())  ++held[(size_t) (m.getChannel() - 1) * 128 + (size_t) m.getNoteNumber()];
            if (m.isNoteOff())
            {
                auto& h = held[(size_t) (m.getChannel() - 1) * 128 + (size_t) m.getNoteNumber()];
                if (h == 0) return;  // its note-on came before the take started
                --h;
            }
            sequence.addEvent (m);
        }

        int numNotes() const
        {
            int n = 0;
            for (auto* e : sequence) n += e->message.isNoteOn() ? 1 : 0;
            return n;
        }

        // The take as a MIDI file: starts at the bar the first note falls in, so timing within the bar is kept
        juce::MidiFile toMidiFile (double bpm, double barQuarters) const
        {
            juce::MidiMessageSequence take (sequence);
            if (take.getNumEvents() > 0)
            {
                const double end = take.getEndTime() + 0.25;
                for (int ch = 1; ch <= 16; ++ch)
                    for (int note = 0; note < 128; ++note)
                        for (int k = 0; k < held[(size_t) (ch - 1) * 128 + (size_t) note]; ++k)
                            take.addEvent (juce::MidiMessage::noteOff (ch, note).withTimeStamp (end));
                const double first = take.getStartTime();
                take.addTimeToMessages (-std::floor (first / barQuarters + 1.0e-9) * barQuarters);
            }
            return makeFile (take, bpm);
        }

        static juce::MidiFile makeFile (juce::MidiMessageSequence quartersSequence, double bpm)
        {
            juce::MidiMessageSequence track;
            track.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::llround (60000000.0 / juce::jmax (1.0, bpm))));
            for (auto* e : quartersSequence)
                track.addEvent (e->message.withTimeStamp (std::round (e->message.getTimeStamp() * ticksPerQuarter)));
            track.updateMatchedPairs();
            juce::MidiFile file;
            file.setTicksPerQuarterNote (ticksPerQuarter);
            file.addTrack (track);
            return file;
        }

    private:
        juce::MidiMessageSequence sequence;
        std::array<int, 16 * 128> held {};
    };

    // The whole progression lane as chords, voiced exactly as the plugin plays it
    inline juce::MidiFile laneToMidi (const progression::Snapshot& lane, int key, theory::Scale scale, voicing::Spread spread,
                                      double bpm, double barQuarters, int channel = 1)
    {
        juce::MidiMessageSequence seq;
        voicing::Voiced previous;
        for (int bar = 0; bar < progression::numBars; ++bar)
        {
            const int halves = lane.isSplit (bar) ? 2 : 1;
            for (int h = 0; h < halves; ++h)
            {
                const auto slot = lane.slot (bar * 2 + h);
                if (slot.empty()) continue;
                const double length = barQuarters / halves;
                const double start = bar * barQuarters + h * length;
                const auto chord = theory::diatonicChord (key, scale, slot.degree, slot.flavour, slot.majorFive);
                previous = voicing::closeVoicing (chord, previous);
                const auto notes = voicing::applySpread (chord, previous, spread);
                for (int i = 0; i < notes.size; ++i)
                {
                    seq.addEvent (juce::MidiMessage::noteOn (channel, notes.notes[(size_t) i], (juce::uint8) 100).withTimeStamp (start));
                    seq.addEvent (juce::MidiMessage::noteOff (channel, notes.notes[(size_t) i]).withTimeStamp (start + length));
                }
            }
        }
        return Recorder::makeFile (seq, bpm);
    }

    // Writes a file to the temp folder for dragging out (overwrites the previous one with the same name)
    inline juce::File writeTemp (const juce::MidiFile& file, const juce::String& name)
    {
        auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile (name + ".mid");
        f.deleteFile();
        juce::FileOutputStream out (f);
        if (! out.openedOk() || ! file.writeTo (out)) return {};
        return f;
    }
}
