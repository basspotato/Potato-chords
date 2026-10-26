// Headless tests for chord theory and the plugin processor.
// Build with -DPOTATO_DEV_TOOLS=ON, run with ctest or ./build/PotatoChordsTests.
#include "PluginProcessor.h"
#include "Theory.h"
#include "Voicing.h"
#include "PreviewSynth.h"
#include "Presets.h"
#include "Writer.h"
#include <set>
#include <cstring>
#include <map>
#include <iostream>

namespace
{
int failures = 0;

#define CHECK(cond, what)                                                         \
    do {                                                                          \
        if (! (cond)) { ++failures; std::cerr << "FAIL: " << what << "\n"; }      \
        else          { std::cout << "ok:   " << what << "\n"; }                   \
    } while (false)

constexpr double sampleRate = 48000.0;
constexpr int blockSize = 256;

using pc::theory::Scale;
using Triad = std::array<int, 3>;

void theoryTests()
{
    CHECK ((pc::theory::triad (60, Scale::major, 0) == Triad { 60, 64, 67 }), "C major I = C E G");
    CHECK ((pc::theory::triad (60, Scale::major, 4) == Triad { 67, 71, 74 }), "C major V = G B D");
    CHECK ((pc::theory::triad (60, Scale::major, 6) == Triad { 71, 74, 77 }), "C major viidim = B D F");
    CHECK ((pc::theory::triad (57, Scale::minor, 0) == Triad { 57, 60, 64 }), "A minor i = A C E");
    CHECK ((pc::theory::triad (57, Scale::minor, 5) == Triad { 65, 69, 72 }), "A minor VI = F A C");
    CHECK (pc::theory::stepToSemitone (Scale::major, -1) == -1, "step below the root wraps down an octave");
    CHECK (pc::theory::stepToSemitone (Scale::minor, 14) == 24, "two octaves up");
}

using pc::theory::Flavour;
using Notes = std::vector<int>;

Notes chordNotes (int key, Scale s, int degree, Flavour f, bool majorFive = false)
{
    const auto c = pc::theory::diatonicChord (key, s, degree, f, majorFive);
    Notes n;
    for (int i = 0; i < c.size; ++i) n.push_back ((c.root + c.intervals[(size_t) i]) % 12);
    return n;
}

bool named (const pc::theory::Label& l, const char* expected) { return std::strcmp (l.text, expected) == 0; }

void chordTests()
{
    using pc::theory::diatonicChord;
    using pc::theory::chordName;
    using pc::theory::romanNumeral;

    CHECK ((chordNotes (0, Scale::major, 0, Flavour::seventh) == Notes { 0, 4, 7, 11 }), "Cmaj7 = C E G B");
    CHECK ((chordNotes (0, Scale::major, 4, Flavour::seventh) == Notes { 7, 11, 2, 5 }), "G7 = G B D F");
    CHECK ((chordNotes (0, Scale::major, 0, Flavour::sus2) == Notes { 0, 2, 7 }), "Csus2 = C D G");
    CHECK ((chordNotes (0, Scale::major, 0, Flavour::sus4) == Notes { 0, 5, 7 }), "Csus4 = C F G");
    CHECK ((chordNotes (0, Scale::major, 0, Flavour::add9) == Notes { 0, 4, 7, 2 }), "Cadd9 = C E G D");
    CHECK ((chordNotes (0, Scale::major, 0, Flavour::power) == Notes { 0, 7 }), "C5 = C G");
    CHECK ((chordNotes (9, Scale::minor, 4, Flavour::triad) == Notes { 4, 7, 11 }), "A minor v = Em");
    CHECK ((chordNotes (9, Scale::minor, 4, Flavour::triad, true) == Notes { 4, 8, 11 }), "A minor major V = E G# B");
    CHECK ((chordNotes (9, Scale::minor, 4, Flavour::seventh, true) == Notes { 4, 8, 11, 2 }), "A minor V7 = E G# B D");
    CHECK ((chordNotes (0, Scale::major, 4, Flavour::triad, true) == Notes { 7, 11, 2 }), "major V switch does nothing in major");

    const char* cMajorNames[] { "C", "Dm", "Em", "F", "G", "Am", "B\xc2\xb0" };
    const char* cMajorNumerals[] { "I", "ii", "iii", "IV", "V", "vi", "vii\xc2\xb0" };
    bool allNames = true, allNumerals = true;
    for (int d = 0; d < 7; ++d)
    {
        const auto c = diatonicChord (0, Scale::major, d, Flavour::triad, false);
        allNames = allNames && named (chordName (c, false), cMajorNames[d]);
        allNumerals = allNumerals && named (romanNumeral (c, d), cMajorNumerals[d]);
    }
    CHECK (allNames, "C major chord names C Dm Em F G Am Bdim");
    CHECK (allNumerals, "C major numerals I ii iii IV V vi viidim");

    CHECK (named (chordName (diatonicChord (0, Scale::major, 0, Flavour::seventh, false), false), "Cmaj7"), "name Cmaj7");
    CHECK (named (chordName (diatonicChord (0, Scale::major, 4, Flavour::seventh, false), false), "G7"), "name G7");
    CHECK (named (chordName (diatonicChord (0, Scale::major, 6, Flavour::seventh, false), false), "Bm7b5"), "name Bm7b5");
    CHECK (named (romanNumeral (diatonicChord (0, Scale::major, 6, Flavour::seventh, false), 6), "vii\xc3\xb8" "7"), "numeral half-diminished 7");
    CHECK (named (chordName (diatonicChord (9, Scale::minor, 0, Flavour::add9, false), false), "Am(add9)"), "name Am(add9)");
    CHECK (named (romanNumeral (diatonicChord (9, Scale::minor, 4, Flavour::triad, true), 4), "V"), "major V in minor is uppercase");
    CHECK (named (romanNumeral (diatonicChord (9, Scale::minor, 2, Flavour::triad, false), 2), "III"), "A minor III");
    CHECK (named (chordName (diatonicChord (5, Scale::major, 3, Flavour::triad, false), pc::theory::usesFlats (5, Scale::major)), "Bb"),
           "F major IV is spelled Bb");
    CHECK (pc::theory::usesFlats (2, Scale::minor) && ! pc::theory::usesFlats (9, Scale::minor), "D minor uses flats, A minor doesn't");

    CHECK (pc::theory::whiteKeyDegree (0) == 0 && pc::theory::whiteKeyDegree (11) == 6 && pc::theory::whiteKeyDegree (1) == -1,
           "white keys C-B map to degrees 0-6, black keys to none");
}

using pc::voicing::Voiced;

Notes toNotes (const Voiced& v) { return Notes (v.notes.begin(), v.notes.begin() + v.size); }

void voicingTests()
{
    using pc::theory::diatonicChord;
    const auto chord = [] (int degree, Flavour f = Flavour::triad) { return diatonicChord (0, Scale::major, degree, f, false); };

    const auto first = pc::voicing::closeVoicing (chord (0), {});
    CHECK ((toNotes (first) == Notes { 60, 64, 67 }), "first chord is root position near middle C");

    const auto four = pc::voicing::closeVoicing (chord (3), first);
    CHECK ((toNotes (four) == Notes { 60, 65, 69 }), "C to F moves to the nearest inversion (C F A)");

    const auto five = pc::voicing::closeVoicing (chord (4), first);
    CHECK ((toNotes (five) == Notes { 59, 62, 67 }), "C to G moves to the nearest inversion (B D G)");

    // A long progression stays in range and never jumps far
    Voiced prev, previousTriad;
    bool inRange = true, triadsSmooth = true;
    const int degrees[] { 0, 4, 5, 3, 0, 4, 5, 3, 1, 6, 2, 5, 3, 4, 0 };
    for (int d : degrees)
        for (auto f : { Flavour::triad, Flavour::seventh, Flavour::add9, Flavour::sus4, Flavour::power })
        {
            const auto v = pc::voicing::closeVoicing (chord (d, f), prev);
            for (int i = 0; i < v.size; ++i)
                inRange = inRange && v.notes[(size_t) i] >= pc::voicing::lowestNote && v.notes[(size_t) i] <= pc::voicing::highestNote + 12;

            prev = v;
        }
    CHECK (inRange, "voice leading stays in range over a long progression");

    for (int d : degrees)
    {
        const auto v = pc::voicing::closeVoicing (chord (d), previousTriad);
        for (int i = 0; i < v.size && previousTriad.size > 0; ++i)
            triadsSmooth = triadsSmooth && pc::voicing::detail::distanceToNearest (v.notes[(size_t) i], previousTriad) <= 4;
        previousTriad = v;
    }
    CHECK (triadsSmooth, "voice leading: triad to triad, every note moves 4 semitones or less");
    CHECK ((toNotes (pc::voicing::closeVoicing (chord (0, Flavour::add9), {})) == Notes { 60, 64, 67, 74 }), "Cadd9 puts D a ninth above C");

    const auto power = pc::voicing::closeVoicing (chord (0, Flavour::power), {});
    CHECK ((toNotes (power) == Notes { 60, 67, 72 }), "power chord doubles the root an octave up");

    const auto wide = pc::voicing::applySpread (chord (0), first, pc::voicing::Spread::wide);
    CHECK ((toNotes (wide) == Notes { 48, 60, 67, 76 }), "wide spread opens the voicing and adds a bass root");
    CHECK ((toNotes (pc::voicing::applySpread (chord (0), first, pc::voicing::Spread::close)) == toNotes (first)), "close spread leaves the voicing alone");
}

void synthTests()
{
    bool allSound = true, allFinite = true, allStop = true, notTooLoud = true;
    for (int s = 0; s < pc::synth::numPresets; ++s)
    {
        pc::synth::PreviewSynth synth;
        synth.prepare (sampleRate);
        synth.setPreset (s);
        for (int n : { 48, 60, 64, 67, 71, 74, 96 })
            synth.noteOn (1, n, 0.8f);
        std::vector<float> l (4800), r (4800);
        float peak = 0.0f;
        for (int b = 0; b < 10; ++b)
        {
            std::fill (l.begin(), l.end(), 0.0f);
            std::fill (r.begin(), r.end(), 0.0f);
            synth.render (l.data(), r.data(), (int) l.size());
            for (size_t i = 0; i < l.size(); ++i)
            {
                peak = std::max (peak, std::abs (l[i]));
                allFinite = allFinite && std::isfinite (l[i]) && std::isfinite (r[i]);
            }
        }
        allSound = allSound && peak > 0.01f;
        notTooLoud = notTooLoud && peak < 1.5f;
        if (peak <= 0.01f || peak >= 1.5f) std::cerr << "  preset " << pc::synth::presets[(size_t) s].name << " peak " << peak << "\n";
        synth.releaseAll();
        for (int b = 0; b < 100; ++b)  // 10 s
            synth.render (l.data(), r.data(), (int) l.size());
        allStop = allStop && synth.numActive() == 0;
    }
    CHECK (allSound, "every preview preset makes sound");
    CHECK (notTooLoud, "no preview preset is far louder than the rest (7-note chord peak under 1.5)");
    CHECK (pc::params::soundNames.size() == pc::synth::numPresets && pc::synth::numPresets == 25, "25 sound presets, all named in the parameter");
    CHECK (allFinite, "preview output is always finite");
    CHECK (allStop, "every preview sound goes quiet after release");

    pc::synth::PreviewSynth synth;
    synth.prepare (sampleRate);
    for (int n = 30; n < 70; ++n) synth.noteOn (1, n, 1.0f);
    CHECK (synth.numActive() == pc::synth::numVoices, "voice stealing caps the synth at 16 voices");
}

// Runs one block, returning the MIDI output
juce::MidiBuffer runBlock (PotatoChordsProcessor& proc, juce::MidiBuffer in, juce::AudioBuffer<float>* audio = nullptr)
{
    juce::AudioBuffer<float> local (2, blockSize);
    auto& buffer = audio != nullptr ? *audio : local;
    proc.processBlock (buffer, in);
    return in;
}

// Net note state after a stream of output: note -> held count
struct Held
{
    std::map<int, int> notes;
    int doubledOns = 0, strayOffs = 0;

    void add (const juce::MidiBuffer& b)
    {
        for (const auto meta : b)
        {
            const auto m = meta.getMessage();
            const int id = m.getChannel() * 128 + m.getNoteNumber();
            if (m.isNoteOn())       { if (notes[id] > 0) ++doubledOns; notes[id] = 1; }
            else if (m.isNoteOff()) { if (notes[id] == 0) ++strayOffs; notes[id] = 0; }
        }
    }

    int sounding() const
    {
        int n = 0;
        for (auto& [id, c] : notes) n += c;
        return n;
    }
};

void setParam (PotatoChordsProcessor& proc, const juce::ParameterID& id, float value)
{
    auto* p = proc.apvts.getParameter (id.getParamID());
    p->setValueNotifyingHost (p->convertTo0to1 (value));
}

Notes noteOnsIn (const juce::MidiBuffer& b, int* samplePos = nullptr)
{
    Notes n;
    for (const auto meta : b)
        if (meta.getMessage().isNoteOn())
        {
            n.push_back (meta.getMessage().getNoteNumber());
            if (samplePos != nullptr) *samplePos = meta.samplePosition;
        }
    std::sort (n.begin(), n.end());
    return n;
}

std::unique_ptr<PotatoChordsProcessor> makeProcessor()
{
    std::unique_ptr<PotatoChordsProcessor> proc { dynamic_cast<PotatoChordsProcessor*> (createPluginFilter()) };
    proc->setPlayConfigDetails (0, 2, sampleRate, blockSize);
    proc->prepareToPlay (sampleRate, blockSize);
    runBlock (*proc, {});  // consume the flush from prepareToPlay
    return proc;
}

void chordPlaybackTests()
{
    auto proc = makeProcessor();
    setParam (*proc, pc::params::key, 0.0f);
    setParam (*proc, pc::params::scale, 0.0f);  // C major
    setParam (*proc, pc::params::chordKeys, 1.0f);
    Held held;

    juce::MidiBuffer in;
    in.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 90), 17);
    int pos = -1;
    auto out = runBlock (*proc, in);
    held.add (out);
    CHECK ((noteOnsIn (out, &pos) == Notes { 60, 64, 67 }), "chord keys: C plays C major");
    CHECK (pos == 17, "chord keys: chord lands on the key's sample offset");
    CHECK (proc->heldDegree() == 0, "GUI sees degree I held");
    CHECK ((toNotes (proc->soundingNotes()) == Notes { 60, 64, 67 }), "GUI sees the sounding notes");

    in.clear();
    in.addEvent (juce::MidiMessage::noteOn (1, 69, (juce::uint8) 90), 5);  // A = vi, while C is still held
    out = runBlock (*proc, in);
    held.add (out);
    CHECK ((noteOnsIn (out) == Notes { 60, 64, 69 }), "chord keys: A plays Am, voice-led from C (C E A)");

    in.clear();
    in.addEvent (juce::MidiMessage::noteOff (1, 60), 3);  // releasing the older key does nothing
    out = runBlock (*proc, in);
    held.add (out);
    CHECK (out.getNumEvents() == 0 && held.sounding() == 3, "releasing an older key keeps the new chord");

    in.clear();
    in.addEvent (juce::MidiMessage::noteOff (1, 69), 40);
    out = runBlock (*proc, in);
    held.add (out);
    CHECK (held.sounding() == 0 && proc->heldDegree() == -1, "releasing the key stops the chord");

    in.clear();
    in.addEvent (juce::MidiMessage::noteOn (1, 61, (juce::uint8) 90), 0);   // black key in the chord octave
    in.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 90), 0);   // outside the chord octave
    out = runBlock (*proc, in);
    held.add (out);
    CHECK ((noteOnsIn (out) == Notes { 48 }), "black keys in the chord octave are ignored, other notes pass through");
    in.clear();
    in.addEvent (juce::MidiMessage::noteOff (1, 48), 0);
    in.addEvent (juce::MidiMessage::noteOff (1, 61), 0);
    held.add (runBlock (*proc, in));
    CHECK (held.sounding() == 0 && held.strayOffs == 0, "pass-through note-off goes out, ignored key's doesn't");

    // The same note arriving twice (two MIDI inputs) is released by its two note-offs
    setParam (*proc, pc::params::chordKeys, 0.0f);
    in.clear();
    in.addEvent (juce::MidiMessage::noteOn (1, 50, (juce::uint8) 90), 0);
    in.addEvent (juce::MidiMessage::noteOn (1, 50, (juce::uint8) 90), 5);
    held.add (runBlock (*proc, in));
    in.clear();
    in.addEvent (juce::MidiMessage::noteOff (1, 50), 0);
    in.addEvent (juce::MidiMessage::noteOff (1, 50), 5);
    held.add (runBlock (*proc, in));
    CHECK (held.sounding() == 0 && ! proc->isNoteSounding (50), "a doubled pass-through note never sticks");
    held.doubledOns = 0;  // the doubled input itself is expected to show as a retrigger
    setParam (*proc, pc::params::chordKeys, 1.0f);

    // Panic releases everything
    in.clear();
    in.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 90), 0);
    held.add (runBlock (*proc, in));
    proc->panic();
    held.add (runBlock (*proc, {}));
    CHECK (held.sounding() == 0 && proc->heldDegree() == -1, "panic sends note-offs for everything");

    // A minor: C key plays the I chord of the key (Am); major V switch makes G play E major
    setParam (*proc, pc::params::key, 9.0f);
    setParam (*proc, pc::params::scale, 1.0f);
    setParam (*proc, pc::params::majorFive, 1.0f);
    in.clear();
    in.addEvent (juce::MidiMessage::noteOn (1, 67, (juce::uint8) 90), 0);
    out = runBlock (*proc, in);
    held.add (out);
    Notes pcs;
    for (int n : noteOnsIn (out)) pcs.push_back (n % 12);
    std::sort (pcs.begin(), pcs.end());
    CHECK ((pcs == Notes { 4, 8, 11 }), "A minor with major V: G key plays E major");

    // Changing flavour while held re-voices the chord without leaving notes behind
    setParam (*proc, pc::params::flavour, 1.0f);
    out = runBlock (*proc, {});
    held.add (out);
    CHECK (noteOnsIn (out).size() == 4 && held.sounding() == 4, "flavour change while held: 7th replaces the triad");

    // Toggling chord keys off while held: the key's note-off still ends the chord
    setParam (*proc, pc::params::chordKeys, 0.0f);
    in.clear();
    in.addEvent (juce::MidiMessage::noteOff (1, 67), 9);
    held.add (runBlock (*proc, in));
    CHECK (held.sounding() == 0, "turning chord keys off while held never strands the chord");

    // Pass-through held while chord keys turns on still gets its note-off
    in.clear();
    in.addEvent (juce::MidiMessage::noteOn (1, 62, (juce::uint8) 90), 0);
    held.add (runBlock (*proc, in));
    setParam (*proc, pc::params::chordKeys, 1.0f);
    in.clear();
    in.addEvent (juce::MidiMessage::noteOff (1, 62), 0);
    held.add (runBlock (*proc, in));
    CHECK (held.sounding() == 0, "turning chord keys on while a note is held never strands it");
    CHECK (held.doubledOns == 0 && held.strayOffs == 0, "no doubled note-ons or stray note-offs");
}

void padTests()
{
    auto proc = makeProcessor();
    setParam (*proc, pc::params::key, 0.0f);
    setParam (*proc, pc::params::scale, 0.0f);
    Held held;

    proc->padDown (3);
    auto out = runBlock (*proc, {});
    held.add (out);
    CHECK ((noteOnsIn (out) == Notes { 65, 69, 72 }), "pad IV plays F major (root position as the first chord)");
    proc->padDown (4);
    held.add (runBlock (*proc, {}));
    CHECK (held.sounding() == 3 && proc->heldDegree() == 4, "pressing another pad switches chord");
    proc->padUp (3);
    held.add (runBlock (*proc, {}));
    CHECK (held.sounding() == 3, "releasing the earlier pad keeps the new chord");
    proc->padUp (4);
    held.add (runBlock (*proc, {}));
    CHECK (held.sounding() == 0, "releasing the pad stops the chord");

    // State load and bypass release a held chord
    proc->padDown (0);
    held.add (runBlock (*proc, {}));
    juce::MemoryBlock state;
    proc->getStateInformation (state);
    proc->setStateInformation (state.getData(), (int) state.getSize());
    held.add (runBlock (*proc, {}));
    CHECK (held.sounding() == 0, "state load sends note-offs for a held chord");

    proc->padDown (1);
    held.add (runBlock (*proc, {}));
    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;
    proc->processBlockBypassed (buffer, midi);
    held.add (midi);
    CHECK (held.sounding() == 0, "bypass sends note-offs for a held chord");
    CHECK (held.doubledOns == 0 && held.strayOffs == 0, "pads: no doubled note-ons or stray note-offs");

    // Host all-notes-off releases everything
    proc->padDown (2);
    held.add (runBlock (*proc, {}));
    juce::MidiBuffer in;
    in.addEvent (juce::MidiMessage::allNotesOff (1), 0);
    held.add (runBlock (*proc, in));
    CHECK (held.sounding() == 0, "all-notes-off from the host releases the chord");
}

struct TestPlayHead : juce::AudioPlayHead
{
    double ppq = 0.0, bpm = 120.0;
    bool playing = true;
    int numerator = 4, denominator = 4;

    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setIsPlaying (playing);
        info.setPpqPosition (ppq);
        info.setBpm (bpm);
        info.setTimeSignature (juce::AudioPlayHead::TimeSignature { numerator, denominator });
        return info;
    }

    void advance (int samples) { ppq += samples * bpm / 60.0 / sampleRate; }
};

// Note-on sample positions (absolute, from `startSample`) and pitches over a run of blocks
struct Timeline
{
    std::vector<std::pair<long, int>> ons, offs;
};

void laneModelTests()
{
    using pc::progression::Slot;
    using pc::progression::Snapshot;

    Snapshot s;
    s.slots[0] = Slot { 0, Flavour::triad, false }.encode();
    s.slots[2] = Slot { 4, Flavour::seventh, false }.encode();
    s.slots[3] = Slot { 5, Flavour::sus4, false }.encode();
    s.splits = 0b10;  // bar 1 is split
    CHECK (s.slotAt (0.0, 4.0) == 0 && s.slotAt (3.99, 4.0) == 0, "bar 0 is one slot");
    CHECK (s.slotAt (4.0, 4.0) == 2 && s.slotAt (6.0, 4.0) == 3, "split bar 1 has two halves");
    CHECK (s.nextBoundary (0.5, 4.0) == 4.0 && s.nextBoundary (4.0, 4.0) == 6.0 && s.nextBoundary (6.0, 4.0) == 8.0, "slot boundaries");
    CHECK (s.chordIn (1) == s.slot (0), "an unsplit bar's second half plays the bar's chord");

    const auto restored = Snapshot::fromString (s.toString());
    CHECK (restored.slots == s.slots && restored.splits == s.splits, "lane round-trips through its text form");
    CHECK ((Snapshot::fromString ("zz qq | q").slots == Snapshot {}.slots), "non-hex lane text loads as empty");
    CHECK (Snapshot::fromString ("7f 0e").slot (0).empty() && Snapshot::fromString ("7f 0e").slot (1).empty(), "bytes without the valid bit load as empty");

    const auto filled = pc::presets::fill (pc::presets::progressions[7], Flavour::triad);
    CHECK (filled.slot (0).degree == 0 && filled.slot (6).degree == 4 && filled.slot (6).majorFive && filled.slot (14).majorFive,
           "preset i-iv-VI-V fills 8 bars with the major V");
    CHECK ((pc::presets::nextChordHints (Scale::major, 4) & 1) != 0, "after V, I is suggested");
    CHECK ((pc::presets::nextChordHints (Scale::minor, 0) & (1 << 5)) != 0, "after i, VI is suggested");
    CHECK (pc::presets::nextChordRank (Scale::major, 0, 3) == 1 && pc::presets::nextChordRank (Scale::major, 0, 4) == 2
           && pc::presets::nextChordRank (Scale::major, 0, 5) == 3 && pc::presets::nextChordRank (Scale::major, 0, 1) == 0,
           "after I the hints rank IV, then V, then vi");
    CHECK (pc::presets::nextChordRank (Scale::major, 4, 0) == 1 && pc::presets::nextChordRank (Scale::major, 4, 5) == 2
           && pc::presets::nextChordRank (Scale::major, 4, 3) == 0, "after V, I comes first, then vi");
    CHECK (pc::presets::nextChordRank (Scale::minor, 0, 3) == 1 && pc::presets::nextChordRank (Scale::minor, -1, 0) == 0,
           "after i, iv comes first; no source chord, no hints");
    bool maskMatches = true;
    for (int d = 0; d < 7; ++d)
        for (auto sc : { Scale::major, Scale::minor })
            for (int c = 0; c < 7; ++c)
                maskMatches = maskMatches && ((pc::presets::nextChordHints (sc, d) >> c) & 1) == (pc::presets::nextChordRank (sc, d, c) > 0 ? 1 : 0);
    CHECK (maskMatches, "the hint mask and the ranking agree");
}

// Plays `blocks` blocks against the play head, collecting chord note-ons and note-offs
Timeline runTransport (PotatoChordsProcessor& proc, TestPlayHead& head, int blocks, Held& held, long& sampleClock)
{
    Timeline t;
    for (int b = 0; b < blocks; ++b)
    {
        auto out = runBlock (proc, {});
        held.add (out);
        for (const auto meta : out)
        {
            const auto m = meta.getMessage();
            if (m.isNoteOn())  t.ons.push_back ({ sampleClock + meta.samplePosition, m.getNoteNumber() });
            if (m.isNoteOff()) t.offs.push_back ({ sampleClock + meta.samplePosition, m.getNoteNumber() });
        }
        if (head.playing) head.advance (blockSize);
        sampleClock += blockSize;
    }
    return t;
}

void laneTests()
{
    auto proc = makeProcessor();
    setParam (*proc, pc::params::key, 0.0f);
    setParam (*proc, pc::params::scale, 0.0f);
    TestPlayHead head;
    head.ppq = 0.0;
    proc->setPlayHead (&head);
    proc->lane.set (pc::presets::fill (pc::presets::progressions[0], Flavour::triad));  // I V vi IV
    Held held;
    long clock = 0;

    // 120 BPM, 4/4: a bar is 2 s = 96000 samples, not a multiple of the 256-sample block
    const long barSamples = 96000;
    const auto t = runTransport (*proc, head, (int) (barSamples * 2 / blockSize) + 4, held, clock);
    long secondBar = -1;
    for (auto& [pos, note] : t.ons)
        if (pos > 0 && secondBar < 0) secondBar = pos;
    CHECK (! t.ons.empty() && t.ons.front().first == 0, "the lane's first chord starts on sample 0 of the song");
    CHECK (secondBar == barSamples, "the second chord lands on the exact sample of bar 2 (not a block boundary)");
    CHECK (proc->playingSlot() == 4, "GUI sees the lane playing bar 3");
    CHECK (held.sounding() == 3, "one triad sounding while the lane plays");

    // Transport stop: every lane note goes off
    head.playing = false;
    runTransport (*proc, head, 1, held, clock);
    CHECK (held.sounding() == 0 && proc->playingSlot() == -1, "transport stop sends note-offs");

    // Restart mid-bar after a locate: the bar's chord starts at once
    head.playing = true;
    head.ppq = 4.0 * 5 + 1.0;  // bar 6, beat 2: chord V (preset repeats: I V vi IV I V vi IV)
    auto r = runTransport (*proc, head, 1, held, clock);
    Notes pcs;
    for (auto& [pos, note] : r.ons) pcs.push_back (note % 12);
    std::sort (pcs.begin(), pcs.end());
    CHECK ((pcs == Notes { 2, 7, 11 }), "locate into bar 6 plays its chord (G) straight away");

    // Host loop: jump back to bar 1 retriggers cleanly
    head.ppq = 0.0;
    r = runTransport (*proc, head, 1, held, clock);
    CHECK (held.sounding() == 3 && held.doubledOns == 0, "loop jump back: old chord off, new chord on, nothing doubled");

    // The lane loops by itself every 8 bars
    head.ppq = 32.0 - 0.01;
    held.add (runBlock (*proc, {}));
    head.advance (blockSize);
    CHECK (proc->playingSlot() == 0, "after bar 8 the lane wraps to bar 1");

    // Tempo change keeps the timing on the new grid
    head.ppq = 3.5;
    head.bpm = 150.0;
    runTransport (*proc, head, 1, held, clock);  // a jump in tempo still lands on the current bar
    const long beforeBar2 = clock;
    const auto tempo = runTransport (*proc, head, (int) (48000 * 0.2 / blockSize) + 2, held, clock);
    // 0.5 quarter - one block of 256 at 150 BPM is left until bar 2
    const double expected = (4.0 - 3.5 - 256 * 150.0 / 60.0 / sampleRate) / (150.0 / 60.0 / sampleRate);
    bool onTime = false;
    for (auto& [pos, note] : tempo.ons)
        onTime = onTime || std::abs ((double) (pos - beforeBar2) - expected) <= 1.0;
    CHECK (onTime, "after a tempo change, the next bar lands on its exact sample");

    // Split bar and 3/4 time
    auto snap = proc->lane.snapshot();
    snap.splits = 0b1;
    snap.slots[1] = pc::progression::Slot { 3, Flavour::triad, false }.encode();
    proc->lane.set (snap);
    head.bpm = 120.0;
    head.numerator = 3;
    head.ppq = 0.0;
    clock = 0;
    runTransport (*proc, head, 1, held, clock);
    const auto split = runTransport (*proc, head, 200, held, clock);
    bool halfBar = false;
    for (auto& [pos, note] : split.ons)
        halfBar = halfBar || pos == 36000;  // 1.5 quarters at 120 BPM
    CHECK (halfBar, "a split bar in 3/4 changes chord exactly half way through the bar");

    // Editing the playing slot changes the chord without a stuck note
    proc->lane.setSlot (0, pc::progression::Slot { 5, Flavour::seventh, false });
    head.ppq = 0.0;
    runTransport (*proc, head, 2, held, clock);
    CHECK (held.sounding() == 4, "editing the playing slot swaps in the new chord");

    // Empty slot: silence
    proc->lane.clear();
    runTransport (*proc, head, 2, held, clock);
    CHECK (held.sounding() == 0, "an empty lane sends note-offs and stays silent");

    // Play progression off
    proc->lane.set (pc::presets::fill (pc::presets::progressions[0], Flavour::triad));
    setParam (*proc, pc::params::playLane, 0.0f);
    runTransport (*proc, head, 2, held, clock);
    CHECK (held.sounding() == 0, "Play progression off: the lane is silent");

    // State: the lane is saved and restored
    juce::MemoryBlock state;
    proc->getStateInformation (state);
    auto other = makeProcessor();
    other->setStateInformation (state.getData(), (int) state.getSize());
    CHECK (other->lane.snapshot().slots == proc->lane.snapshot().slots, "state restores the progression lane");
    CHECK (held.doubledOns == 0 && held.strayOffs == 0, "lane: no doubled note-ons or stray note-offs");
    proc->setPlayHead (nullptr);
}

int countNoteOns (const juce::MidiFile& f, std::vector<double>* times = nullptr)
{
    int n = 0;
    for (int t = 0; t < f.getNumTracks(); ++t)
        for (auto* e : *f.getTrack (t))
            if (e->message.isNoteOn())
            {
                ++n;
                if (times != nullptr) times->push_back (e->message.getTimeStamp());
            }
    return n;
}

void exportTests()
{
    using namespace pc::midiexport;
    const auto lane = pc::presets::fill (pc::presets::progressions[0], Flavour::triad);
    std::vector<double> times;
    const auto file = laneToMidi (lane, 0, Scale::major, pc::voicing::Spread::close, 120.0, 4.0);
    CHECK (countNoteOns (file, &times) == 24, "lane export: 8 triads = 24 notes");
    std::sort (times.begin(), times.end());
    CHECK (times.front() == 0.0 && std::count (times.begin(), times.end(), 4.0 * ticksPerQuarter) == 3, "lane export: bar 2 starts on tick 3840");

    Recorder take;
    take.add ({ 9.5, 0x90, 60, 100 });   // bar 3 (beat 2.5) in 4/4
    take.add ({ 10.5, 0x80, 60, 0 });
    take.add ({ 10.0, 0x90, 64, 100 });  // never released
    take.add ({ 11.0, 0x80, 67, 0 });    // off without an on: dropped
    std::vector<double> takeTimes;
    const auto takeFile = take.toMidiFile (120.0, 4.0);
    CHECK (countNoteOns (takeFile, &takeTimes) == 2 && take.numNotes() == 2, "capture: two notes in the take");
    std::sort (takeTimes.begin(), takeTimes.end());
    CHECK (takeTimes.front() == 1.5 * ticksPerQuarter, "capture: the take starts at its bar, keeping the note's place in the bar");
    int offs = 0;
    for (auto* e : *takeFile.getTrack (0)) offs += e->message.isNoteOff() ? 1 : 0;
    CHECK (offs == 2, "capture: a note still held at the end gets a note-off");

    // The processor hands captured chord notes to the message thread
    auto proc = makeProcessor();
    setParam (*proc, pc::params::key, 0.0f);
    setParam (*proc, pc::params::scale, 0.0f);
    setParam (*proc, pc::params::chordKeys, 1.0f);
    proc->setCapturing (true);
    juce::MidiBuffer in;
    in.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 90), 10);
    runBlock (*proc, in);
    in.clear();
    in.addEvent (juce::MidiMessage::noteOff (1, 60), 20);
    runBlock (*proc, in);
    Recorder live;
    proc->drainCapture ([&] (const CaptureEvent& e) { live.add (e); });
    Notes captured;
    const auto liveFile = live.toMidiFile (120.0, 4.0);
    for (auto* e : *liveFile.getTrack (0))
        if (e->message.isNoteOn()) captured.push_back (e->message.getNoteNumber());
    std::sort (captured.begin(), captured.end());
    CHECK ((captured == Notes { 60, 64, 67 }), "capture records the 3 chord notes one key played in Chord keys mode");
    proc->setCapturing (false);
}

void rhythmTests()
{
    auto proc = makeProcessor();
    setParam (*proc, pc::params::key, 0.0f);
    setParam (*proc, pc::params::scale, 0.0f);
    TestPlayHead head;
    proc->setPlayHead (&head);
    proc->lane.set (pc::presets::fill (pc::presets::progressions[0], Flavour::triad));
    Held held;
    long clock = 0;

    setParam (*proc, pc::params::rhythm, 2.0f);  // offbeat stabs
    auto t = runTransport (*proc, head, (int) (48000 / blockSize) + 2, held, clock);  // one second = 2 beats
    std::vector<long> onTimes, offTimes;
    for (auto& [pos, note] : t.ons) if (std::find (onTimes.begin(), onTimes.end(), pos) == onTimes.end()) onTimes.push_back (pos);
    for (auto& [pos, note] : t.offs) if (std::find (offTimes.begin(), offTimes.end(), pos) == offTimes.end()) offTimes.push_back (pos);
    CHECK ((onTimes == std::vector<long> { 12000, 36000 }), "offbeat stabs land on the 'and' of each beat, to the sample");
    CHECK ((offTimes == std::vector<long> { 18000, 42000 }), "offbeat stabs are a 1/16 note long");
    CHECK (t.ons.size() == 6, "each stab is the whole triad");

    head.playing = false;
    runTransport (*proc, head, 1, held, clock);
    setParam (*proc, pc::params::rhythm, 5.0f);  // arp up
    head.playing = true;
    head.ppq = 0.0;
    clock = 0;
    t = runTransport (*proc, head, 24000 / blockSize, held, clock);  // just under one beat
    Notes arp;
    for (auto& [pos, note] : t.ons) arp.push_back (note);
    CHECK ((arp == Notes { 60, 64, 67, 72 }), "arp up plays the chord notes one by one, then the octave");
    CHECK (held.sounding() <= 1, "arp: one note at a time");

    head.playing = false;
    runTransport (*proc, head, 1, held, clock);
    CHECK (held.sounding() == 0 && held.doubledOns == 0 && held.strayOffs == 0, "rhythm patterns: nothing stuck, doubled or stray");

    const auto file = pc::midiexport::laneToMidi (pc::presets::fill (pc::presets::progressions[0], Flavour::triad), 0, Scale::major,
                                                  pc::voicing::Spread::close, 120.0, 4.0, 3);
    CHECK (countNoteOns (file) == 8 * 8 * 3, "lane export with 8th pulse: 8 stabs per bar of 3 notes");
    proc->setPlayHead (nullptr);
}

void melodyTests()
{
    using pc::melody::Cell;
    pc::melody::Lane tune;
    tune.addNote (4, 2, 0);   // C5 on beat 2, an 1/8 long
    tune.addNote (8, 1, 2);   // E5 on beat 3
    auto snap = tune.snapshot();
    CHECK (snap.cell (4).row == 0 && ! snap.cell (4).hold && snap.cell (5).hold && snap.cell (6).empty(), "a 2-step note is a start and a hold");
    CHECK (snap.lengthFrom (4) == 2 && snap.noteAt (5).second == 4, "note length and start");
    tune.addNote (5, 1, 4);   // writing inside a note replaces it
    snap = tune.snapshot();
    CHECK (snap.cell (4).empty() && snap.cell (5).row == 4, "a new note over an old one replaces it");
    tune.removeNote (5);
    tune.addNote (4, 2, 0);
    snap = tune.snapshot();
    CHECK (pc::melody::Snapshot::fromString (snap.toString()).cells == snap.cells, "melody round-trips through its text form");
    CHECK (pc::melody::Snapshot::fromString ("90").cell (0).empty(), "a hold with nothing before it loads as empty");
    CHECK (pc::melody::rowToNote (0, 0, Scale::major) == 72 && pc::melody::rowToNote (7, 9, Scale::minor) == 81,
           "melody rows start at the key root near C5 and climb the scale");

    auto proc = makeProcessor();
    setParam (*proc, pc::params::key, 0.0f);
    setParam (*proc, pc::params::scale, 0.0f);
    proc->melody.set (snap);
    TestPlayHead head;
    proc->setPlayHead (&head);
    Held held;
    long clock = 0;
    const auto t = runTransport (*proc, head, 48000 / blockSize, held, clock);  // just under two beats
    std::vector<std::pair<long, int>> melodyOns, melodyOffs;
    for (auto& e : t.ons) melodyOns.push_back (e);
    for (auto& e : t.offs) melodyOffs.push_back (e);
    CHECK ((melodyOns == std::vector<std::pair<long, int>> { { 24000, 72 } }), "melody note starts on beat 2, to the sample");
    CHECK ((melodyOffs == std::vector<std::pair<long, int>> { { 36000, 72 } }), "a held note lasts its full length (no retrigger)");

    head.ppq = 1.3;  // locate into the middle of the note
    auto r = runTransport (*proc, head, 1, held, clock);
    CHECK (r.ons.size() == 1 && r.ons[0].second == 72, "locating into a note picks it up straight away");
    head.playing = false;
    runTransport (*proc, head, 1, held, clock);
    CHECK (held.sounding() == 0 && held.strayOffs == 0 && held.doubledOns == 0, "melody: stop releases it, nothing stray or doubled");

    juce::MemoryBlock state;
    proc->getStateInformation (state);
    auto other = makeProcessor();
    other->setStateInformation (state.getData(), (int) state.getSize());
    CHECK (other->melody.snapshot().cells == snap.cells, "state restores the melody");

    const auto file = pc::midiexport::melodyToMidi (snap, 0, Scale::major, 120.0, 4.0);
    std::vector<double> times;
    CHECK (countNoteOns (file, &times) == 2 && times.front() == 960.0, "melody export: two notes, the first on beat 2");
    proc->setPlayHead (nullptr);
}

void songTests()
{
    using namespace pc::song;
    CHECK (fromTemplate (0).totalBars() == 104 && fromTemplate (1).totalBars() == 88 && fromTemplate (2).totalBars() == 176,
           "templates: Pop 104, Pop-EDM 88, EDM club 176 bars");
    const auto pop = fromTemplate (0);
    CHECK (Song::fromString (pop.toString()).toString() == pop.toString(), "a song round-trips through its text form");
    CHECK (Song::fromString ("x 3:7 99:8 1:16 :4 2:").toString() == "1:16", "bad song text keeps only the valid sections");
    CHECK (barsForMinutes (3.5, 128.0, 4.0) == 112 && barsForMinutes (0.1, 120.0, 4.0) == 8, "minutes to bars, rounded to 8");

    bool rulesKept = true, close = true;
    for (int t = 0; t < numTemplates; ++t)
        for (int target : { 48, 64, 80, 96, 112, 128, 160, 200, 240 })
        {
            const auto song = fitToBars (t, target);
            for (int i = 0; i < song.count; ++i)
            {
                const auto& sec = song.sections[(size_t) i];
                rulesKept = rulesKept && sec.bars % 4 == 0 && sec.bars >= 4;
            }
            // The rules move in 4-bar to whole-group steps (a club group is 56 bars), so 24 bars is the worst case
            const int total = song.totalBars();
            if (target >= 64 && target <= 200) close = close && std::abs (total - target) <= 24;
        }
    CHECK (rulesKept, "fitting keeps every section a multiple of 4 bars, none under 4");
    CHECK (close, "fitting lands within 24 bars of the target for 64-200 bars");
    CHECK (fitToBars (0, 104).toString() == pop.toString(), "fitting to the template's own length changes nothing");

    // Section MIDI: Pop intro (8 bars, no melody) then verse (16 bars, loop A, melody)
    auto lane = pc::presets::fill (pc::presets::progressions[0], Flavour::triad);  // I V vi IV twice
    lane.slots[8] = pc::progression::Slot { 3, Flavour::triad, false }.encode();      // bar 5 = IV, so loop B differs
    pc::melody::Lane tune;
    tune.addNote (0, 4, 0);
    pc::midiexport::Feel chords, melodyFeel;
    melodyFeel.channel = 2;
    const auto file = pc::midiexport::songToMidi (pop, 0, 2, lane, tune.snapshot(), chords, melodyFeel);
    CHECK (file.getNumTracks() == 2, "a section file has a chord track and a melody track");
    double lastOff = 0.0, firstMelody = -1.0;
    for (auto* e : *file.getTrack (0)) if (e->message.isNoteOff()) lastOff = std::max (lastOff, e->message.getTimeStamp());
    for (auto* e : *file.getTrack (1)) if (e->message.isNoteOn() && firstMelody < 0) firstMelody = e->message.getTimeStamp();
    CHECK (lastOff <= 24 * 4 * 960 && lastOff > 23 * 4 * 960, "intro + verse fill exactly 24 bars");
    CHECK (firstMelody == 8 * 4 * 960, "the melody waits for the verse (no melody in the intro)");

    // The chorus loops bars 5-8 of the lane: its first chord is IV (F A C), the intro's is I
    const auto chorus = pc::midiexport::songToMidi (pop, 3, 4, lane, tune.snapshot(), chords, melodyFeel);
    Notes firstChord;
    for (auto* e : *chorus.getTrack (0))
        if (e->message.isNoteOn() && e->message.getTimeStamp() < 1.0) firstChord.push_back (e->message.getNoteNumber() % 12);
    std::sort (firstChord.begin(), firstChord.end());
    CHECK ((firstChord == Notes { 0, 5, 9 }), "a chorus loops lane bars 5-8");

    // The song is saved with the plugin's state; state from before Song mode loads the Pop template
    auto proc = makeProcessor();
    PotatoChordsProcessor::SongState custom { fitToBars (1, 64), 1, 6, 128 };
    proc->setSong (custom);
    juce::MemoryBlock state;
    proc->getStateInformation (state);
    auto other = makeProcessor();
    other->setStateInformation (state.getData(), (int) state.getSize());
    const auto restored = other->getSong();
    CHECK (restored.song.toString() == custom.song.toString() && restored.templateIndex == 1 && restored.length == 6 && restored.bpm == 128,
           "the song, template, length and tempo are saved with the plugin");

    auto xml = std::make_unique<juce::XmlElement> (other->apvts.state.getType());
    xml->setAttribute ("version", 1);
    juce::MemoryBlock old;
    juce::AudioProcessor::copyXmlToBinary (*xml, old);
    other->setStateInformation (old.getData(), (int) old.getSize());
    CHECK (other->getSong().song.toString() == fromTemplate (0).toString(), "older state without a song loads the Pop template");
}

// Song play: the host transport walks the sections, each with its own loop half, rhythm and melody
void songPlaybackTests()
{
    using namespace pc::song;
    auto proc = makeProcessor();
    setParam (*proc, pc::params::key, 0.0f);
    setParam (*proc, pc::params::scale, 0.0f);
    auto lane = pc::presets::fill (pc::presets::progressions[0], Flavour::triad);  // I V vi IV twice
    lane.slots[8] = pc::progression::Slot { 3, Flavour::triad, false }.encode();      // bar 5 = IV
    proc->lane.set (lane);
    pc::melody::Lane tune;
    tune.addNote (0, 4, 0);  // C5 on beat 1 of bar 1
    proc->melody.set (tune.snapshot());
    Song song;
    song.add (Kind::intro, 4);
    song.add (Kind::verse, 4);
    song.add (Kind::chorus, 4);
    proc->setSong ({ song, 0, 0 });
    setParam (*proc, pc::params::songPlay, 1.0f);
    TestPlayHead head;
    proc->setPlayHead (&head);

    const long barSamples = 96000;
    struct Ev { long pos; int channel, note; };
    std::vector<Ev> ons;
    Held held;
    long clock = 0;
    int sectionInChorus = -2;
    auto play = [&] (long samples)
    {
        for (long done = 0; done < samples; done += blockSize)
        {
            const auto out = runBlock (*proc, {});
            held.add (out);
            for (const auto meta : out)
                if (meta.getMessage().isNoteOn())
                    ons.push_back ({ clock + meta.samplePosition, meta.getMessage().getChannel(), meta.getMessage().getNoteNumber() });
            if (head.playing) head.advance (blockSize);
            clock += blockSize;
            if (clock > 9 * barSamples && sectionInChorus == -2) sectionInChorus = proc->playingSection();
        }
    };
    play (13 * barSamples);

    auto chordAt = [&] (long pos)
    {
        Notes pcs;
        for (auto& e : ons) if (e.pos == pos && e.channel == 1) pcs.push_back (e.note % 12);
        std::sort (pcs.begin(), pcs.end());
        return pcs;
    };
    std::vector<long> melodyOns;
    for (auto& e : ons) if (e.channel == 2) melodyOns.push_back (e.pos);
    CHECK ((chordAt (0) == Notes { 0, 4, 7 }), "song play: the intro starts on lane bar 1 (I)");
    CHECK ((chordAt (8 * barSamples) == Notes { 0, 5, 9 }), "song play: the chorus jumps to lane bar 5 (IV), to the sample");
    CHECK ((chordAt (12 * barSamples) == Notes { 0, 4, 7 }), "song play: the song loops back to the intro after its last section");
    CHECK ((melodyOns == std::vector<long> { 4 * barSamples, 8 * barSamples }), "song play: no melody in the intro; verse and chorus play it");
    CHECK (sectionInChorus == 2, "the GUI sees the chorus playing");
    CHECK (held.doubledOns == 0 && held.strayOffs == 0, "section changes: nothing doubled or stray");

    // Song play off mid-song: back to the 8-bar loop at once, cleanly (song bar 14 = lane bar 6, chord V)
    setParam (*proc, pc::params::songPlay, 0.0f);
    ons.clear();
    play (blockSize);
    CHECK ((chordAt (13 * barSamples) == Notes { 2, 7, 11 }) && proc->playingSection() == -1,
           "turning Song play off goes straight back to the lane loop");
    head.playing = false;
    play (blockSize);
    CHECK (held.sounding() == 0 && held.doubledOns == 0 && held.strayOffs == 0, "song play: stop releases everything");
    proc->setPlayHead (nullptr);
}

void writerTests()
{
    using namespace pc::writer;
    bool catalogueOk = true;
    for (const auto& l : loops)
    {
        catalogueOk = catalogueOk && l.length >= 2 && l.length <= 4 && l.styles != 0 && l.moods != 0 && l.weight > 0;
        for (int i = 0; i < l.length; ++i) catalogueOk = catalogueOk && l.degrees[(size_t) i] >= 0 && l.degrees[(size_t) i] < 7;
        catalogueOk = catalogueOk && leadsHome (l.scale, l.degrees[(size_t) l.length - 1], l.degrees[0]);
    }
    CHECK (catalogueOk, "writer: every catalogue loop is in the key and leads back to its first chord");

    bool filled = true, chorusIsLoop = true, oneChange = true, darkMinor = true, brightPopMajor = true, deepSevenths = true,
         rhythmOk = true, repeatable = true, homeOk = true;
    std::set<std::string> variety[numStyles];
    for (int st = 0; st < numStyles; ++st)
        for (int mo = 0; mo < numMoods; ++mo)
            for (uint32_t seed = 0; seed < 200; ++seed)
            {
                const auto style = (Style) st;
                const auto mood = (Mood) mo;
                const auto r = write (style, mood, seed);
                const auto& loop = loops[(size_t) r.loop];
                repeatable = repeatable && write (style, mood, seed).lane.toString() == r.lane.toString();
                variety[st].insert (r.lane.toString());
                rhythmOk = rhythmOk && r.rhythm >= 0 && r.rhythm < pc::sequencer::numPatterns;
                filled = filled && r.lane.splits == 0 && r.scale == loop.scale;
                Bars verse, chorus;
                for (int bar = 0; bar < 8; ++bar)
                {
                    const auto slot = r.lane.slot (bar * 2);
                    filled = filled && ! slot.empty() && r.lane.slot (bar * 2 + 1).empty();
                    auto& half = bar < 4 ? verse : chorus;
                    half.degree[(size_t) bar % 4] = slot.degree;
                    if (style == Style::deepHouse) deepSevenths = deepSevenths && slot.flavour == pc::theory::Flavour::seventh;
                }
                const auto fast = expand (loop, 0, false), slow = expand (loop, 0, true);
                chorusIsLoop = chorusIsLoop && (chorus.degree == fast.degree || chorus.degree == slow.degree);
                // The verse is the chorus, a rotation of the loop, or the chorus with one chord swapped
                bool rotation = false;
                for (int rot = 0; rot < loop.length; ++rot)
                    rotation = rotation || verse.degree == expand (loop, rot, false).degree || verse.degree == expand (loop, rot, true).degree;
                int differences = 0;
                for (int k = 0; k < 4; ++k) differences += verse.degree[(size_t) k] != chorus.degree[(size_t) k] ? 1 : 0;
                oneChange = oneChange && (rotation || differences <= 1);
                if (! rotation) homeOk = homeOk && leadsHome (r.scale, verse.degree[3], verse.degree[0]) && ! pingPong (verse);
                if (mood == Mood::dark) darkMinor = darkMinor && r.scale == Scale::minor;
                if (mood == Mood::bright && style == Style::pop) brightPopMajor = brightPopMajor && r.scale == Scale::major;
            }
    CHECK (filled, "writer: all 8 bars get a chord, in the loop's scale");
    CHECK (chorusIsLoop, "writer: bars 5-8 (chorus) are the catalogue loop itself");
    CHECK (oneChange, "writer: bars 1-4 (verse) change at most one thing: a rotation or one swapped chord");
    CHECK (homeOk, "writer: a verse with a swapped chord still leads home and has no ping-pong");
    CHECK (darkMinor && brightPopMajor, "writer: dark moods write in minor, bright pop in major");
    CHECK (deepSevenths, "writer: deep house is coloured with 7ths");
    CHECK (rhythmOk, "writer: the rhythm is a real pattern");
    CHECK (repeatable, "writer: the same seed always gives the same progression");
    bool varied = true;
    for (auto& v : variety) varied = varied && v.size() >= 6;
    CHECK (varied, "writer: the dice give every style plenty of different results");

    // Complete keeps the user's chords and fills the rest of the half from a loop that agrees with them
    pc::progression::Snapshot mine;
    mine.slots[0] = pc::progression::Slot { 0, Flavour::add9, false }.encode();  // I
    mine.slots[2] = pc::progression::Slot { 4, Flavour::add9, false }.encode();  // V
    const auto done = complete (mine, Scale::major, Style::pop, Mood::bright, 1);
    Notes degrees;
    for (int bar = 0; bar < 8; ++bar) degrees.push_back (done.slot (bar * 2).degree);
    CHECK ((degrees == Notes { 0, 4, 5, 3, 5, 3, 0, 4 }), "complete: I V _ _ becomes I-V-vi-IV, and the empty half its rotation vi-IV-I-V");
    CHECK (done.slot (4).flavour == Flavour::add9, "complete: new chords take the colour of the user's chords");

    const auto full = pc::presets::fill (pc::presets::progressions[0], Flavour::triad);
    CHECK (complete (full, Scale::major, Style::pop, Mood::bright, 3).toString() == full.toString(), "complete: a full lane is left alone");

    const auto fresh = complete ({}, Scale::minor, Style::pop, Mood::bright, 5);
    bool allMinorLoop = true;
    for (int bar = 0; bar < 8; ++bar) allMinorLoop = allMinorLoop && ! fresh.slot (bar * 2).empty();
    CHECK (allMinorLoop && fresh.slot (8).degree >= 0, "complete: an empty lane is written in the lane's scale");
    bool inMinor = false;
    for (const auto& l : loops) if (l.scale == Scale::minor && expand (l, 0, false).degree[0] == fresh.slot (8).degree) inMinor = true;
    CHECK (inMinor, "complete: an empty minor lane gets a minor loop");

    // Unusual chords that no loop fits still get a filled bar (from the next-chord hints)
    pc::progression::Snapshot odd;
    odd.slots[0] = pc::progression::Slot { 6, Flavour::triad, false }.encode();
    odd.slots[2] = pc::progression::Slot { 1, Flavour::triad, false }.encode();
    const auto oddDone = complete (odd, Scale::major, Style::pop, Mood::bright, 0);
    bool oddFilled = true;
    for (int bar = 0; bar < 8; ++bar) oddFilled = oddFilled && ! oddDone.slot (bar * 2).empty();
    CHECK (oddFilled && oddDone.slot (0).degree == 6 && oddDone.slot (2).degree == 1, "complete: chords outside the catalogue are kept and the gaps filled");
}

// Keys clicked on the on-screen keyboard play like incoming MIDI
void keyboardClickTests()
{
    auto proc = makeProcessor();
    setParam (*proc, pc::params::key, 0.0f);
    setParam (*proc, pc::params::scale, 0.0f);
    setParam (*proc, pc::params::chordKeys, 0.0f);
    Held held;
    proc->keyDown (64);
    auto out = runBlock (*proc, {});
    held.add (out);
    CHECK ((noteOnsIn (out) == Notes { 64 }) && held.sounding() == 1, "a clicked key plays its note");
    proc->keyUp (64);
    held.add (runBlock (*proc, {}));
    CHECK (held.sounding() == 0 && held.strayOffs == 0, "letting go of the key stops it");

    setParam (*proc, pc::params::chordKeys, 1.0f);
    proc->keyDown (PotatoChordsProcessor::chordKeysLow);  // C in the chord octave
    out = runBlock (*proc, {});
    held.add (out);
    CHECK (noteOnsIn (out).size() == 3, "with Chord keys on, a clicked key in the chord octave plays its chord");
    setParam (*proc, pc::params::chordKeys, 0.0f);
    proc->keyUp (PotatoChordsProcessor::chordKeysLow);
    held.add (runBlock (*proc, {}));
    CHECK (held.sounding() == 0 && held.strayOffs == 0 && held.doubledOns == 0, "the clicked chord stops even if Chord keys was turned off meanwhile");
}

void exportVelocityTests()
{
    pc::melody::Lane tune;
    tune.addNote (0, 2, 0);
    const auto m = pc::midiexport::melodyToMidi (tune.snapshot(), 0, Scale::major, 120.0, 4.0, 2, 0.0, 0, 77);
    const auto l = pc::midiexport::laneToMidi (pc::presets::fill (pc::presets::progressions[0], Flavour::triad), 0, Scale::major,
                                               pc::voicing::Spread::close, 120.0, 4.0, 0, 1, 0.0, 0, 0.0, 77);
    bool melodyOk = false, laneOk = true;
    for (auto* e : *m.getTrack (0)) if (e->message.isNoteOn()) melodyOk = e->message.getVelocity() == 77;
    for (auto* e : *l.getTrack (0)) if (e->message.isNoteOn()) laneOk = laneOk && e->message.getVelocity() == 77;
    CHECK (melodyOk && laneOk, "exported lane and melody use the Velocity setting");
}

void melodyOctaveTests()
{
    CHECK (pc::melody::rowToNote (0, 0, Scale::major, -1) == pc::melody::rowToNote (0, 0, Scale::major) - 12, "melody octave -1 is 12 semitones down");
    CHECK (pc::melody::rowToNote (13, 11, Scale::major, 5) == 127 && pc::melody::rowToNote (0, 0, Scale::major, -9) == 0, "melody notes stay in the MIDI range");

    auto proc = makeProcessor();
    setParam (*proc, pc::params::key, 0.0f);
    setParam (*proc, pc::params::scale, 0.0f);
    setParam (*proc, pc::params::melodyOctave, 0.0f);  // -1
    proc->melody.addNote (0, 4, 0);
    TestPlayHead head;
    proc->setPlayHead (&head);
    Held held;
    long clock = 0;
    const auto t = runTransport (*proc, head, 4, held, clock);
    bool found = false;
    for (auto& [pos, note] : t.ons) found = found || note == 60;  // C4: the melody's C5 down an octave
    CHECK (found, "the melody plays an octave lower with Melody octave -1");

    const auto file = pc::midiexport::melodyToMidi (proc->melody.snapshot(), 0, Scale::major, 120.0, 4.0, 2, 0.0, -1);
    int first = -1;
    for (auto* e : *file.getTrack (0))
        if (e->message.isNoteOn() && first < 0) first = e->message.getNoteNumber();
    CHECK (first == 60, "exported melody follows Melody octave");
}

void repeatTests()
{
    pc::melody::Lane tune;
    tune.addNote (0, 4, 3);
    tune.addNote (62, 2, 5);   // runs to the end of bar 4
    tune.addNote (70, 2, 9);   // already in bar 5: replaced by the copy
    tune.repeatFirstHalf();
    const auto s = tune.snapshot();
    CHECK (s.cell (64).row == 3 && s.lengthFrom (64) == 4 && s.cell (126).row == 5 && s.lengthFrom (126) == 2, "bars 1-4 are copied into bars 5-8");
    CHECK (s.cell (70).empty() && s.cell (0).row == 3, "the copy replaces what was in bars 5-8 and leaves bars 1-4 alone");
}

void swingTests()
{
    auto proc = makeProcessor();
    setParam (*proc, pc::params::key, 0.0f);
    setParam (*proc, pc::params::scale, 0.0f);
    setParam (*proc, pc::params::rhythm, 6.0f);  // 16th pulse
    setParam (*proc, pc::params::swing, 1.0f);
    TestPlayHead head;
    proc->setPlayHead (&head);
    proc->lane.set (pc::presets::fill (pc::presets::progressions[0], Flavour::triad));
    Held held;
    long clock = 0;
    // 120 BPM: a 1/16 step is 6000 samples, so full swing pushes odd steps 2000 samples late
    const auto t = runTransport (*proc, head, 6000 * 6 / blockSize + 8, held, clock);
    std::vector<long> starts;
    for (auto& [pos, note] : t.ons)
        if (starts.empty() || starts.back() != pos) starts.push_back (pos);
    CHECK (starts.size() >= 6 && starts[0] == 0 && starts[1] == 6000 + 2000 && starts[2] == 12000 && starts[3] == 18000 + 2000,
           "swing pushes every odd 1/16 late by a third of a step");

    head.playing = false;
    runTransport (*proc, head, 4, held, clock);
    CHECK (held.sounding() == 0 && held.doubledOns == 0 && held.strayOffs == 0, "swung notes start and stop cleanly");

    CHECK (pc::sequencer::numPatterns == 11 && pc::params::rhythmNames.size() == 11, "eleven rhythms, all named in the parameter");
    CHECK (pc::sequencer::patterns[0].period == 64 && pc::sequencer::patterns[5].numHits == 16, "the first six rhythms keep their places");

    const auto file = pc::midiexport::laneToMidi (pc::presets::fill (pc::presets::progressions[0], Flavour::triad), 0, Scale::major,
                                                  pc::voicing::Spread::close, 120.0, 4.0, 6, 1, 0.0, 0, 1.0);
    std::vector<int> times;
    for (auto* e : *file.getTrack (0))
        if (e->message.isNoteOn() && times.size() < 12 && (times.empty() || times.back() != (int) e->message.getTimeStamp()))
            times.push_back ((int) e->message.getTimeStamp());
    CHECK ((times.size() >= 3 && times[0] == 0 && times[1] == 240 + 80 && times[2] == 480), "exported lane is swung the same way");
}

void feelTests()
{
    auto proc = makeProcessor();
    setParam (*proc, pc::params::key, 0.0f);
    setParam (*proc, pc::params::scale, 0.0f);
    setParam (*proc, pc::params::velocity, 64.0f);
    proc->padDown (0);
    auto out = runBlock (*proc, {});
    bool allAt64 = true;
    for (const auto meta : out)
        if (meta.getMessage().isNoteOn()) allAt64 = allAt64 && meta.getMessage().getVelocity() == 64;
    CHECK (allAt64 && noteOnsIn (out).size() == 3, "pads play at the Velocity setting");
    proc->padUp (0);
    runBlock (*proc, {});

    // Humanise: velocities spread within the limit, notes land late (never early), nothing hangs
    setParam (*proc, pc::params::humanise, 1.0f);
    Held held;
    int minVel = 127, maxVel = 0;
    long latest = 0, clock = 0;
    for (int round = 0; round < 40; ++round)
    {
        proc->padDown (round % 7);
        for (int b = 0; b < 4; ++b, clock += blockSize)
        {
            out = runBlock (*proc, {});
            held.add (out);
            for (const auto meta : out)
                if (meta.getMessage().isNoteOn())
                {
                    minVel = std::min (minVel, (int) meta.getMessage().getVelocity());
                    maxVel = std::max (maxVel, (int) meta.getMessage().getVelocity());
                    latest = std::max (latest, (long) (b * blockSize + meta.samplePosition));
                }
        }
        proc->padUp (round % 7);
        held.add (runBlock (*proc, {}));
        clock += blockSize;
    }
    CHECK (minVel >= 64 - pc::params::humaniseMaxVelocity && maxVel <= 64 + pc::params::humaniseMaxVelocity && maxVel - minVel > 10,
           "humanise varies velocity within its range");
    CHECK (latest > 0 && latest <= (long) (pc::params::humaniseMaxSeconds * sampleRate) + 1, "humanise delays notes by at most 15 ms");
    CHECK (held.sounding() == 0 && held.doubledOns == 0 && held.strayOffs == 0, "humanised chords start and stop cleanly");

    // The melody goes through the same path
    setParam (*proc, pc::params::playLane, 1.0f);
    TestPlayHead head;
    proc->setPlayHead (&head);
    for (int i = 0; i < 64; ++i) proc->melody.addNote (i * 2, 1, i % 14);
    Held tune;
    long tuneClock = 0;
    runTransport (*proc, head, 800, tune, tuneClock);
    head.playing = false;
    runTransport (*proc, head, 2, tune, tuneClock);
    CHECK (tune.sounding() == 0 && tune.doubledOns == 0 && tune.strayOffs == 0, "humanised melody notes start and stop cleanly");
}

void octaveTests()
{
    auto proc = makeProcessor();
    setParam (*proc, pc::params::key, 0.0f);
    setParam (*proc, pc::params::scale, 0.0f);
    Held held;

    setParam (*proc, pc::params::octave, 3.0f);  // +1
    proc->padDown (3);
    auto out = runBlock (*proc, {});
    held.add (out);
    CHECK ((noteOnsIn (out) == Notes { 77, 81, 84 }), "octave +1 lifts pad IV to F5 A5 C6");
    setParam (*proc, pc::params::octave, 0.0f);  // -2, changed while the chord is held
    out = runBlock (*proc, {});
    held.add (out);
    CHECK ((noteOnsIn (out) == Notes { 41, 45, 48 }) && held.sounding() == 3, "changing octave while holding re-voices the chord");
    proc->padUp (3);
    held.add (runBlock (*proc, {}));
    CHECK (held.sounding() == 0 && held.doubledOns == 0 && held.strayOffs == 0, "octave: nothing hangs or doubles");

    pc::voicing::Voiced v;
    v.notes = { 120, 124, 127, 0 };
    v.size = 3;
    const auto up = pc::voicing::shiftOctaves (v, 2);
    CHECK (up.notes[0] <= up.notes[1] && up.notes[1] <= up.notes[2] && up.notes[2] <= 127, "octave shift never leaves the MIDI range");

    const auto file = pc::midiexport::laneToMidi (pc::presets::fill (pc::presets::progressions[0], Flavour::triad), 0, Scale::major,
                                                  pc::voicing::Spread::close, 120.0, 4.0, 0, 1, 0.0, -1);
    Notes first;
    for (auto* e : *file.getTrack (0))
        if (e->message.isNoteOn() && e->message.getTimeStamp() < 1.0) first.push_back (e->message.getNoteNumber());
    std::sort (first.begin(), first.end());
    CHECK ((first == Notes { 48, 52, 55 }), "exported progression follows the octave");
}

void strumTests()
{
    auto proc = makeProcessor();
    setParam (*proc, pc::params::key, 0.0f);
    setParam (*proc, pc::params::scale, 0.0f);
    setParam (*proc, pc::params::strum, 0.5f);  // 20 ms = 960 samples between notes
    Held held;
    std::vector<std::pair<long, int>> ons;
    long clock = 0;
    auto run = [&] (int blocks)
    {
        for (int b = 0; b < blocks; ++b, clock += blockSize)
        {
            const auto out = runBlock (*proc, {});
            held.add (out);
            for (const auto meta : out)
                if (meta.getMessage().isNoteOn()) ons.push_back ({ clock + meta.samplePosition, meta.getMessage().getNoteNumber() });
        }
    };

    proc->padDown (3);
    run (10);
    CHECK ((ons == std::vector<std::pair<long, int>> { { 0, 65 }, { 960, 69 }, { 1920, 72 } }),
           "strum rolls the chord up from the low note, each on its exact sample across blocks");
    proc->padUp (3);
    run (1);
    CHECK (held.sounding() == 0, "releasing a strummed chord stops every note");

    ons.clear();
    proc->padDown (4);
    run (2);  // 512 samples: only the lowest note has started
    proc->padUp (4);
    run (10);
    CHECK (ons.size() == 1 && held.sounding() == 0, "notes of a strum not yet started are dropped when the chord stops");

    proc->padDown (0);
    run (1);
    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;
    proc->processBlockBypassed (buffer, midi);
    held.add (midi);
    run (10);
    CHECK (held.sounding() == 0, "bypass mid-strum leaves nothing hanging");
    CHECK (held.doubledOns == 0 && held.strayOffs == 0, "strum: no doubled note-ons or stray note-offs");

    // The lane, with short hits and the widest strum
    setParam (*proc, pc::params::strum, 1.0f);
    setParam (*proc, pc::params::rhythm, 3.0f);  // 8th pulse
    TestPlayHead head;
    proc->setPlayHead (&head);
    proc->lane.set (pc::presets::fill (pc::presets::progressions[0], Flavour::triad));
    Held laneHeld;
    long laneClock = 0;
    runTransport (*proc, head, 1500, laneHeld, laneClock);
    head.playing = false;
    runTransport (*proc, head, 2, laneHeld, laneClock);
    CHECK (laneHeld.sounding() == 0 && laneHeld.doubledOns == 0 && laneHeld.strayOffs == 0, "strummed lane hits start and stop cleanly");

    // Export keeps the strum: 20 ms at 120 BPM = 0.04 quarters = 38.4 ticks
    const auto file = pc::midiexport::laneToMidi (pc::presets::fill (pc::presets::progressions[0], Flavour::triad), 0, Scale::major,
                                                  pc::voicing::Spread::close, 120.0, 4.0, 0, 1, 0.02);
    std::vector<int> times;
    for (auto* e : *file.getTrack (0))
        if (e->message.isNoteOn() && times.size() < 3) times.push_back ((int) e->message.getTimeStamp());
    CHECK ((times == std::vector<int> { 0, 38, 77 }), "exported progression is strummed the same way");
}

void fxTests()
{
    pc::fx::PreviewFx fx;
    fx.prepare (sampleRate);
    std::vector<float> l (4096, 0.0f), r (4096, 0.0f);
    l[0] = r[0] = 1.0f;
    fx.process (l.data(), r.data(), 4096, 0.0f, 0.0f);
    bool dry = l[0] == 1.0f;
    for (int i = 1; i < 4096; ++i) dry = dry && l[(size_t) i] == 0.0f;
    CHECK (dry, "chorus and reverb at 0 leave the sound untouched");

    fx.reset();
    std::fill (l.begin(), l.end(), 0.0f);
    std::fill (r.begin(), r.end(), 0.0f);
    l[0] = r[0] = 1.0f;
    fx.process (l.data(), r.data(), 4096, 0.0f, 1.0f);
    float tail = 0.0f, peak = 0.0f;
    bool finite = true;
    for (int i = 0; i < 4096; ++i)
    {
        if (i > 2000) tail = std::max (tail, std::abs (l[(size_t) i]));
        peak = std::max (peak, std::abs (l[(size_t) i]));
        finite = finite && std::isfinite (l[(size_t) i]) && std::isfinite (r[(size_t) i]);
    }
    CHECK (finite && tail > 1.0e-4f && peak < 2.0f, "reverb rings on after a click");

    // Chorus on a sine: finite, not silent, and the two sides differ (stereo)
    pc::fx::Chorus chorus;
    chorus.prepare (sampleRate);
    for (int i = 0; i < 4096; ++i) l[(size_t) i] = r[(size_t) i] = (float) std::sin (i * 0.05);
    chorus.process (l.data(), r.data(), 4096, 1.0f);
    float diff = 0.0f, level = 0.0f;
    for (int i = 0; i < 4096; ++i)
    {
        diff = std::max (diff, std::abs (l[(size_t) i] - r[(size_t) i]));
        level = std::max (level, std::abs (l[(size_t) i]));
        finite = finite && std::isfinite (l[(size_t) i]);
    }
    CHECK (finite && level > 0.5f && level < 2.0f && diff > 1.0e-3f, "chorus is stereo and stays in range");
}

void undoTests()
{
    std::unique_ptr<PotatoChordsProcessor> proc { dynamic_cast<PotatoChordsProcessor*> (createPluginFilter()) };
    CHECK (! proc->canUndo() && ! proc->canRedo(), "nothing to undo at start");

    pc::progression::Slot slot;
    slot.degree = 4;
    proc->rememberEdit();
    proc->lane.setSlot (0, slot);
    proc->rememberEdit();
    proc->melody.addNote (0, 4, 2);

    proc->undo();
    CHECK (proc->melody.snapshot().cell (0).empty() && proc->lane.slot (0).degree == 4, "undo removes the last edit only");
    proc->undo();
    CHECK (proc->lane.slot (0).empty() && ! proc->canUndo(), "undo goes back to the start");
    proc->redo();
    proc->redo();
    CHECK (proc->lane.slot (0).degree == 4 && proc->melody.snapshot().cell (0).row == 2 && ! proc->canRedo(), "redo brings both edits back");

    proc->undo();
    proc->rememberEdit();
    proc->lane.setSplit (1, true);
    CHECK (! proc->canRedo(), "a new edit clears redo");

    for (int i = 0; i < 150; ++i) { proc->rememberEdit(); proc->melody.addNote (i % 128, 1, i % 14); }
    int steps = 0;
    while (proc->canUndo()) { proc->undo(); ++steps; }
    CHECK (steps == 100, "undo history is capped at 100 steps");
}

void processorTests()
{
    std::unique_ptr<PotatoChordsProcessor> proc { dynamic_cast<PotatoChordsProcessor*> (createPluginFilter()) };
    proc->setPlayConfigDetails (0, 2, sampleRate, blockSize);
    proc->prepareToPlay (sampleRate, blockSize);

    juce::AudioBuffer<float> buffer (2, blockSize);
    buffer.applyGain (0.0f);
    buffer.setSample (0, 10, 1.0f);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 17);
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 1, 64), 30);
    proc->processBlock (buffer, midi);

    CHECK (buffer.getMagnitude (0, 0, 17) <= 0.0f, "audio is silent before the first note");
    CHECK (buffer.getMagnitude (0, 17, blockSize - 17) > 0.0f, "preview sound plays the note");
    CHECK (midi.getNumEvents() == 2, "incoming MIDI passes through (chord keys off)");
    for (const auto meta : midi)
        if (meta.getMessage().isNoteOn())
            CHECK (meta.samplePosition == 17 && meta.getMessage().getNoteNumber() == 60, "MIDI keeps its sample offset");

    // Preview off: MIDI still goes out, audio is silent
    setParam (*proc, pc::params::preview, 0.0f);
    midi.clear();
    midi.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100), 0);
    proc->processBlock (buffer, midi);
    CHECK (buffer.getMagnitude (0, blockSize) <= 0.0f && midi.getNumEvents() == 1, "preview off: silent, MIDI still out");

    // State round trip into a fresh instance
    proc->apvts.getParameter (pc::params::key.getParamID())->setValueNotifyingHost (
        proc->apvts.getParameter (pc::params::key.getParamID())->convertTo0to1 (7.0f));
    juce::MemoryBlock state;
    proc->getStateInformation (state);
    std::unique_ptr<PotatoChordsProcessor> other { dynamic_cast<PotatoChordsProcessor*> (createPluginFilter()) };
    other->setStateInformation (state.getData(), (int) state.getSize());
    CHECK ((int) other->apvts.getRawParameterValue (pc::params::key.getParamID())->load() == 7, "state restores the key");
    CHECK ((int) other->apvts.state.getProperty ("version") == PotatoChordsProcessor::stateVersion, "state carries its version");
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    theoryTests();
    chordTests();
    voicingTests();
    synthTests();
    chordPlaybackTests();
    padTests();
    laneModelTests();
    laneTests();
    exportTests();
    rhythmTests();
    melodyTests();
    songTests();
    songPlaybackTests();
    writerTests();
    keyboardClickTests();
    exportVelocityTests();
    melodyOctaveTests();
    repeatTests();
    swingTests();
    feelTests();
    octaveTests();
    strumTests();
    fxTests();
    undoTests();
    processorTests();
    std::cout << (failures == 0 ? "All tests passed\n" : "Tests failed\n");
    return failures == 0 ? 0 : 1;
}
