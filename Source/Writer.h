#pragma once
#include "Presets.h"
#include <array>
#include <cstdint>

// Write a chord progression: pick a known-good loop for the style and mood, then personalise it with at most one
// change, instead of chaining random chords. The lane gets the verse loop in bars 1-4 and the chorus loop in bars
// 5-8 (the halves Song mode plays for verse-type and chorus-type sections). Complete fills only the empty bars.
// Pure and allocation-free; the same seed always gives the same result.
namespace pc::writer
{
    enum class Style { pop, popEdm, edmClub, deepHouse, trance };
    enum class Mood { bright, emotional, dark, epic, chill };
    constexpr int numStyles = 5, numMoods = 5;
    inline constexpr std::array<const char*, numStyles> styleNames { "Pop", "Pop-EDM", "EDM club", "Deep house", "Trance" };
    inline constexpr std::array<const char*, numMoods> moodNames { "Bright", "Emotional", "Dark", "Epic", "Chill" };

    constexpr uint8_t bit (Style s) { return (uint8_t) (1 << (int) s); }
    constexpr uint8_t bit (Mood m) { return (uint8_t) (1 << (int) m); }

    struct Loop
    {
        const char* name;
        theory::Scale scale;
        int length;                  // 2, 3 or 4 chords
        std::array<int, 4> degrees;
        uint8_t majorFiveMask;       // bit i = chord i is the major V (minor loops only)
        uint8_t styles, moods;       // bit masks of Style and Mood
        int weight;                  // how common the loop is in hits
    };

    // The research catalogue's loops that fit the v1 chord set (no borrowed chords), plus more common diatonic ones.
    // Append only: the song ideas refer to loops by index.
    namespace detail
    {
        constexpr auto P = bit (Style::pop), PE = bit (Style::popEdm), C = bit (Style::edmClub), D = bit (Style::deepHouse),
                       T = bit (Style::trance);
        constexpr auto Br = bit (Mood::bright), Em = bit (Mood::emotional), Dk = bit (Mood::dark), Ep = bit (Mood::epic),
                       Ch = bit (Mood::chill);
        using theory::Scale;
        inline constexpr std::array<Loop, 31> loops {{
            { "I-V-vi-IV",    Scale::major, 4, { 0, 4, 5, 3 }, 0, P | PE | C,  Br | Em | Ep, 10 },
            { "vi-IV-I-V",    Scale::major, 4, { 5, 3, 0, 4 }, 0, P | PE | C,  Em | Ch,      9 },
            { "I-vi-IV-V",    Scale::major, 4, { 0, 5, 3, 4 }, 0, P,           Br | Ch,      5 },
            { "IV-V-vi-I",    Scale::major, 4, { 3, 4, 5, 0 }, 0, P | PE,      Br | Em,      5 },
            { "IV-I-V-vi",    Scale::major, 4, { 3, 0, 4, 5 }, 0, P | PE,      Br,           6 },
            { "IV-V-iii-vi",  Scale::major, 4, { 3, 4, 2, 5 }, 0, PE | T,      Em | Ep,      5 },
            { "I-IV-V-I",     Scale::major, 4, { 0, 3, 4, 0 }, 0, P,           Br,           3 },
            { "ii-V-I",       Scale::major, 3, { 1, 4, 0, 0 }, 0, D | P,       Ch,           4 },
            { "I-iii-IV-V",   Scale::major, 4, { 0, 2, 3, 4 }, 0, P,           Br | Ch,      3 },
            { "i-VI-III-VII", Scale::minor, 4, { 0, 5, 2, 6 }, 0, PE | C | T,  Ep | Em,      9 },
            { "i-VI-VII",     Scale::minor, 3, { 0, 5, 6, 0 }, 0, T | C | PE,  Ep | Dk,      6 },
            { "VI-VII-i",     Scale::minor, 3, { 5, 6, 0, 0 }, 0, C | T | PE,  Ep,           5 },
            { "i-VII-VI-VII", Scale::minor, 4, { 0, 6, 5, 6 }, 0, C | PE | P,  Dk | Ep,      6 },
            { "i-iv-VI-V",    Scale::minor, 4, { 0, 3, 5, 4 }, 0b1000, T | C,  Dk | Ep,      5 },
            { "i-VII-VI-V",   Scale::minor, 4, { 0, 6, 5, 4 }, 0b1000, P | C,  Dk,           4 },
            { "i-iv",         Scale::minor, 2, { 0, 3, 0, 0 }, 0, D | C,       Dk | Ch,      6 },
            { "I-IV-vi-V",    Scale::major, 4, { 0, 3, 5, 4 }, 0, P | PE,      Br | Em,      5 },
            { "vi-V-IV-V",    Scale::major, 4, { 5, 4, 3, 4 }, 0, P | PE | C,  Em | Ep,      4 },
            { "I-V-IV",       Scale::major, 3, { 0, 4, 3, 0 }, 0, P | C,       Br,           3 },
            { "vi-IV-V-I",    Scale::major, 4, { 5, 3, 4, 0 }, 0, P | PE | T,  Em | Ch,      4 },
            { "ii-V-I-vi",    Scale::major, 4, { 1, 4, 0, 5 }, 0, D | P,       Ch | Em,      3 },
            { "IV-I-vi-V",    Scale::major, 4, { 3, 0, 5, 4 }, 0, PE | P | C,  Br | Em,      4 },
            { "I-vi-ii-V",    Scale::major, 4, { 0, 5, 1, 4 }, 0, D | P | C,   Br | Ch,      4 },
            { "i-III-VI-VII", Scale::minor, 4, { 0, 2, 5, 6 }, 0, PE | C | T,  Ep | Em,      6 },
            { "i-VI-iv-v",    Scale::minor, 4, { 0, 5, 3, 4 }, 0, T | P,       Dk | Em,      4 },
            { "i-iv-III-VII", Scale::minor, 4, { 0, 3, 2, 6 }, 0, PE | C,      Dk | Ch,      4 },
            { "i-VII-iv",     Scale::minor, 3, { 0, 6, 3, 0 }, 0, D | C,       Dk | Ch,      3 },
            { "i-III-iv",     Scale::minor, 3, { 0, 2, 3, 0 }, 0, D | C,       Ch | Dk,      3 },
            { "i-v-VI-iv",    Scale::minor, 4, { 0, 4, 5, 3 }, 0, P | PE,      Em | Dk,      4 },
            { "i-VI-VII-iv",  Scale::minor, 4, { 0, 5, 6, 3 }, 0, T | C,       Ep | Dk,      4 },
            { "i-VII",        Scale::minor, 2, { 0, 6, 0, 0 }, 0, D | C | T,   Dk | Ch,      5 },
        }};
    }
    inline constexpr const auto& loops = detail::loops;
    constexpr int numLoops = (int) detail::loops.size();

    // Small deterministic generator (xorshift32), so a seed always replays the same result
    struct Dice
    {
        uint32_t state;
        explicit constexpr Dice (uint32_t seed) : state (seed * 2654435761u + 0x9e3779b9u) { if (state == 0) state = 1; next(); }
        constexpr uint32_t next()
        {
            state ^= state << 13; state ^= state >> 17; state ^= state << 5;
            return state;
        }
        constexpr int below (int n) { return n > 0 ? (int) (next() % (uint32_t) n) : 0; }
    };

    // Four bars of chords: degree per bar plus the major V flag
    struct Bars
    {
        std::array<int, 4> degree { -1, -1, -1, -1 };
        std::array<bool, 4> majorFive {};
    };

    // A loop rotated to start on chord `rotation`, spread over 4 bars. 3-chord loops hold their last chord for two
    // bars; 2-chord loops change every bar, or every two bars when `slowVamp` is set.
    constexpr Bars expand (const Loop& loop, int rotation, bool slowVamp = false)
    {
        Bars b;
        const int n = loop.length;
        for (int bar = 0; bar < 4; ++bar)
        {
            int i = bar;
            if (n == 3) i = bar < 3 ? bar : 2;
            else if (n == 2) i = slowVamp ? bar / 2 : bar % 2;
            const int src = (i + rotation) % n;
            b.degree[(size_t) bar] = loop.degrees[(size_t) src];
            b.majorFive[(size_t) bar] = ((loop.majorFiveMask >> src) & 1) != 0;
        }
        return b;
    }

    // The chord that shares two notes with `degree` (I for vi, IV for ii, ...): the "near neighbour" swap
    constexpr int neighbour (theory::Scale s, int degree)
    {
        constexpr int major[7] { 5, 3, 4, 1, 2, 0, 4 };
        constexpr int minor[7] { 2, 3, 0, 5, 6, 3, 4 };
        if (degree < 0 || degree > 6) return degree;
        return s == theory::Scale::major ? major[degree] : minor[degree];
    }

    // The checks every written loop must pass: no chord repeated in the next bar unless the loop itself holds it,
    // no A-B-A-B ping-pong made by a swap, and the last chord leads back to the first.
    constexpr bool leadsHome (theory::Scale s, int last, int first)
    {
        return last == first || last == 0 || (presets::nextChordHints (s, last) & (1 << first)) != 0
            || last == 4 || last == 6 || last == 3;  // home goes anywhere; V, VII and IV always lead back
    }

    constexpr bool pingPong (const Bars& b)
    {
        return b.degree[0] == b.degree[2] && b.degree[1] == b.degree[3] && b.degree[0] != b.degree[1];
    }

    struct Result
    {
        progression::Snapshot lane;
        theory::Scale scale = theory::Scale::major;
        int rhythm = 0;                    // index into sequencer::patterns
        int loop = 0;                      // which catalogue loop the chorus uses
    };

    // Rhythm and chord colour by style (four choices each, picked by the dice)
    constexpr int rhythmFor (Style s, int pick)
    {
        constexpr int table[numStyles][4] { { 1, 3, 7, 5 }, { 4, 2, 3, 8 }, { 2, 8, 6, 3 }, { 8, 2, 4, 7 }, { 6, 5, 9, 10 } };
        return table[(int) s][pick & 3];
    }

    constexpr theory::Flavour flavourFor (Style s, int pick, bool lastChord)
    {
        using theory::Flavour;
        switch (s)
        {
            case Style::pop:       return pick == 3 ? Flavour::add9 : Flavour::triad;
            case Style::popEdm:    return pick == 0 ? Flavour::seventh : pick == 1 ? Flavour::add9 : pick == 2 ? Flavour::sus2 : Flavour::triad;
            case Style::edmClub:   return pick == 3 ? Flavour::power : Flavour::triad;
            case Style::deepHouse: return pick == 3 ? Flavour::add9 : Flavour::seventh;
            case Style::trance:    return lastChord && pick % 2 == 0 ? Flavour::sus2 : pick == 3 ? Flavour::add9 : Flavour::triad;
        }
        return Flavour::triad;
    }

    // Picks a catalogue loop for the style and mood, weighted toward the common ones. When nothing matches both,
    // the mood is relaxed, then the style.
    constexpr int pickLoop (Style style, Mood mood, Dice& dice, int onlyScale = -1)
    {
        for (int pass = 0; pass < 3; ++pass)
        {
            int total = 0;
            for (const auto& l : loops)
                if ((onlyScale < 0 || (int) l.scale == onlyScale) && (pass >= 2 || (l.styles & bit (style)) != 0) && (pass >= 1 || (l.moods & bit (mood)) != 0)) total += l.weight;
            if (total == 0) continue;
            int r = dice.below (total);
            for (int i = 0; i < numLoops; ++i)
            {
                const auto& l = loops[(size_t) i];
                if (! ((onlyScale < 0 || (int) l.scale == onlyScale) && (pass >= 2 || (l.styles & bit (style)) != 0) && (pass >= 1 || (l.moods & bit (mood)) != 0))) continue;
                if (r < l.weight) return i;
                r -= l.weight;
            }
        }
        return 0;
    }

    inline void putBars (progression::Snapshot& s, int firstBar, const Bars& b, Style style, int colourPick)
    {
        for (int bar = 0; bar < 4; ++bar)
        {
            progression::Slot slot;
            slot.degree = b.degree[(size_t) bar];
            slot.majorFive = b.majorFive[(size_t) bar];
            slot.flavour = flavourFor (style, colourPick, bar == 3);
            s.slots[(size_t) (firstBar + bar) * 2] = slot.encode();
            s.slots[(size_t) (firstBar + bar) * 2 + 1] = 0;
        }
    }

    // The verse: the chorus loop rotated to start on another chord, or with one chord swapped for its neighbour,
    // or left as it is. At most one change, and only if the result still passes the checks.
    constexpr Bars verseFrom (const Loop& loop, const Bars& chorus, bool slowVamp, Dice& dice)
    {
        const int roll = dice.below (4);
        if (roll < 2 && loop.length > 1)
        {
            const int rotation = 1 + dice.below (loop.length - 1);
            return expand (loop, rotation, slowVamp);
        }
        if (roll == 2)
        {
            const int first = dice.below (4);
            for (int k = 0; k < 4; ++k)
            {
                const int bar = (first + k) % 4;
                if (bar == 0) continue;  // keep the verse's first chord so it still sets the key
                Bars b = chorus;
                b.degree[(size_t) bar] = neighbour (loop.scale, chorus.degree[(size_t) bar]);
                b.majorFive[(size_t) bar] = false;
                const bool repeats = b.degree[(size_t) bar] == b.degree[(size_t) bar - 1]
                                  || (bar < 3 && b.degree[(size_t) bar] == b.degree[(size_t) bar + 1]);
                if (! repeats && ! pingPong (b) && leadsHome (loop.scale, b.degree[3], b.degree[0])) return b;
            }
        }
        return chorus;
    }

    // `onlyScale` keeps the loop in one scale (Complete on an empty lane keeps the lane's scale)
    inline Result write (Style style, Mood mood, uint32_t seed, int onlyScale = -1)
    {
        Dice dice (seed);
        Result r;
        r.loop = pickLoop (style, mood, dice, onlyScale);
        const auto& loop = loops[(size_t) r.loop];
        r.scale = loop.scale;
        const bool slowVamp = style == Style::deepHouse && dice.below (2) == 0;
        const auto chorus = expand (loop, 0, slowVamp);
        const auto verse = verseFrom (loop, chorus, slowVamp, dice);
        const int colourPick = dice.below (4);
        r.rhythm = rhythmFor (style, dice.below (4));
        putBars (r.lane, 0, verse, style, colourPick);
        putBars (r.lane, 4, chorus, style, colourPick);
        r.lane.splits = 0;
        return r;
    }

    // A bar counts as empty when none of its slots holds a chord
    inline bool barEmpty (const progression::Snapshot& s, int bar)
    {
        return s.slot (bar * 2).empty() && (! s.isSplit (bar) || s.slot (bar * 2 + 1).empty());
    }

    inline int barDegree (const progression::Snapshot& s, int bar)
    {
        return s.slot (bar * 2).empty() ? s.slot (bar * 2 + 1).degree : s.slot (bar * 2).degree;
    }

    // Fills only the empty bars and leaves every chord the user placed. A half with chords gets the catalogue loop
    // (in the lane's scale) that agrees with all of them, the style's loops first; an empty half gets the other half
    // started two bars later (I-V-vi-IV becomes vi-IV-I-V); an empty lane is written from scratch.
    inline progression::Snapshot complete (const progression::Snapshot& current, theory::Scale scale, Style style,
                                              Mood mood, uint32_t seed)
    {
        bool anything = false;
        for (int bar = 0; bar < progression::numBars; ++bar) anything = anything || ! barEmpty (current, bar);
        if (! anything)
        {
            return write (style, mood, seed, (int) scale).lane;
        }

        Dice dice (seed);
        auto out = current;
        // The colour of the user's first chord carries over to the new ones
        theory::Flavour flavour = theory::Flavour::triad;
        for (int i = progression::numSlots - 1; i >= 0; --i)
            if (! current.slot (i).empty()) flavour = current.slot (i).flavour;

        auto fill = [&] (int bar, int degree, bool majorFive)
        {
            progression::Slot slot;
            slot.degree = degree;
            slot.flavour = flavour;
            slot.majorFive = majorFive && scale == theory::Scale::minor && degree == 4;
            out.slots[(size_t) bar * 2] = slot.encode();
        };

        bool halfFilled[2] {};
        for (int half = 0; half < 2; ++half)
        {
            const int first = half * 4;
            int known = 0;
            for (int bar = first; bar < first + 4; ++bar) known += barEmpty (current, bar) ? 0 : 1;
            if (known == 0 || known == 4) { halfFilled[half] = known == 4; continue; }

            // Best loop: agrees with every chord already there; style and mood matches, then weight, break ties
            int bestScore = -1, bestLoop = -1, bestRotation = 0;
            bool bestSlow = false;
            for (int i = 0; i < numLoops; ++i)
            {
                const auto& l = loops[(size_t) i];
                if (l.scale != scale) continue;
                for (int slow = 0; slow < (l.length == 2 ? 2 : 1); ++slow)
                    for (int rot = 0; rot < l.length; ++rot)
                    {
                        const auto b = expand (l, rot, slow == 1);
                        bool agrees = true;
                        for (int k = 0; k < 4 && agrees; ++k)
                            if (! barEmpty (current, first + k)) agrees = barDegree (current, first + k) == b.degree[(size_t) k];
                        if (! agrees) continue;
                        const int score = ((l.styles & bit (style)) != 0 ? 400 : 0) + ((l.moods & bit (mood)) != 0 ? 200 : 0)
                                        + l.weight * 10 + dice.below (10);
                        if (score > bestScore) { bestScore = score; bestLoop = i; bestRotation = rot; bestSlow = slow == 1; }
                    }
            }
            if (bestLoop >= 0)
            {
                const auto b = expand (loops[(size_t) bestLoop], bestRotation, bestSlow);
                for (int k = 0; k < 4; ++k)
                    if (barEmpty (current, first + k)) fill (first + k, b.degree[(size_t) k], b.majorFive[(size_t) k]);
            }
            else
            {
                // No known loop fits the user's chords: follow the next-chord hints from the chord before
                for (int k = 0; k < 4; ++k)
                {
                    const int bar = first + k;
                    if (! barEmpty (out, bar)) continue;
                    const int prev = bar > 0 && ! barEmpty (out, bar - 1) ? barDegree (out, bar - 1) : -1;
                    const int nextBar = bar + 1 < progression::numBars && ! barEmpty (out, bar + 1) ? barDegree (out, bar + 1) : -1;
                    int degree = 0;
                    if (prev >= 0)
                    {
                        for (const int d : presets::nextChordRanked (scale, prev))
                            if (d >= 0 && d != nextBar) { degree = d; break; }
                    }
                    else if (nextBar == 0) degree = 4;
                    fill (bar, degree, false);
                }
            }
            halfFilled[half] = true;
        }

        // An empty half: the other half, started two bars later
        for (int half = 0; half < 2; ++half)
        {
            if (halfFilled[half]) continue;
            const int other = 1 - half;
            if (! halfFilled[other]) continue;
            for (int k = 0; k < 4; ++k)
            {
                const int from = other * 4 + (k + 2) % 4;
                out.slots[(size_t) (half * 4 + k) * 2] = out.slots[(size_t) from * 2];
                out.slots[(size_t) (half * 4 + k) * 2 + 1] = out.slots[(size_t) from * 2 + 1];
                if (out.isSplit (from)) out.splits |= (uint8_t) (1 << (half * 4 + k));
            }
        }
        return out;
    }
}
