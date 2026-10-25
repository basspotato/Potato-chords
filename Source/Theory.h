#pragma once
#include <array>
#include <cstddef>

// Scales and diatonic chords. Pure, allocation-free and JUCE-free so it is easy to test.
namespace pc::theory
{
    enum class Scale { major, minor };
    enum class Flavour { triad, seventh, sus2, sus4, add9, power };
    enum class Triad { major, minor, diminished };

    constexpr int numDegrees = 7;
    constexpr int numFlavours = 6;

    constexpr std::array<int, 7> intervals (Scale s)
    {
        return s == Scale::major ? std::array<int, 7> { 0, 2, 4, 5, 7, 9, 11 }
                                 : std::array<int, 7> { 0, 2, 3, 5, 7, 8, 10 };
    }

    // Semitones above the key root of scale step `step` (any integer; wraps into higher/lower octaves)
    constexpr int stepToSemitone (Scale s, int step)
    {
        const int octave = (step >= 0 ? step : step - 6) / 7;
        return intervals (s)[(size_t) (step - octave * 7)] + 12 * octave;
    }

    // Root-position diatonic triad on `degree` (0 = I) as MIDI notes, with the key root at `rootNote`
    constexpr std::array<int, 3> triad (int rootNote, Scale s, int degree)
    {
        return { rootNote + stepToSemitone (s, degree),
                 rootNote + stepToSemitone (s, degree + 2),
                 rootNote + stepToSemitone (s, degree + 4) };
    }

    struct Chord
    {
        int root = 0;                     // pitch class 0-11
        Triad triad = Triad::major;       // quality of the underlying triad
        Flavour flavour = Flavour::triad;
        int seventh = 0;                  // semitones of the 7th above the root (10 or 11), used for naming
        std::array<int, 4> intervals {};  // semitones above the root, low to high
        int size = 0;
    };

    // The chord on `degree` (0-6) of the key. In minor, `majorFive` turns v into the major V (E in A minor),
    // the only chord from outside the key. sus2, sus4, add9 and Power always use a perfect fifth.
    constexpr Chord diatonicChord (int key, Scale s, int degree, Flavour f, bool majorFive)
    {
        const int base = stepToSemitone (s, degree);
        int third = stepToSemitone (s, degree + 2) - base;
        const int fifth = stepToSemitone (s, degree + 4) - base;
        int seventh = stepToSemitone (s, degree + 6) - base;
        if (s == Scale::minor && degree == 4 && majorFive)
        {
            third = 4;
            seventh = 10;
        }

        Chord c;
        c.root = (key + base) % 12;
        c.triad = third == 4 ? Triad::major : (fifth == 6 ? Triad::diminished : Triad::minor);
        c.flavour = f;
        c.seventh = seventh;
        switch (f)
        {
            case Flavour::triad:   c.intervals = { 0, third, fifth, 0 };       c.size = 3; break;
            case Flavour::seventh: c.intervals = { 0, third, fifth, seventh }; c.size = 4; break;
            case Flavour::sus2:    c.intervals = { 0, 2, 7, 0 };               c.size = 3; break;
            case Flavour::sus4:    c.intervals = { 0, 5, 7, 0 };               c.size = 3; break;
            case Flavour::add9:    c.intervals = { 0, third, fifth, 14 };      c.size = 4; break;
            case Flavour::power:   c.intervals = { 0, 7, 0, 0 };               c.size = 2; break;
        }
        return c;
    }

    // Fixed-size text so labels stay allocation-free
    struct Label
    {
        char text[16] {};
        int length = 0;

        constexpr void add (const char* s) { while (*s != 0 && length < 15) text[length++] = *s++; text[length] = 0; }
    };

    // Flat keys (by their major key signature) spell black notes as flats: F major shows Bb, not A#
    constexpr bool usesFlats (int key, Scale s)
    {
        const int major = s == Scale::major ? key : (key + 3) % 12;
        return major == 5 || major == 10 || major == 3 || major == 8 || major == 1 || major == 6;
    }

    constexpr const char* noteName (int pitchClass, bool flats)
    {
        constexpr const char* sharpNames[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        constexpr const char* flatNames[]  { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
        return (flats ? flatNames : sharpNames)[((pitchClass % 12) + 12) % 12];
    }

    // Chord name, e.g. C, Am, Bdim, G7, Cmaj7, Bm7b5, Fsus4, Am(add9), E5
    constexpr Label chordName (const Chord& c, bool flats)
    {
        Label l;
        l.add (noteName (c.root, flats));
        const bool minorish = c.triad != Triad::major;
        switch (c.flavour)
        {
            case Flavour::triad:
                l.add (c.triad == Triad::minor ? "m" : c.triad == Triad::diminished ? "\xc2\xb0" : "");
                break;
            case Flavour::seventh:
                if (c.triad == Triad::diminished)  l.add ("m7b5");
                else if (c.triad == Triad::minor)  l.add ("m7");
                else                               l.add (c.seventh == 11 ? "maj7" : "7");
                break;
            case Flavour::sus2:  l.add ("sus2"); break;
            case Flavour::sus4:  l.add ("sus4"); break;
            case Flavour::add9:  l.add (! minorish ? "add9" : c.triad == Triad::minor ? "m(add9)" : "\xc2\xb0(add9)"); break;
            case Flavour::power: l.add ("5"); break;
        }
        return l;
    }

    // Roman numeral: uppercase = major, lowercase = minor/diminished, e.g. I, vi, viidim, V7, iim7b5 (drawn with the degree and slashed-o signs)
    constexpr Label romanNumeral (const Chord& c, int degree)
    {
        constexpr const char* upper[] { "I", "II", "III", "IV", "V", "VI", "VII" };
        constexpr const char* lower[] { "i", "ii", "iii", "iv", "v", "vi", "vii" };
        Label l;
        const bool majorCase = c.triad == Triad::major;
        l.add ((majorCase ? upper : lower)[degree % 7]);
        switch (c.flavour)
        {
            case Flavour::triad:   if (c.triad == Triad::diminished) l.add ("\xc2\xb0"); break;
            case Flavour::seventh:
                if (c.triad == Triad::diminished) l.add ("\xc3\xb8" "7");
                else l.add (majorCase && c.seventh == 11 ? "maj7" : "7");
                break;
            case Flavour::sus2:  l.add ("sus2"); break;
            case Flavour::sus4:  l.add ("sus4"); break;
            case Flavour::add9:  l.add (c.triad == Triad::diminished ? "\xc2\xb0" "add9" : "add9"); break;
            case Flavour::power: l.add ("5"); break;
        }
        return l;
    }

    // One-key chords: the white keys of one octave pick degrees I-vii (C = I ... B = vii), -1 for black keys
    constexpr int whiteKeyDegree (int pitchClass)
    {
        constexpr int map[12] { 0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6 };
        return map[((pitchClass % 12) + 12) % 12];
    }
}
