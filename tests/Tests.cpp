// Headless tests for chord theory and the plugin processor.
// Build with -DPOTATO_DEV_TOOLS=ON, run with ctest or ./build/PotatoChordsTests.
#include "PluginProcessor.h"
#include "Theory.h"
#include "Voicing.h"
#include "PreviewSynth.h"
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
    using pc::synth::Sound;
    bool allSound = true, allFinite = true, allStop = true;
    for (auto s : { Sound::piano, Sound::softPiano, Sound::electricPiano, Sound::pad, Sound::pluck, Sound::lead })
    {
        pc::synth::PreviewSynth synth;
        synth.prepare (sampleRate);
        synth.setSound (s);
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
        synth.releaseAll();
        for (int b = 0; b < 100; ++b)  // 10 s
            synth.render (l.data(), r.data(), (int) l.size());
        allStop = allStop && synth.numActive() == 0;
    }
    CHECK (allSound, "every preview sound makes sound");
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
    processorTests();
    std::cout << (failures == 0 ? "All tests passed\n" : "Tests failed\n");
    return failures == 0 ? 0 : 1;
}
