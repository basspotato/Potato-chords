#pragma once
#include "Theory.h"
#include <algorithm>
#include <cstdlib>
#include <utility>

// Turns a chord into MIDI notes: automatic voice leading (nearest inversion to the previous chord) and spread.
// Pure and allocation-free.
namespace pc::voicing
{
    enum class Spread { close, wide };

    constexpr int lowestNote = 48;   // C3 in MIDI terms (Cubase shows it as C2)
    constexpr int highestNote = 79;
    constexpr int centreNote = 64;   // voicings are pulled towards here so a progression doesn't drift away
    constexpr int maxNotes = 8;

    struct Voiced
    {
        std::array<int, maxNotes> notes {};  // ascending MIDI notes
        int size = 0;

        constexpr bool contains (int note) const
        {
            for (int i = 0; i < size; ++i)
                if (notes[(size_t) i] == note) return true;
            return false;
        }
    };

    namespace detail
    {
        inline int distanceToNearest (int note, const Voiced& v)
        {
            int best = 128;
            for (int i = 0; i < v.size; ++i)
                best = std::min (best, std::abs (note - v.notes[(size_t) i]));
            return best;
        }

        inline void sort (Voiced& v)
        {
            for (int i = 1; i < v.size; ++i)
                for (int j = i; j > 0 && v.notes[(size_t) j - 1] > v.notes[(size_t) j]; --j)
                    std::swap (v.notes[(size_t) j - 1], v.notes[(size_t) j]);
        }
    }

    // Close voicing (all notes stacked upwards from the lowest) nearest to `previous`. With no previous
    // chord (size 0) it picks root position near the centre of the range.
    inline Voiced closeVoicing (const theory::Chord& c, const Voiced& previous)
    {
        // add9: voice-lead the triad, then put the 9th a ninth above the root (or a 2nd above it if that's too high)
        if (c.flavour == theory::Flavour::add9)
        {
            auto triad = c;
            triad.flavour = theory::Flavour::triad;
            triad.size = 3;
            auto v = closeVoicing (triad, previous);
            int root = v.notes[0];
            for (int i = 0; i < v.size; ++i)
                if (v.notes[(size_t) i] % 12 == c.root) root = v.notes[(size_t) i];
            v.notes[(size_t) v.size++] = root + 14 <= highestNote ? root + 14 : root + 2;
            detail::sort (v);
            return v;
        }

        // Pitch classes in chord order, without repeats
        std::array<int, 4> pcs {};
        int n = 0;
        for (int i = 0; i < c.size; ++i)
        {
            const int pc = (c.root + c.intervals[(size_t) i]) % 12;
            bool seen = false;
            for (int j = 0; j < n; ++j) seen = seen || pcs[(size_t) j] == pc;
            if (! seen) pcs[(size_t) n++] = pc;
        }

        Voiced best;
        int bestScore = 1 << 30;
        for (int inversion = 0; inversion < n; ++inversion)
        {

            for (int base = lowestNote; base < lowestNote + 24; ++base)
            {
                if (base % 12 != pcs[(size_t) inversion]) continue;

                Voiced v;
                v.notes[0] = base;
                v.size = 1;
                for (int k = 1; k < n; ++k)
                {
                    int note = v.notes[(size_t) k - 1] + 1;
                    while (note % 12 != pcs[(size_t) ((inversion + k) % n)]) ++note;
                    v.notes[(size_t) v.size++] = note;
                }
                if (v.notes[(size_t) v.size - 1] > highestNote) continue;

                int sum = 0;
                for (int i = 0; i < v.size; ++i) sum += v.notes[(size_t) i];
                const int drift = std::abs (sum - centreNote * v.size) / v.size;

                int score = 0;
                if (previous.size == 0)
                {
                    score = (inversion == 0 ? 0 : 1000) + drift;
                }
                else
                {
                    // Total movement, counted both ways so different note counts still compare fairly
                    for (int i = 0; i < v.size; ++i)        score += 2 * detail::distanceToNearest (v.notes[(size_t) i], previous);
                    for (int i = 0; i < previous.size; ++i) score += 2 * detail::distanceToNearest (previous.notes[(size_t) i], v);
                    score += drift;
                }

                if (score < bestScore)
                {
                    bestScore = score;
                    best = v;
                }
            }
        }

        // A power chord is only two notes: double the root an octave up so it sounds full
        if (n == 2 && best.size == 2)
        {
            const int root = best.notes[0] % 12 == c.root ? best.notes[0] : best.notes[1];
            best.notes[2] = root + 12 <= highestNote ? root + 12 : root - 12;
            best.size = 3;
            detail::sort (best);
        }
        return best;
    }

    // Wide: lift the second note up an octave (open voicing) and add the root as a bass note below
    inline Voiced applySpread (const theory::Chord& c, const Voiced& close, Spread s)
    {
        if (s == Spread::close || close.size == 0)
            return close;

        Voiced v = close;
        if (v.size >= 3)
            v.notes[1] += 12;
        int bass = 36 + c.root;
        while (bass + 12 <= v.notes[0] - 12) bass += 12;
        if (! v.contains (bass) && v.size < maxNotes)
            v.notes[(size_t) v.size++] = bass;
        detail::sort (v);
        return v;
    }

    // Moves a voicing whole octaves up or down; a note that would leave the MIDI range stays an octave inside it
    inline Voiced shiftOctaves (Voiced v, int octaves)
    {
        for (int i = 0; i < v.size; ++i)
        {
            int n = v.notes[(size_t) i] + 12 * octaves;
            while (n > 127) n -= 12;
            while (n < 0) n += 12;
            v.notes[(size_t) i] = n;
        }
        detail::sort (v);
        return v;
    }
}
