#pragma once
#include "Writer.h"
#include "MelodyWriter.h"
#include "Song.h"

// Song ideas: one-click starting points that set the key, chords (verse and chorus loops from the catalogue),
// chord colour, rhythm, a written melody, the song template and the tempo. Pure; loading the same idea always
// gives the same result.
namespace pc::ideas
{
    struct Half { int loop, rotation; bool slowVamp; };

    struct Idea
    {
        const char* name;
        const char* about;
        int key;                       // 0 = C ... 11 = B
        Half verse, chorus;            // bars 1-4 and bars 5-8
        theory::Flavour flavour;
        int rhythm;                    // index into sequencer::patterns
        melodywriter::Shape shape;
        melodywriter::Busy busy;
        uint32_t melodySeed;
        int songTemplate;              // index into song::templates
        int bpm;
    };

    namespace detail
    {
        using F = theory::Flavour;
        using S = melodywriter::Shape;
        using B = melodywriter::Busy;
        // Loop indexes refer to writer::loops (0 I-V-vi-IV ... 15 i-iv); verse and chorus share a scale
        inline constexpr std::array<Idea, 12> ideas {{
            { "Radio anthem",      "Big four-chord pop, verse turned round",  7,  { 0, 2, false },  { 0, 0, false },  F::triad,   3, S::singAlong, B::medium, 11, 0, 118 },
            { "Heartbreak ballad", "Slow and open, add9 colour",              0,  { 1, 0, false },  { 4, 0, false },  F::add9,    0, S::singAlong, B::sparse, 4,  0, 76 },
            { "Retro 50s",         "Doo-wop verse, classic cadence chorus",   5,  { 2, 0, false },  { 6, 0, false },  F::triad,   1, S::singAlong, B::busy,   7,  0, 96 },
            { "Future bass",       "Royal road in 7ths, syncopated chords",   2,  { 5, 2, false },  { 5, 0, false },  F::seventh, 4, S::riff,      B::medium, 21, 1, 150 },
            { "Tropical house",    "Sad four-chord, offbeat stabs, whoop",    9,  { 1, 2, false },  { 1, 0, false },  F::add9,    2, S::whoop,     B::medium, 3,  1, 110 },
            { "Epic trance",       "Heroic minor, gated 16ths, arp lead",     9,  { 13, 0, false }, { 9, 0, false },  F::triad,   6, S::arp,       B::busy,   5,  2, 138 },
            { "Festival drop",     "Power chords, house stabs, riff lead",    5,  { 10, 0, false }, { 12, 0, false }, F::power,   8, S::riff,      B::busy,   9,  2, 128 },
            { "Deep house night",  "Two-chord minor 7 vamp",                  7,  { 15, 0, true },  { 15, 0, false }, F::seventh, 8, S::riff,      B::sparse, 2,  2, 122 },
            { "Dark pop",          "Andalusian descent, moody sing-along",    4,  { 12, 0, false }, { 14, 0, false }, F::triad,   3, S::singAlong, B::medium, 6,  0, 100 },
            { "Melodic dubstep",   "Epic minor with an aeolian landing",      1,  { 11, 0, false }, { 9, 0, false },  F::add9,    0, S::singAlong, B::sparse, 8,  1, 140 },
            { "City pop chill",    "Jazzy ii-V-I in 7ths, half notes",        3,  { 8, 0, false },  { 7, 0, false },  F::seventh, 7, S::singAlong, B::medium, 13, 0, 92 },
            { "Millennial whoop",  "Climb-home chorus with the wa-oh hook",   4,  { 0, 0, false },  { 3, 0, false },  F::triad,   3, S::whoop,     B::medium, 1,  0, 120 },
        }};
    }
    inline constexpr const auto& ideas = detail::ideas;
    constexpr int numIdeas = (int) detail::ideas.size();

    struct Loaded
    {
        progression::Snapshot lane;
        melody::Snapshot tune;
        theory::Scale scale = theory::Scale::major;
    };

    inline Loaded load (const Idea& idea)
    {
        Loaded out;
        out.scale = writer::loops[(size_t) idea.chorus.loop].scale;
        for (int half = 0; half < 2; ++half)
        {
            const auto& h = half == 0 ? idea.verse : idea.chorus;
            const auto bars = writer::expand (writer::loops[(size_t) h.loop], h.rotation, h.slowVamp);
            for (int k = 0; k < 4; ++k)
            {
                progression::Slot slot;
                slot.degree = bars.degree[(size_t) k];
                slot.majorFive = bars.majorFive[(size_t) k];
                slot.flavour = idea.flavour;
                out.lane.slots[(size_t) (half * 4 + k) * 2] = slot.encode();
            }
        }
        out.tune = melodywriter::write (out.lane, out.scale, idea.shape, idea.busy, idea.melodySeed);
        return out;
    }
}
