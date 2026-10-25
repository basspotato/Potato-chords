#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include "Progression.h"
#include "Voicing.h"
#include "Sequencer.h"
#include "Melody.h"

// Turning what the plugin plays into Standard MIDI Files, for drag-and-drop into the DAW.
// Message thread only (these allocate).
namespace pc::midiexport
{
    constexpr int ticksPerQuarter = 960;
    constexpr int melodyChannelDefault = 2;

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

    // The whole progression lane, voiced and played with the rhythm pattern exactly as the plugin plays it
    inline juce::MidiFile laneToMidi (const progression::Snapshot& lane, int key, theory::Scale scale, voicing::Spread spread,
                                      double bpm, double barQuarters, int patternIndex = 0, int channel = 1,
                                      double strumSeconds = 0.0, int octaves = 0, double swing = 0.0)
    {
        using sequencer::stepQuarters;
        const auto& pattern = sequencer::patterns[(size_t) juce::jlimit (0, sequencer::numPatterns - 1, patternIndex)];
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
                const auto notes = voicing::shiftOctaves (voicing::applySpread (chord, previous, spread), octaves);
                const int steps = (int) std::llround (length / stepQuarters);
                for (int step = 0; step < steps; ++step)
                {
                    const auto* hit = sequencer::hitStartingAt (pattern, step);
                    if (hit == nullptr) continue;
                    const double on = start + step * stepQuarters + (step % 2 == 1 ? swing * sequencer::stepQuarters / 3.0 : 0.0);
                    const double off = start + juce::jmin (steps, step + hit->length) * stepQuarters;
                    const double gap = strumSeconds * bpm / 60.0;  // in quarters
                    auto add = [&] (int note, int rank)
                    {
                        // A strummed note that would start after the hit ends is left out, as when played live
                        const double at = on + rank * gap;
                        if (at >= off) return;
                        seq.addEvent (juce::MidiMessage::noteOn (channel, note, (juce::uint8) 100).withTimeStamp (at));
                        seq.addEvent (juce::MidiMessage::noteOff (channel, note).withTimeStamp (off));
                    };
                    if (hit->arp < 0)
                        for (int i = 0; i < notes.size; ++i)
                        {
                            int rank = 0;
                            for (int j = 0; j < notes.size; ++j)
                                rank += notes.notes[(size_t) j] < notes.notes[(size_t) i] || (notes.notes[(size_t) j] == notes.notes[(size_t) i] && j < i) ? 1 : 0;
                            add (notes.notes[(size_t) i], rank);
                        }
                    else
                        add (juce::jmin (127, notes.notes[(size_t) (hit->arp % notes.size)] + 12 * (hit->arp / notes.size)), 0);
                }
            }
        }
        return Recorder::makeFile (seq, bpm);
    }

    // The melody lane as notes (steps past the end of a short bar, e.g. in 3/4, are left out)
    inline juce::MidiFile melodyToMidi (const melody::Snapshot& tune, int key, theory::Scale scale, double bpm, double barQuarters,
                                        int channel = melodyChannelDefault, double swing = 0.0)
    {
        using sequencer::stepQuarters;
        const int stepsInBar = juce::jmin (melody::stepsPerBar, (int) std::llround (barQuarters / stepQuarters));
        juce::MidiMessageSequence seq;
        for (int bar = 0; bar < progression::numBars; ++bar)
            for (int step = 0; step < stepsInBar; ++step)
            {
                const int index = bar * melody::stepsPerBar + step;
                const auto c = tune.cell (index);
                if (c.empty() || c.hold) continue;
                const int length = juce::jmin (tune.lengthFrom (index), stepsInBar - step);
                const int note = melody::rowToNote (c.row, key, scale);
                const double on = bar * barQuarters + step * stepQuarters + (step % 2 == 1 ? swing * stepQuarters / 3.0 : 0.0);
                seq.addEvent (juce::MidiMessage::noteOn (channel, note, (juce::uint8) 100).withTimeStamp (on));
                seq.addEvent (juce::MidiMessage::noteOff (channel, note).withTimeStamp (on + length * stepQuarters));
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
