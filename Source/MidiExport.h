#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include "Progression.h"
#include "Voicing.h"
#include "Sequencer.h"
#include "Melody.h"
#include "Song.h"

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

    // How generated notes are played: the same settings the plugin plays with
    struct Feel
    {
        int key = 0;
        theory::Scale scale = theory::Scale::major;
        voicing::Spread spread = voicing::Spread::close;
        double bpm = 120.0, barQuarters = 4.0;
        int pattern = 0, channel = 1, octaves = 0, velocity = 100;
        double strumSeconds = 0.0, swing = 0.0;
    };

    // Lane bars [srcBar, srcBar + srcBars) looped over numBars bars from destQuarters, voice-led from `previous`
    inline void addLaneBars (juce::MidiMessageSequence& seq, const progression::Snapshot& lane, int srcBar, int srcBars,
                             double destQuarters, int numBars, const Feel& f, voicing::Voiced& previous)
    {
        using sequencer::stepQuarters;
        const auto& pattern = sequencer::patterns[(size_t) juce::jlimit (0, sequencer::numPatterns - 1, f.pattern)];
        const double gap = f.strumSeconds * f.bpm / 60.0;  // in quarters
        for (int b = 0; b < numBars; ++b)
        {
            const int bar = srcBar + b % juce::jmax (1, srcBars);
            const int halves = lane.isSplit (bar) ? 2 : 1;
            for (int h = 0; h < halves; ++h)
            {
                const auto slot = lane.slot (bar * 2 + h);
                if (slot.empty()) continue;
                const double length = f.barQuarters / halves;
                const double start = destQuarters + b * f.barQuarters + h * length;
                const auto chord = theory::diatonicChord (f.key, f.scale, slot.degree, slot.flavour, slot.majorFive);
                previous = voicing::closeVoicing (chord, previous);
                const auto notes = voicing::shiftOctaves (voicing::applySpread (chord, previous, f.spread), f.octaves);
                const int steps = (int) std::llround (length / stepQuarters);
                for (int step = 0; step < steps; ++step)
                {
                    const auto* hit = sequencer::hitStartingAt (pattern, step);
                    if (hit == nullptr) continue;
                    const double on = start + step * stepQuarters + (step % 2 == 1 ? f.swing * stepQuarters / 3.0 : 0.0);
                    const double off = start + juce::jmin (steps, step + hit->length) * stepQuarters;
                    auto add = [&] (int note, int rank)
                    {
                        // A strummed note that would start after the hit ends is left out, as when played live
                        const double at = on + rank * gap;
                        if (at >= off) return;
                        seq.addEvent (juce::MidiMessage::noteOn (f.channel, note, (juce::uint8) f.velocity).withTimeStamp (at));
                        seq.addEvent (juce::MidiMessage::noteOff (f.channel, note).withTimeStamp (off));
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
    }

    // Melody bars [srcBar, srcBar + srcBars) looped over numBars bars from destQuarters
    // (steps past the end of a short bar, e.g. in 3/4, are left out)
    inline void addMelodyBars (juce::MidiMessageSequence& seq, const melody::Snapshot& tune, int srcBar, int srcBars,
                               double destQuarters, int numBars, const Feel& f)
    {
        using sequencer::stepQuarters;
        const int stepsInBar = juce::jmin (melody::stepsPerBar, (int) std::llround (f.barQuarters / stepQuarters));
        for (int b = 0; b < numBars; ++b)
        {
            const int bar = srcBar + b % juce::jmax (1, srcBars);
            for (int step = 0; step < stepsInBar; ++step)
            {
                const int index = bar * melody::stepsPerBar + step;
                const auto c = tune.cell (index);
                if (c.empty() || c.hold) continue;
                const int length = juce::jmin (tune.lengthFrom (index), stepsInBar - step);
                const int note = melody::rowToNote (c.row, f.key, f.scale, f.octaves);
                const double on = destQuarters + b * f.barQuarters + step * stepQuarters + (step % 2 == 1 ? f.swing * stepQuarters / 3.0 : 0.0);
                seq.addEvent (juce::MidiMessage::noteOn (f.channel, note, (juce::uint8) f.velocity).withTimeStamp (on));
                seq.addEvent (juce::MidiMessage::noteOff (f.channel, note).withTimeStamp (on + length * stepQuarters));
            }
        }
    }

    // The whole progression lane, voiced and played with the rhythm pattern exactly as the plugin plays it
    inline juce::MidiFile laneToMidi (const progression::Snapshot& lane, int key, theory::Scale scale, voicing::Spread spread,
                                      double bpm, double barQuarters, int patternIndex = 0, int channel = 1,
                                      double strumSeconds = 0.0, int octaves = 0, double swing = 0.0, int velocity = 100)
    {
        juce::MidiMessageSequence seq;
        voicing::Voiced previous;
        addLaneBars (seq, lane, 0, progression::numBars, 0.0, progression::numBars,
                     { key, scale, spread, bpm, barQuarters, patternIndex, channel, octaves, velocity, strumSeconds, swing }, previous);
        return Recorder::makeFile (seq, bpm);
    }

    // The melody lane as notes
    inline juce::MidiFile melodyToMidi (const melody::Snapshot& tune, int key, theory::Scale scale, double bpm, double barQuarters,
                                        int channel = melodyChannelDefault, double swing = 0.0, int octaves = 0, int velocity = 100)
    {
        juce::MidiMessageSequence seq;
        Feel f;
        f.key = key; f.scale = scale; f.bpm = bpm; f.barQuarters = barQuarters;
        f.channel = channel; f.swing = swing; f.octaves = octaves; f.velocity = velocity;
        addMelodyBars (seq, tune, 0, progression::numBars, 0.0, progression::numBars, f);
        return Recorder::makeFile (seq, bpm);
    }

    // Which half of the lane a song section loops: its own half, or the other one if that half is empty
    inline int loopStartBar (const progression::Snapshot& lane, song::Kind kind)
    {
        const int want = song::info (kind).loop * 4;
        for (int bar = want; bar < want + 4; ++bar)
            if (! lane.slot (bar * 2).empty() || ! lane.slot (bar * 2 + 1).empty()) return want;
        return 4 - want;
    }

    inline int melodyLoopStartBar (const melody::Snapshot& tune, song::Kind kind)
    {
        const int want = song::info (kind).loop * 4;
        for (int i = want * melody::stepsPerBar; i < (want + 4) * melody::stepsPerBar; ++i)
            if (! tune.cell (i).empty()) return want;
        return 4 - want;
    }

    // Song sections [first, last) as a two-track MIDI file: chords on track 1 (channel 1), melody on track 2
    // (channel 2). Each section plays its loop with its own rhythm; the melody only where the section has one.
    // `chords` and `tune` carry the plugin's settings (key, spread, strum...); their pattern is replaced per section.
    inline juce::MidiFile songToMidi (const song::Song& s, int first, int last, const progression::Snapshot& lane,
                                      const melody::Snapshot& tune, Feel chords, Feel tuneFeel)
    {
        juce::MidiMessageSequence chordSeq, melodySeq;
        voicing::Voiced previous;
        first = juce::jlimit (0, s.count, first);
        last = juce::jlimit (first, s.count, last);
        double at = 0.0;
        for (int i = first; i < last; ++i)
        {
            const auto& section = s.sections[(size_t) i];
            const auto& kind = song::info (section.kind);
            chords.pattern = kind.rhythm;
            addLaneBars (chordSeq, lane, loopStartBar (lane, section.kind), 4, at, section.bars, chords, previous);
            if (kind.melody)
                addMelodyBars (melodySeq, tune, melodyLoopStartBar (tune, section.kind), 4, at, section.bars, tuneFeel);
            at += section.bars * chords.barQuarters;
        }

        juce::MidiFile file;
        file.setTicksPerQuarterNote (ticksPerQuarter);
        for (auto* seq : { &chordSeq, &melodySeq })
        {
            juce::MidiMessageSequence track;
            if (seq == &chordSeq)
                track.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::llround (60000000.0 / juce::jmax (1.0, chords.bpm))));
            track.addEvent (juce::MidiMessage::textMetaEvent (3, seq == &chordSeq ? "Chords" : "Melody"));
            for (auto* e : *seq)
                track.addEvent (e->message.withTimeStamp (std::round (e->message.getTimeStamp() * ticksPerQuarter)));
            track.updateMatchedPairs();
            file.addTrack (track);
        }
        return file;
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
