#pragma once
#include <array>

// Rhythm patterns for the progression lane, on a 1/16-note grid. A pattern repeats every `period` steps
// from the start of each chord slot, and a slot boundary always ends whatever is sounding.
// Pure and allocation-free.
namespace pc::sequencer
{
    constexpr double stepQuarters = 0.25;  // one 1/16 note
    constexpr int maxHits = 16;

    struct Hit
    {
        int start = 0, length = 0;
        int arp = -1;  // -1 = the whole chord, else the n-th chord note (low to high, wrapping)
    };

    struct Pattern
    {
        const char* name;
        int period;
        int numHits;
        std::array<Hit, maxHits> hits;
    };

    inline constexpr std::array<Pattern, 6> patterns {{
        { "Sustain",       64, 1,  {{ { 0, 64 } }} },
        { "Quarters",      16, 4,  {{ { 0, 3 }, { 4, 3 }, { 8, 3 }, { 12, 3 } }} },
        { "Offbeat stabs", 16, 4,  {{ { 2, 1 }, { 6, 1 }, { 10, 1 }, { 14, 1 } }} },
        { "8th pulse",     16, 8,  {{ { 0, 1 }, { 2, 1 }, { 4, 1 }, { 6, 1 }, { 8, 1 }, { 10, 1 }, { 12, 1 }, { 14, 1 } }} },
        { "Syncopated",    16, 6,  {{ { 0, 2 }, { 3, 2 }, { 6, 2 }, { 8, 2 }, { 11, 2 }, { 14, 2 } }} },
        { "Arp up",        16, 16, {{ { 0, 1, 0 }, { 1, 1, 1 }, { 2, 1, 2 }, { 3, 1, 3 }, { 4, 1, 0 }, { 5, 1, 1 }, { 6, 1, 2 }, { 7, 1, 3 },
                                      { 8, 1, 0 }, { 9, 1, 1 }, { 10, 1, 2 }, { 11, 1, 3 }, { 12, 1, 0 }, { 13, 1, 1 }, { 14, 1, 2 }, { 15, 1, 3 } }} },
    }};

    constexpr int numPatterns = (int) patterns.size();

    // The hit that starts on `step` (steps counted from the slot start), or nullptr
    constexpr const Hit* hitStartingAt (const Pattern& p, int step)
    {
        const int s = step % p.period;
        for (int i = 0; i < p.numHits; ++i)
            if (p.hits[(size_t) i].start == s) return &p.hits[(size_t) i];
        return nullptr;
    }

    // The hit sounding during `step`, or nullptr
    constexpr const Hit* hitSoundingAt (const Pattern& p, int step)
    {
        const int s = step % p.period;
        for (int i = 0; i < p.numHits; ++i)
            if (s >= p.hits[(size_t) i].start && s < p.hits[(size_t) i].start + p.hits[(size_t) i].length) return &p.hits[(size_t) i];
        return nullptr;
    }
}
